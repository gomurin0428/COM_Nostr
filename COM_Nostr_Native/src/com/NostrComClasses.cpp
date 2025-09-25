#include "pch.h"

#include "src/com/NostrComClasses.h"

#include "runtime/ComCallbackDispatcher.h"
#include "runtime/NativeClientResources.h"
#include "runtime/NativeHttpClient.h"
#include "runtime/WinHttpWebSocket.h"
#include "NostrJsonSerializer.h"
#include "ComValueHelpers.h"
#include "NostrHResults.h"
#include "src/dto/NostrDtoComObjects.h"

#include <atlcomcli.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

using namespace com::nostr::native;

namespace
{
    using json = nlohmann::json;

    constexpr double kMillisecondsPerSecond = 1000.0;
    constexpr double kMaxTimeoutSeconds = static_cast<double>((std::numeric_limits<long long>::max)()) / kMillisecondsPerSecond;

    struct RelayDescriptorData
    {
        std::wstring url;
        bool readEnabled = true;
        bool writeEnabled = true;
        bool preferred = false;
        std::wstring metadataJson;
    };

    std::wstring TrimWhitespace(const std::wstring& value)
    {
        const wchar_t* whitespace = L" \t\r\n";
        const auto start = value.find_first_not_of(whitespace);
        if (start == std::wstring::npos)
        {
            return std::wstring();
        }

        const auto end = value.find_last_not_of(whitespace);
        return value.substr(start, end - start + 1);
    }

    bool StartsWithInsensitive(const std::wstring& value, const wchar_t* prefix)
    {
        if (!prefix)
        {
            return false;
        }

        const size_t prefixLength = wcslen(prefix);
        if (value.size() < prefixLength)
        {
            return false;
        }

        return _wcsnicmp(value.c_str(), prefix, prefixLength) == 0;
    }

    HRESULT NormalizeRelayUrl(const std::wstring& rawUrl, std::wstring& normalized)
    {
        normalized.clear();
        const std::wstring trimmed = TrimWhitespace(rawUrl);
        if (trimmed.empty())
        {
            return hresults::InvalidArgument();
        }

        bool secure = false;
        std::wstring parseInput;
        if (StartsWithInsensitive(trimmed, L"wss://"))
        {
            secure = true;
            parseInput = L"https://" + trimmed.substr(6);
        }
        else if (StartsWithInsensitive(trimmed, L"ws://"))
        {
            parseInput = L"http://" + trimmed.substr(5);
        }
        else
        {
            return hresults::InvalidArgument();
        }

        URL_COMPONENTS components;
        ZeroMemory(&components, sizeof(components));
        components.dwStructSize = sizeof(components);
        components.dwSchemeLength = static_cast<DWORD>(-1);
        components.dwHostNameLength = static_cast<DWORD>(-1);
        components.dwUrlPathLength = static_cast<DWORD>(-1);
        components.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(parseInput.c_str(), static_cast<DWORD>(parseInput.length()), 0, &components))
        {
            return hresults::FromWin32(GetLastError());
        }

        std::wstring host;
        if (components.lpszHostName && components.dwHostNameLength > 0)
        {
            host.assign(components.lpszHostName, components.dwHostNameLength);
        }

        if (host.empty())
        {
            return hresults::InvalidArgument();
        }

        std::transform(host.begin(), host.end(), host.begin(), ::towlower);

        INTERNET_PORT port = components.nPort;
        if (port == 0)
        {
            port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        }

        std::wstring path;
        if (components.lpszUrlPath && components.dwUrlPathLength > 0)
        {
            path.assign(components.lpszUrlPath, components.dwUrlPathLength);
        }
        if (components.lpszExtraInfo && components.dwExtraInfoLength > 0)
        {
            path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
        }
        if (path.empty())
        {
            path = L"/";
        }

        normalized = secure ? L"wss://" : L"ws://";
        normalized += host;

        const bool defaultPort = (!secure && port == INTERNET_DEFAULT_HTTP_PORT) ||
                                 (secure && port == INTERNET_DEFAULT_HTTPS_PORT);
        if (!defaultPort)
        {
            normalized.append(L":");
            normalized.append(std::to_wstring(port));
        }

