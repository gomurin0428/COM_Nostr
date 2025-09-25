#include "pch.h"

#include "src/com/NostrComClasses.h"

#include "runtime/ComCallbackDispatcher.h"
#include "runtime/NativeClientResources.h"
#include "runtime/NativeHttpClient.h"
#include "runtime/WinHttpWebSocket.h"
#include "NostrJsonSerializer.h"
#include "ComValueHelpers.h"
#include "NostrHResults.h"

#include <atlcomcli.h>

#include <cmath>
#include <limits>
#include <memory>
#include <utility>

using namespace com::nostr::native;

namespace
{
    constexpr double kMillisecondsPerSecond = 1000.0;
    constexpr double kMaxTimeoutSeconds = static_cast<double>((std::numeric_limits<long long>::max)()) / kMillisecondsPerSecond;
}

HRESULT CNostrClient::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrClient::FinalRelease() noexcept
{
    ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
    disposed_ = true;
    initialized_ = false;
    signer_.Release();
    guard.Unlock();

    if (dispatcher_)
    {
        dispatcher_->Shutdown();
        dispatcher_.reset();
    }

    serializer_.reset();
    resources_.reset();
    options_ = ClientRuntimeOptions{};
    webSocketFactoryProgId_.clear();
}

STDMETHODIMP CNostrClient::Initialize(IDispatch* options)
{
    ClientRuntimeOptions normalizedOptions;
    std::wstring webSocketProgId;
    HRESULT hr = NormalizeClientOptions(options, normalizedOptions, webSocketProgId);
    if (FAILED(hr))
    {
        return hr;
    }

    auto serializer = std::make_shared<NostrJsonSerializer>();
    if (!serializer)
    {
        return E_OUTOFMEMORY;
    }

    const ClientRuntimeOptions httpOptions = normalizedOptions;

    NativeClientResources::HttpClientFactory httpFactory = [httpOptions]() -> std::unique_ptr<NativeHttpClient>
    {
        return std::make_unique<NativeHttpClient>(httpOptions);
    };

    NativeClientResources::WebSocketFactory webSocketFactory = []() -> std::unique_ptr<INativeWebSocket>
    {
        return std::make_unique<WinHttpWebSocket>();
    };

    auto resources = std::make_unique<NativeClientResources>(normalizedOptions,
                                                             std::move(httpFactory),
                                                             std::move(webSocketFactory),
                                                             serializer);
    if (!resources)
    {
        return E_OUTOFMEMORY;
    }

    auto dispatcher = std::make_unique<ComCallbackDispatcher>();
    if (!dispatcher)
    {
        return E_OUTOFMEMORY;
    }

    hr = dispatcher->Start();
    if (FAILED(hr))
    {
        return hr;
    }

    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            guard.Unlock();
            dispatcher->Shutdown();
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (initialized_)
        {
            guard.Unlock();
            dispatcher->Shutdown();
            return hresults::E_NOSTR_ALREADY_INITIALIZED;
        }

        options_ = normalizedOptions;
        webSocketFactoryProgId_ = std::move(webSocketProgId);
        serializer_ = serializer;
        resources_ = std::move(resources);
        dispatcher_ = std::move(dispatcher);
        initialized_ = true;
    }

    return S_OK;
}

STDMETHODIMP CNostrClient::SetSigner(INostrSigner* signer)
{
    if (!signer)
    {
        return hresults::PointerRequired();
    }

    ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
    if (disposed_)
    {
        return hresults::E_NOSTR_OBJECT_DISPOSED;
    }

    if (!initialized_)
    {
        return hresults::E_NOSTR_NOT_INITIALIZED;
    }

    CComPtr<INostrSigner> signerPtr = signer;
    signer_ = std::move(signerPtr);
    return S_OK;
}

