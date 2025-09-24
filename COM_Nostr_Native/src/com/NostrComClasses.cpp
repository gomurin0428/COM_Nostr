#include "pch.h"

#include "src/com/NostrComClasses.h"

using namespace com::nostr::native;

HRESULT CNostrClient::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrClient::FinalRelease() noexcept
{
}

STDMETHODIMP CNostrClient::Initialize(IDispatch* options)
{
    UNREFERENCED_PARAMETER(options);
    return E_NOTIMPL;
}

STDMETHODIMP CNostrClient::SetSigner(INostrSigner* signer)
{
    UNREFERENCED_PARAMETER(signer);
    return E_NOTIMPL;
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