        normalized += path;
        return S_OK;
    }

    HRESULT ReadRelayDescriptor(IDispatch* dispatch, RelayDescriptorData& data)
    {
        if (!dispatch)
        {
            return hresults::PointerRequired();
        }

        ATL::CComQIPtr<IRelayDescriptor> relay(dispatch);
        if (!relay)
        {
            return DISP_E_TYPEMISMATCH;
        }

        CComBSTR url;
        HRESULT hr = relay->get_Url(&url);
        if (FAILED(hr))
        {
            return hr;
        }

        if (!url || SysStringLen(url) == 0)
        {
            return hresults::InvalidArgument();
        }

        data.url = BstrToWString(url);

        VARIANT_BOOL flag = VARIANT_TRUE;
        if (SUCCEEDED(relay->get_ReadEnabled(&flag)))
        {
            data.readEnabled = flag == VARIANT_TRUE;
        }

        flag = VARIANT_TRUE;
        if (SUCCEEDED(relay->get_WriteEnabled(&flag)))
        {
            data.writeEnabled = flag == VARIANT_TRUE;
        }

        flag = VARIANT_FALSE;
        if (SUCCEEDED(relay->get_Preferred(&flag)))
        {
            data.preferred = flag == VARIANT_TRUE;
        }

        CComVariant metadata;
        if (SUCCEEDED(relay->get_Metadata(&metadata)) && VariantHasValue(metadata))
        {
            CComVariant copy(metadata);
            if (SUCCEEDED(copy.ChangeType(VT_BSTR)) && copy.bstrVal)
            {
                data.metadataJson = BstrToWString(copy.bstrVal);
            }
        }

        return S_OK;
    }

    DWORD ResolveReceiveTimeoutMilliseconds(const RelaySessionData& state)
    {
        const auto& receiveTimeout = state.runtimeOptions.ReceiveTimeout();
        if (receiveTimeout && receiveTimeout->count() > 0)
        {
            const long long milliseconds = receiveTimeout->count();
            if (milliseconds <= 0)
            {
                return 1000;
            }

            if (milliseconds > static_cast<long long>((std::numeric_limits<DWORD>::max)()))
            {
                return (std::numeric_limits<DWORD>::max)();
            }

            return static_cast<DWORD>(milliseconds);
        }

        return 1000;
    }

    std::string ExtractMessageType(const std::vector<uint8_t>& payload)
    {
        try
        {
            const json parsed = json::parse(payload.begin(), payload.end());
            if (parsed.is_array() && !parsed.empty() && parsed[0].is_string())
            {
                return parsed[0].get<std::string>();
            }
        }
        catch (const std::exception&)
        {
        }

        return std::string();
    }

    std::vector<std::shared_ptr<RelaySessionData::SubscriptionEntry>> CollectSubscriptions(const std::shared_ptr<RelaySessionData>& state,
                                                                                           const std::optional<std::wstring>& subscriptionId)
    {
        std::vector<std::shared_ptr<RelaySessionData::SubscriptionEntry>> result;
        if (!state)
        {
            return result;
        }

        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        if (subscriptionId && !subscriptionId->empty())
        {
            const auto it = state->subscriptions.find(*subscriptionId);
            if (it != state->subscriptions.end() && it->second)
            {
                result.push_back(it->second);
            }
        }
        else
        {
            result.reserve(state->subscriptions.size());
            for (const auto& [id, entry] : state->subscriptions)
            {
                if (entry)
                {
                    result.push_back(entry);
                }
            }
        }

        return result;
    }

    void UpdateSubscriptionStatus(const std::shared_ptr<RelaySessionData>& state,
                                  const std::wstring& subscriptionId,
                                  SubscriptionStatus status)
    {
        if (!state)
        {
            return;
        }

        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        const auto it = state->subscriptions.find(subscriptionId);
        if (it != state->subscriptions.end() && it->second)
        {
            it->second->status = status;
        }
    }

    void DispatchNotice(const std::shared_ptr<RelaySessionData>& state,
                        ComCallbackDispatcher* dispatcher,
                        const std::wstring& message)
    {
        if (!state || !dispatcher)
        {
            return;
        }

        const auto targets = CollectSubscriptions(state, std::nullopt);
        if (targets.empty())
        {
            return;
        }

        for (const auto& target : targets)
        {
            if (!target || !target->callback)
            {
                continue;
            }

            INostrEventCallback* callback = target->callback;
            if (!callback)
            {
                continue;
            }

            callback->AddRef();
            const std::wstring relayUrl = state->url;
            const std::wstring noticeText = message;
            dispatcher->Post([callback, relayUrl, noticeText]() mutable
            {
                CComVariant args[2];
                args[0] = CComVariant(noticeText.c_str());
                args[1] = CComVariant(relayUrl.c_str());

                DISPPARAMS params{};
                params.cArgs = 2;
                params.rgvarg = args;

                callback->Invoke(3, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
                callback->Release();
            });
        }
    }

    void DispatchEndOfStoredEvents(const std::shared_ptr<RelaySessionData>& state,
                                   ComCallbackDispatcher* dispatcher,
                                   const std::wstring& subscriptionId)
    {
        if (!state || !dispatcher)
        {
            return;
        }

        const auto targets = CollectSubscriptions(state, subscriptionId);
        if (targets.empty())
        {
            return;
        }

        UpdateSubscriptionStatus(state, subscriptionId, SubscriptionStatus_Active);

        for (const auto& target : targets)
        {
            if (!target || !target->callback)
            {
                continue;
            }

            INostrEventCallback* callback = target->callback;
            if (!callback)
            {
                continue;
            }

            callback->AddRef();
            const std::wstring relayUrl = state->url;
            const std::wstring subIdCopy = subscriptionId;
            dispatcher->Post([callback, relayUrl, subIdCopy]() mutable
            {
                CComVariant args[2];
                args[0] = CComVariant(subIdCopy.c_str());
                args[1] = CComVariant(relayUrl.c_str());

                DISPPARAMS params{};
                params.cArgs = 2;
                params.rgvarg = args;

                callback->Invoke(2, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
                callback->Release();
            });
        }
    }

    void DispatchClosed(const std::shared_ptr<RelaySessionData>& state,
                        ComCallbackDispatcher* dispatcher,
                        const std::wstring& subscriptionId,
                        const std::wstring& reason)
    {
        if (!state || !dispatcher)
        {
            return;
        }

        const auto targets = CollectSubscriptions(state, subscriptionId);
        if (targets.empty())
        {
            return;
        }

        UpdateSubscriptionStatus(state, subscriptionId, SubscriptionStatus_Closed);

        for (const auto& target : targets)
        {
            if (!target || !target->callback)
            {
                continue;
            }

            INostrEventCallback* callback = target->callback;
            if (!callback)
            {
                continue;
            }

            callback->AddRef();
            const std::wstring relayUrl = state->url;
            const std::wstring subIdCopy = subscriptionId;
            const std::wstring reasonCopy = reason;
            dispatcher->Post([callback, relayUrl, subIdCopy, reasonCopy]() mutable
            {
                CComVariant args[3];
                args[0] = CComVariant(reasonCopy.c_str());
                args[1] = CComVariant(subIdCopy.c_str());
                args[2] = CComVariant(relayUrl.c_str());

                DISPPARAMS params{};
                params.cArgs = 3;
                params.rgvarg = args;

                callback->Invoke(4, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
                callback->Release();
            });
        }
    }

    void DispatchEventMessage(const std::shared_ptr<RelaySessionData>& state,
                              ComCallbackDispatcher* dispatcher,
                              const std::shared_ptr<NostrJsonSerializer>& serializer,
                              const NostrJsonSerializer::EventMessage& message)
    {
        if (!state || !dispatcher || !serializer)
        {
            return;
        }

        const auto targets = CollectSubscriptions(state, message.subscriptionId);
        if (targets.empty())
        {
            return;
        }

        ATL::CComPtr<INostrEvent> eventDispatch;
        if (FAILED(serializer->PopulateEventDispatch(message.event, &eventDispatch)))
        {
            return;
        }

        INostrEvent* sharedEvent = eventDispatch;
        if (!sharedEvent)
        {
            return;
        }

        for (const auto& target : targets)
        {
            if (!target || !target->callback)
            {
                continue;
            }

            INostrEventCallback* callback = target->callback;
            if (!callback)
            {
                continue;
            }

            callback->AddRef();
            sharedEvent->AddRef();
            const std::wstring relayUrl = state->url;
            dispatcher->Post([callback, relayUrl, sharedEvent]() mutable
            {
                CComVariant args[2];
                args[0].vt = VT_DISPATCH;
                args[0].pdispVal = sharedEvent;
                args[1] = CComVariant(relayUrl.c_str());

                DISPPARAMS params{};
                params.cArgs = 2;
                params.rgvarg = args;

                callback->Invoke(1, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);

                args[0].pdispVal = nullptr;
                args[0].vt = VT_EMPTY;

                sharedEvent->Release();
                callback->Release();
            });
        }
    }

    void DispatchAuthRequired(const std::shared_ptr<RelaySessionData>& state,
                              ComCallbackDispatcher* dispatcher,
                              const NostrJsonSerializer::AuthChallengeMessage& message)
    {
        if (!state || !dispatcher)
        {
            return;
        }

        ATL::CComPtr<INostrAuthCallback> authCallback = state->authCallback;
        if (!authCallback)
        {
            return;
        }

        ATL::CComObject<CAuthChallenge>* challenge = nullptr;
        if (FAILED(ATL::CComObject<CAuthChallenge>::CreateInstance(&challenge)))
        {
            return;
        }

        challenge->AddRef();
        challenge->put_RelayUrl(CComBSTR(state->url.c_str()));
        challenge->put_Challenge(CComBSTR(message.challenge.c_str()));
        if (message.expiresAt)
        {
            CComVariant expires(*message.expiresAt);
            challenge->put_ExpiresAt(expires);
        }

        ATL::CComPtr<IDispatch> dispatchChallenge;
        challenge->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&dispatchChallenge));
        challenge->Release();

        dispatcher->Post([authCallback, dispatchChallenge]() mutable
        {
            CComVariant arg(dispatchChallenge);

            DISPPARAMS params{};
            params.cArgs = 1;
            params.rgvarg = &arg;

            authCallback->Invoke(1, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
        });
    }

    void DispatchOkMessage(const std::shared_ptr<RelaySessionData>& state,
                           ComCallbackDispatcher* dispatcher,
                           const NostrJsonSerializer::OkMessage& message)
    {
        if (!state)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> guard(state->lastOkMutex);
            state->hasLastOk = true;
            state->lastOkSuccess = message.success;
            state->lastOkEventId = message.eventId;
            state->lastOkMessageText = message.message;
        }

        if (!message.success && dispatcher)
        {
            DispatchNotice(state, dispatcher, message.message);
        }
    }

    void HandleIncomingMessage(const std::shared_ptr<RelaySessionData>& state,
                               ComCallbackDispatcher* dispatcher,
                               const std::shared_ptr<NostrJsonSerializer>& serializer,
                               const NativeWebSocketMessage& message)
    {
        if (!state || !dispatcher || !serializer)
        {
            return;
        }

        if (message.bufferType != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
        {
            return;
        }

        const std::string type = ExtractMessageType(message.payload);
        if (type.empty())
        {
            return;
        }

        try
        {
            if (type == "EVENT")
            {
                const auto eventMessage = serializer->DeserializeEvent(message.payload);
                DispatchEventMessage(state, dispatcher, serializer, eventMessage);
            }
            else if (type == "EOSE")
            {
                const auto eose = serializer->DeserializeEndOfStoredEvents(message.payload);
                DispatchEndOfStoredEvents(state, dispatcher, eose.subscriptionId);
            }
            else if (type == "NOTICE")
            {
                const auto notice = serializer->DeserializeNotice(message.payload);
                DispatchNotice(state, dispatcher, notice.message);
            }
            else if (type == "AUTH")
            {
                const auto auth = serializer->DeserializeAuthChallenge(message.payload);
                DispatchAuthRequired(state, dispatcher, auth);
            }
            else if (type == "OK")
            {
                const auto ok = serializer->DeserializeOk(message.payload);
                DispatchOkMessage(state, dispatcher, ok);
            }
            else if (type == "CLOSED")
            {
                const auto closed = serializer->DeserializeClosed(message.payload);
                DispatchClosed(state, dispatcher, closed.subscriptionId, closed.reason);
            }
        }
        catch (const std::exception&)
        {
        }
    }

    void RunReceiveLoop(const std::shared_ptr<RelaySessionData>& state)
    {
        if (!state)
        {
            return;
        }

        const DWORD timeout = ResolveReceiveTimeoutMilliseconds(*state);

        while (!state->stopRequested.load(std::memory_order_acquire))
        {
            if (!state->webSocket)
            {
                break;
            }

            NativeWebSocketMessage message;
            const HRESULT hr = state->webSocket->Receive(timeout, message);
            if (hr == hresults::Timeout())
            {
                continue;
            }

            if (FAILED(hr))
            {
                if (!state->stopRequested.load(std::memory_order_acquire))
                {
                    state->state.store(RelaySessionState_Faulted, std::memory_order_release);
                }
                break;
            }

            HandleIncomingMessage(state, state->dispatcher, state->serializer, message);
        }

        state->stopRequested.store(true, std::memory_order_release);
    }

    void StartReceiveLoop(const std::shared_ptr<RelaySessionData>& state)
    {
        if (!state)
        {
            return;
        }

        if (!state->dispatcher || !state->serializer)
        {
            return;
        }

        state->stopRequested.store(false, std::memory_order_release);
        state->receiveThread = std::thread([weakState = std::weak_ptr<RelaySessionData>(state)]()
        {
            try
            {
                if (auto locked = weakState.lock())
                {
                    RunReceiveLoop(locked);
                }
            }
            catch (const std::exception&)
            {
            }
        });
    }

    void StopReceiveLoop(const std::shared_ptr<RelaySessionData>& state)
    {
        if (!state)
        {
            return;
        }

        state->stopRequested.store(true, std::memory_order_release);
        if (state->webSocket)
        {
            state->webSocket->Abort();
        }

        if (state->receiveThread.joinable())
        {
            state->receiveThread.join();
        }

        state->state.store(RelaySessionState_Disconnected, std::memory_order_release);
    }
}

HRESULT CNostrClient::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrClient::FinalRelease() noexcept
{
    std::unordered_map<std::wstring, std::shared_ptr<RelaySessionData>> sessions;
    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        disposed_ = true;
        initialized_ = false;
        signer_.Release();
        sessions.swap(relaySessions_);
        guard.Unlock();
    }

    for (auto& pair : sessions)
    {
        const auto& entry = pair.second;
        if (!entry)
        {
            continue;
        }

        StopReceiveLoop(entry);

        if (entry->webSocket)
        {
            entry->webSocket->Close(WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, std::wstring());
            entry->webSocket.reset();
        }
    }

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
    if (!session)
    {
        return hresults::PointerRequired();
    }

    *session = nullptr;

    RelayDescriptorData descriptorData;
    HRESULT hr = ReadRelayDescriptor(descriptor, descriptorData);
    if (FAILED(hr))
    {
        return hr;
    }

    std::wstring normalizedUrl;
    hr = NormalizeRelayUrl(descriptorData.url, normalizedUrl);
    if (FAILED(hr))
    {
        return hr;
    }

    ATL::CComPtr<INostrAuthCallback> authCallbackPtr = authCallback;
    std::unique_ptr<NativeHttpClient> httpClient;
    std::unique_ptr<INativeWebSocket> webSocket;
    ClientRuntimeOptions runtimeOptions;

    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (!initialized_ || !resources_)
        {
            return hresults::E_NOSTR_NOT_INITIALIZED;
        }

        if (relaySessions_.find(normalizedUrl) != relaySessions_.end())
        {
            return hresults::InvalidArgument();
        }

        runtimeOptions = options_;
        httpClient = resources_->CreateHttpClient();
        webSocket = resources_->CreateWebSocket();
    }

    if (!httpClient || !webSocket)
    {
        return E_OUTOFMEMORY;
    }

    NativeHttpClient::RelayInformation relayInfo;
    hr = httpClient->FetchRelayInformation(normalizedUrl, relayInfo);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = webSocket->Connect(normalizedUrl, runtimeOptions);
    if (FAILED(hr))
    {
        return hr;
    }

    auto entry = std::make_shared<RelaySessionData>();
    if (!entry)
    {
        return E_OUTOFMEMORY;
    }

    entry->url = normalizedUrl;
    entry->readEnabled = descriptorData.readEnabled;
    entry->writeEnabled = descriptorData.writeEnabled;
    entry->preferred = descriptorData.preferred;
    entry->metadataJson = !relayInfo.metadataJson.empty() ? relayInfo.metadataJson : descriptorData.metadataJson;
    entry->supportedNips = relayInfo.supportedNips;
    entry->webSocket = std::move(webSocket);
    entry->authCallback = std::move(authCallbackPtr);
    entry->dispatcher = dispatcher_.get();
    entry->serializer = serializer_;
    entry->runtimeOptions = runtimeOptions;
    entry->state.store(RelaySessionState_Connected, std::memory_order_release);

    ATL::CComObject<CNostrRelaySession>* sessionObject = nullptr;
    hr = ATL::CComObject<CNostrRelaySession>::CreateInstance(&sessionObject);
    if (FAILED(hr))
    {
        return hr;
    }

    sessionObject->AddRef();
    hr = sessionObject->Initialize(entry);
    if (FAILED(hr))
    {
        sessionObject->Release();
        return hr;
    }

    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            sessionObject->Release();
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        auto [it, inserted] = relaySessions_.emplace(normalizedUrl, entry);
        if (!inserted)
        {
            sessionObject->Release();
            if (entry->webSocket)
            {
                entry->webSocket->Abort();
            }
            return hresults::InvalidArgument();
        }
    }

    StartReceiveLoop(entry);

    *session = sessionObject;
    return S_OK;
}