HRESULT CNostrClient::NormalizeClientOptions(IDispatch* optionsDispatch,
                                             ClientRuntimeOptions& optionsOut,
                                             std::wstring& webSocketProgId) const
{
    optionsOut = ClientRuntimeOptions{};
    webSocketProgId.clear();

    if (!optionsDispatch)
    {
        return S_OK;
    }

    ATL::CComDispatchDriver driver(optionsDispatch);
    if (!driver)
    {
        return DISP_E_TYPEMISMATCH;
    }

    CComVariant value;
    HRESULT hr = driver.GetProperty(1, &value);
    if (SUCCEEDED(hr) && VariantHasValue(value))
    {
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return DISP_E_TYPEMISMATCH;
        }

        const std::wstring progId = BstrToWString(value.bstrVal);
        if (!progId.empty())
        {
            webSocketProgId = progId;
        }
    }
    else if (FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND)
    {
        return hr;
    }

    value.Clear();
    hr = driver.GetProperty(2, &value);
    if (SUCCEEDED(hr) && VariantHasValue(value))
    {
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return DISP_E_TYPEMISMATCH;
        }

        const std::wstring userAgent = BstrToWString(value.bstrVal);
        if (!userAgent.empty())
        {
            optionsOut.SetUserAgent(userAgent);
        }
    }
    else if (FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND)
    {
        return hr;
    }

    auto applyTimeout = [&driver, &value, &optionsOut](int dispId, void (ClientRuntimeOptions::*setter)(ClientRuntimeOptions::Duration) noexcept) -> HRESULT
    {
        value.Clear();
        HRESULT localHr = driver.GetProperty(dispId, &value);
        if (SUCCEEDED(localHr) && VariantHasValue(value))
        {
            const auto seconds = VariantToDouble(value);
            if (!seconds.has_value())
            {
                return hresults::InvalidArgument();
            }

            if (*seconds <= 0.0 || *seconds > kMaxTimeoutSeconds)
            {
                return hresults::InvalidArgument();
            }

            const auto milliseconds = static_cast<long long>(std::llround(*seconds * kMillisecondsPerSecond));
            if (milliseconds <= 0)
            {
                return hresults::InvalidArgument();
            }

            (optionsOut.*setter)(ClientRuntimeOptions::Duration(milliseconds));
        }
        else if (FAILED(localHr) && localHr != DISP_E_MEMBERNOTFOUND)
        {
            return localHr;
        }

        return S_OK;
    };

    hr = applyTimeout(3, &ClientRuntimeOptions::SetConnectTimeout);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = applyTimeout(4, &ClientRuntimeOptions::SetSendTimeout);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = applyTimeout(5, &ClientRuntimeOptions::SetReceiveTimeout);
    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

STDMETHODIMP CNostrClient::ConnectRelay(IDispatch* descriptor, INostrAuthCallback* authCallback, INostrRelaySession** session)
{
    UNREFERENCED_PARAMETER(descriptor);
    UNREFERENCED_PARAMETER(authCallback);
    UNREFERENCED_PARAMETER(session);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::DisconnectRelay(BSTR relayUrl)
{
    UNREFERENCED_PARAMETER(relayUrl);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::HasRelay(BSTR relayUrl, VARIANT_BOOL* hasRelay)
{
    UNREFERENCED_PARAMETER(relayUrl);
    UNREFERENCED_PARAMETER(hasRelay);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::OpenSubscription(BSTR relayUrl, SAFEARRAY* filters, INostrEventCallback* callback, IDispatch* options, INostrSubscription** subscription)
{
    UNREFERENCED_PARAMETER(relayUrl);
    UNREFERENCED_PARAMETER(filters);
    UNREFERENCED_PARAMETER(callback);
    UNREFERENCED_PARAMETER(options);
    UNREFERENCED_PARAMETER(subscription);
    return E_NOTIMPL;
}
STDMETHODIMP CNostrClient::PublishEvent(BSTR relayUrl, IDispatch* eventPayload)
{
    UNREFERENCED_PARAMETER(relayUrl);
    UNREFERENCED_PARAMETER(eventPayload);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::RespondAuth(BSTR relayUrl, IDispatch* authEvent)
{
    UNREFERENCED_PARAMETER(relayUrl);
    UNREFERENCED_PARAMETER(authEvent);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::RefreshRelayInfo(BSTR relayUrl)
{
    UNREFERENCED_PARAMETER(relayUrl);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::ListRelays(SAFEARRAY** relayUrls)
{
    UNREFERENCED_PARAMETER(relayUrls);
    return E_NOTIMPL;
}
HRESULT CNostrRelaySession::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrRelaySession::FinalRelease() noexcept
{
}

STDMETHODIMP CNostrRelaySession::get_Url(BSTR* value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::get_State(RelaySessionState* value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::get_LastOkResult(IDispatch** value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}
STDMETHODIMP CNostrRelaySession::get_SupportedNips(SAFEARRAY** value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::get_WriteEnabled(VARIANT_BOOL* value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::get_ReadEnabled(VARIANT_BOOL* value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::Reconnect()
{
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::Close()
{
    return E_NOTIMPL;
}

STDMETHODIMP CNostrRelaySession::GetDescriptor(IDispatch** value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}
STDMETHODIMP CNostrRelaySession::UpdatePolicy(IDispatch* descriptor)
{
    UNREFERENCED_PARAMETER(descriptor);
    return E_NOTIMPL;
}

HRESULT CNostrSubscription::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrSubscription::FinalRelease() noexcept
{
}

STDMETHODIMP CNostrSubscription::get_Id(BSTR* value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrSubscription::get_Status(SubscriptionStatus* value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}
STDMETHODIMP CNostrSubscription::get_Filters(SAFEARRAY** value)
{
    UNREFERENCED_PARAMETER(value);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrSubscription::UpdateFilters(SAFEARRAY* filters)
{
    UNREFERENCED_PARAMETER(filters);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrSubscription::Close()
{
    return E_NOTIMPL;
}
HRESULT CNostrSigner::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrSigner::FinalRelease() noexcept
{
}