STDMETHODIMP CNostrClient::DisconnectRelay(BSTR relayUrl)
{
    if (!relayUrl)
    {
        return hresults::PointerRequired();
    }

    std::wstring normalizedUrl;
    HRESULT hr = NormalizeRelayUrl(BstrToWString(relayUrl), normalizedUrl);
    if (FAILED(hr))
    {
        return hr;
    }

    std::shared_ptr<RelaySessionData> entry;
    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (!initialized_)
        {
            return hresults::E_NOSTR_NOT_INITIALIZED;
        }

        const auto it = relaySessions_.find(normalizedUrl);
        if (it == relaySessions_.end())
        {
            return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
        }

        entry = it->second;
        relaySessions_.erase(it);
    }

    if (entry)
    {
        StopReceiveLoop(entry);
        if (entry->webSocket)
        {
            entry->webSocket->Close(WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, std::wstring());
            entry->webSocket.reset();
        }

        entry->authCallback.Release();
    }

    return S_OK;
}

STDMETHODIMP CNostrClient::HasRelay(BSTR relayUrl, VARIANT_BOOL* hasRelay)
{
    if (!relayUrl || !hasRelay)
    {
        return hresults::PointerRequired();
    }

    *hasRelay = VARIANT_FALSE;

    std::wstring normalizedUrl;
    HRESULT hr = NormalizeRelayUrl(BstrToWString(relayUrl), normalizedUrl);
    if (FAILED(hr))
    {
        return hr;
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

    if (relaySessions_.find(normalizedUrl) != relaySessions_.end())
    {
        *hasRelay = VARIANT_TRUE;
    }

    return S_OK;
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
    if (!relayUrls)
    {
        return hresults::PointerRequired();
    }

    *relayUrls = nullptr;

    std::vector<ATL::CComBSTR> urls;
    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (!initialized_)
        {
            return hresults::E_NOSTR_NOT_INITIALIZED;
        }

        urls.reserve(relaySessions_.size());
        for (const auto& entry : relaySessions_)
        {
            const auto& session = entry.second;
            if (session)
            {
                urls.emplace_back(session->url.c_str());
            }
            else
            {
                urls.emplace_back(entry.first.c_str());
            }
        }
    }

    return CreateSafeArrayFromStrings(urls, relayUrls);
}
HRESULT CNostrRelaySession::Initialize(std::shared_ptr<RelaySessionData> state)
{
    if (!state)
    {
        return hresults::PointerRequired();
    }

    state_ = state;
    return S_OK;
}

HRESULT CNostrRelaySession::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrRelaySession::FinalRelease() noexcept
{
    state_.reset();
}

STDMETHODIMP CNostrRelaySession::get_Url(BSTR* value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    *value = nullptr;
    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    return CComBSTR(state->url.c_str()).CopyTo(value);
}

STDMETHODIMP CNostrRelaySession::get_State(RelaySessionState* value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    *value = state->state.load(std::memory_order_acquire);
    return S_OK;
}

STDMETHODIMP CNostrRelaySession::get_LastOkResult(IDispatch** value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    *value = nullptr;

    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    bool success = false;
    std::wstring eventId;
    std::wstring message;
    bool hasValue = false;

    {
        std::lock_guard<std::mutex> guard(state->lastOkMutex);
        hasValue = state->hasLastOk;
        if (hasValue)
        {
            success = state->lastOkSuccess;
            eventId = state->lastOkEventId;
            message = state->lastOkMessageText;
        }
    }

    if (!hasValue)
    {
        return S_FALSE;
    }

    ATL::CComObject<CNostrOkResult>* ok = nullptr;
    HRESULT hr = ATL::CComObject<CNostrOkResult>::CreateInstance(&ok);
    if (FAILED(hr))
    {
        return hr;
    }

    ok->AddRef();
    ok->put_Success(success ? VARIANT_TRUE : VARIANT_FALSE);
    ok->put_EventId(CComBSTR(eventId.c_str()));
    ok->put_Message(CComBSTR(message.c_str()));

    hr = ok->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(value));
    ok->Release();
    return hr;
}
STDMETHODIMP CNostrRelaySession::get_SupportedNips(SAFEARRAY** value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    *value = nullptr;
    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    std::vector<long> nips;
    nips.reserve(state->supportedNips.size());
    for (const int nip : state->supportedNips)
    {
        nips.push_back(static_cast<long>(nip));
    }

    return CreateSafeArrayFromLongs(nips, value);
}

STDMETHODIMP CNostrRelaySession::get_WriteEnabled(VARIANT_BOOL* value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    *value = state->writeEnabled ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
}

STDMETHODIMP CNostrRelaySession::get_ReadEnabled(VARIANT_BOOL* value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    *value = state->readEnabled ? VARIANT_TRUE : VARIANT_FALSE;
    return S_OK;
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
    if (!value)
    {
        return hresults::PointerRequired();
    }

    *value = nullptr;
    const auto state = state_.lock();
    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    ATL::CComObject<CRelayDescriptor>* descriptor = nullptr;
    HRESULT hr = ATL::CComObject<CRelayDescriptor>::CreateInstance(&descriptor);
    if (FAILED(hr))
    {
        return hr;
    }

    descriptor->AddRef();

    hr = descriptor->put_Url(CComBSTR(state->url.c_str()));
    if (SUCCEEDED(hr))
    {
        hr = descriptor->put_ReadEnabled(state->readEnabled ? VARIANT_TRUE : VARIANT_FALSE);
    }
    if (SUCCEEDED(hr))
    {
        hr = descriptor->put_WriteEnabled(state->writeEnabled ? VARIANT_TRUE : VARIANT_FALSE);
    }
    if (SUCCEEDED(hr))
    {
        hr = descriptor->put_Preferred(state->preferred ? VARIANT_TRUE : VARIANT_FALSE);
    }
    if (SUCCEEDED(hr))
    {
        CComVariant metadata;
        if (!state->metadataJson.empty())
        {
            metadata = state->metadataJson.c_str();
        }
        hr = descriptor->put_Metadata(metadata);
    }

    if (FAILED(hr))
    {
        descriptor->Release();
        return hr;
    }

    *value = descriptor;
    return S_OK;
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
