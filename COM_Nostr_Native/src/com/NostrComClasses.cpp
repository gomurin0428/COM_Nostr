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
#include <array>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

using namespace com::nostr::native;

namespace
{
    using json = nlohmann::json;

    constexpr double kMillisecondsPerSecond = 1000.0;
    constexpr double kMaxTimeoutSeconds = static_cast<double>((std::numeric_limits<long long>::max)()) / kMillisecondsPerSecond;
    constexpr wchar_t kQueueOverflowReason[] = L"Subscription queue overflow.";

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

    HRESULT GenerateSubscriptionId(std::wstring& id)
    {
        id.clear();
        std::array<uint8_t, 32> buffer{};
        const NTSTATUS status = BCryptGenRandom(nullptr,
                                                buffer.data(),
                                                static_cast<ULONG>(buffer.size()),
                                                BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(status))
        {
            return HRESULT_FROM_NT(status);
        }

        static constexpr wchar_t kHexDigits[] = L"0123456789abcdef";
        std::wstring generated;
        generated.reserve(buffer.size() * 2);
        for (uint8_t value : buffer)
        {
            generated.push_back(kHexDigits[(value >> 4) & 0x0F]);
            generated.push_back(kHexDigits[value & 0x0F]);
        }

        id = std::move(generated);
        return S_OK;
    }

    HRESULT NormalizeSubscriptionOptions(IDispatch* dispatch,
                                         RelaySessionData::SubscriptionOptionsData& options)
    {
        options = RelaySessionData::SubscriptionOptionsData{};
        if (!dispatch)
        {
            return S_OK;
        }

        ATL::CComDispatchDriver driver(dispatch);
        if (!driver)
        {
            return DISP_E_TYPEMISMATCH;
        }

        CComVariant value;
        HRESULT hr = driver.GetProperty(1, &value);
        if (SUCCEEDED(hr) && VariantHasValue(value))
        {
            hr = value.ChangeType(VT_BOOL);
            if (FAILED(hr))
            {
                return DISP_E_TYPEMISMATCH;
            }

            options.keepAlive = value.boolVal == VARIANT_TRUE;
        }
        else if (FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND)
        {
            return hr;
        }

        value.Clear();
        hr = driver.GetProperty(2, &value);
        if (SUCCEEDED(hr) && VariantHasValue(value))
        {
            const auto seconds = VariantToDouble(value);
            if (!seconds || *seconds < 0.0)
            {
                return hresults::InvalidArgument();
            }

            if (*seconds > 0.0)
            {
                options.autoRequeryWindowSeconds = *seconds;
            }
        }
        else if (FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND)
        {
            return hr;
        }

        value.Clear();
        hr = driver.GetProperty(3, &value);
        if (SUCCEEDED(hr) && VariantHasValue(value))
        {
            const auto length = VariantToLong(value);
            if (!length || *length <= 0)
            {
                return hresults::InvalidArgument();
            }

            options.maxQueueLength = static_cast<uint32_t>(*length);
        }
        else if (FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND)
        {
            return hr;
        }

        value.Clear();
        hr = driver.GetProperty(4, &value);
        if (SUCCEEDED(hr) && VariantHasValue(value))
        {
            const auto strategy = VariantToLong(value);
            if (!strategy)
            {
                return hresults::InvalidArgument();
            }

            if (*strategy != QueueOverflowStrategy_DropOldest &&
                *strategy != QueueOverflowStrategy_Throw)
            {
                return hresults::InvalidArgument();
            }

            options.overflowStrategy = static_cast<QueueOverflowStrategy>(*strategy);
        }
        else if (FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND)
        {
            return hr;
        }

        return S_OK;
    }

    HRESULT CopyFilterDispatchArray(SAFEARRAY* array, std::vector<ATL::CComPtr<IDispatch>>& target)
    {
        target.clear();
        if (!array)
        {
            return hresults::InvalidArgument();
        }

        LONG lower = 0;
        LONG upper = -1;
        HRESULT hr = SafeArrayGetLBound(array, 1, &lower);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = SafeArrayGetUBound(array, 1, &upper);
        if (FAILED(hr))
        {
            return hr;
        }

        if (upper < lower)
        {
            return S_OK;
        }

        VARTYPE vt = VT_EMPTY;
        hr = SafeArrayGetVartype(array, &vt);
        const bool directDispatch = SUCCEEDED(hr) && vt == VT_DISPATCH;

        const LONG count = upper - lower + 1;
        target.reserve(static_cast<size_t>(count));

        for (LONG index = lower; index <= upper; ++index)
        {
            if (directDispatch)
            {
                IDispatch* pointer = nullptr;
                hr = SafeArrayGetElement(array, &index, &pointer);
                if (FAILED(hr))
                {
                    return hr;
                }

                target.emplace_back();
                target.back().Attach(pointer);
                continue;
            }

            CComVariant element;
            hr = SafeArrayGetElement(array, &index, &element);
            if (FAILED(hr))
            {
                return hr;
            }

            if (element.vt != VT_DISPATCH || !element.pdispVal)
            {
                return DISP_E_TYPEMISMATCH;
            }

            target.emplace_back();
            target.back().Attach(element.pdispVal);
            element.pdispVal = nullptr;
            element.vt = VT_EMPTY;
        }

        return S_OK;
    }

    HRESULT CreateFilterSafeArray(const std::vector<ATL::CComPtr<IDispatch>>& source, SAFEARRAY** result)
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;

        SAFEARRAY* array = SafeArrayCreateVector(VT_DISPATCH, 0, static_cast<ULONG>(source.size()));
        if (!array && !source.empty())
        {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = S_OK;
        for (LONG index = 0; index < static_cast<LONG>(source.size()); ++index)
        {
            IDispatch* pointer = source[static_cast<size_t>(index)];
            if (!pointer)
            {
                hr = hresults::PointerRequired();
                break;
            }

            pointer->AddRef();
            hr = SafeArrayPutElement(array, &index, pointer);
            pointer->Release();
            if (FAILED(hr))
            {
                break;
            }
        }

        if (FAILED(hr))
        {
            SafeArrayDestroy(array);
            return hr;
        }

        if (!array)
        {
            array = SafeArrayCreateVector(VT_DISPATCH, 0, 0);
            if (!array)
            {
                return E_OUTOFMEMORY;
            }
        }

        *result = array;
        return S_OK;
    }

    void ScheduleSubscriptionDispatch(const std::shared_ptr<RelaySessionData>& state,
                                       const std::shared_ptr<RelaySessionData::SubscriptionEntry>& entry)
    {
        if (!state || !entry)
        {
            return;
        }

        ComCallbackDispatcher* dispatcher = state->dispatcher;
        if (!dispatcher)
        {
            return;
        }

        INostrEventCallback* callback = entry->callback;
        if (!callback)
        {
            return;
        }

        callback->AddRef();
        const std::weak_ptr<RelaySessionData> weakState(state);
        const std::weak_ptr<RelaySessionData::SubscriptionEntry> weakEntry(entry);
        const std::shared_ptr<NostrJsonSerializer> serializer = state->serializer;

        const HRESULT hr = dispatcher->Post([weakState, weakEntry, serializer, callback]() mutable
        {
            auto stateLocked = weakState.lock();
            auto entryLocked = weakEntry.lock();
            if (!stateLocked || !entryLocked)
            {
                callback->Release();
                return;
            }

            for (;;)
            {
                NostrJsonSerializer::EventData eventData;
                bool hasEvent = false;

                {
                    std::lock_guard<std::mutex> guard(stateLocked->subscriptionMutex);
                    if (!entryLocked->eventQueue.empty() && !entryLocked->overflowClosed)
                    {
                        eventData = entryLocked->eventQueue.front();
                        entryLocked->eventQueue.pop_front();
                        hasEvent = true;
                    }
                    else
                    {
                        entryLocked->dispatchInProgress = false;
                    }
                }

                if (!hasEvent)
                {
                    callback->Release();
                    return;
                }

                if (!serializer)
                {
                    continue;
                }

                ATL::CComPtr<INostrEvent> eventDispatch;
                if (FAILED(serializer->PopulateEventDispatch(eventData, &eventDispatch)) || !eventDispatch)
                {
                    continue;
                }

                CComVariant args[2];
                args[0].vt = VT_DISPATCH;
                args[0].pdispVal = eventDispatch.Detach();
                args[1] = CComVariant(stateLocked->url.c_str());

                DISPPARAMS params{};
                params.cArgs = 2;
                params.rgvarg = args;

                callback->Invoke(1, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);

                if (args[0].pdispVal)
                {
                    args[0].pdispVal->Release();
                    args[0].pdispVal = nullptr;
                }

                args[0].vt = VT_EMPTY;
            }
        });

        if (FAILED(hr))
        {
            callback->Release();
            std::lock_guard<std::mutex> guard(state->subscriptionMutex);
            entry->dispatchInProgress = false;
        }
    }

    bool EnqueueEventForSubscription(const std::shared_ptr<RelaySessionData>& state,
                                     const std::shared_ptr<RelaySessionData::SubscriptionEntry>& entry,
                                     const NostrJsonSerializer::EventMessage& message,
                                     std::wstring& overflowReason)
    {
        if (!state || !entry)
        {
            return false;
        }

        bool scheduleDispatch = false;

        {
            std::lock_guard<std::mutex> guard(state->subscriptionMutex);
            if (entry->overflowClosed)
            {
                return false;
            }

            entry->lastEventTimestamp = message.event.createdAt;

            if (entry->options.maxQueueLength && entry->options.maxQueueLength.value() > 0)
            {
                const size_t limit = entry->options.maxQueueLength.value();
                if (entry->eventQueue.size() >= limit)
                {
                    if (entry->options.overflowStrategy == QueueOverflowStrategy_DropOldest)
                    {
                        if (!entry->eventQueue.empty())
                        {
                            entry->eventQueue.pop_front();
                        }
                    }
                    else
                    {
                        entry->overflowClosed = true;
                        entry->status = SubscriptionStatus_Closed;
                        entry->eventQueue.clear();
                        entry->dispatchInProgress = false;
                        entry->closeRequested = false;
                        overflowReason = kQueueOverflowReason;
                        return false;
                    }
                }
            }

            entry->eventQueue.push_back(message.event);
            if (!entry->dispatchInProgress)
            {
                entry->dispatchInProgress = true;
                scheduleDispatch = true;
            }
        }

        if (scheduleDispatch)
        {
            ScheduleSubscriptionDispatch(state, entry);
        }

        return true;
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
            return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7601);
        }

        CComBSTR url;
        HRESULT hr = relay->get_Url(&url);
        if (FAILED(hr))
        {
            return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7602);
        }

        if (!url || SysStringLen(url) == 0)
        {
            return hresults::InvalidArgument();
        }

        data.url = BstrToWString(url);

        VARIANT_BOOL flag = VARIANT_TRUE;
        hr = relay->get_ReadEnabled(&flag);
        if (FAILED(hr))
        {
            return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7603);
        }
        data.readEnabled = flag == VARIANT_TRUE;

        flag = VARIANT_TRUE;
        hr = relay->get_WriteEnabled(&flag);
        if (FAILED(hr))
        {
            return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7604);
        }
        data.writeEnabled = flag == VARIANT_TRUE;

        flag = VARIANT_FALSE;
        hr = relay->get_Preferred(&flag);
        if (FAILED(hr))
        {
            return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7605);
        }
        data.preferred = flag == VARIANT_TRUE;

        CComVariant metadata;
        hr = relay->get_Metadata(&metadata);
        if (FAILED(hr))
        {
            return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7606);
        }
        if (VariantHasValue(metadata))
        {
            CComVariant copy(metadata);
            hr = copy.ChangeType(VT_BSTR);
            if (FAILED(hr))
            {
                return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x7607);
            }
            if (copy.bstrVal)
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
        {
            std::lock_guard<std::mutex> guard(state->subscriptionMutex);
            const auto it = state->subscriptions.find(subscriptionId);
            if (it != state->subscriptions.end() && it->second)
            {
                it->second->closeRequested = false;
            }
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

        UNREFERENCED_PARAMETER(serializer);

        const auto targets = CollectSubscriptions(state, message.subscriptionId);
        if (targets.empty())
        {
            return;
        }

        std::vector<std::shared_ptr<RelaySessionData::SubscriptionEntry>> overflowed;
        overflowed.reserve(targets.size());

        for (const auto& target : targets)
        {
            if (!target || !target->callback)
            {
                continue;
            }

            std::wstring overflowReason;
            if (!EnqueueEventForSubscription(state, target, message, overflowReason) && !overflowReason.empty())
            {
                overflowed.push_back(target);
            }
        }

        for (const auto& overflowedEntry : overflowed)
        {
            if (!overflowedEntry)
            {
                continue;
            }

            DispatchClosed(state, dispatcher, overflowedEntry->id, kQueueOverflowReason);
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

        {
            std::lock_guard<std::mutex> guard(state->authMutex);
            state->pendingAuthChallenge = message.challenge;
            state->pendingAuthExpiresAt = message.expiresAt;
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
            std::unique_lock<std::mutex> guard(state->lastOkMutex);
            state->hasLastOk = true;
            state->lastOkSuccess = message.success;
            state->lastOkEventId = message.eventId;
            state->lastOkMessageText = message.message;
            ++state->lastOkVersion;
        }

        state->lastOkCondition.notify_all();

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
        state->lastOkCondition.notify_all();
    }

    HRESULT CreateSafeArrayFromEventTags(const std::vector<std::vector<std::wstring>>& tags,
                                         SAFEARRAY** result)
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;

        std::vector<std::vector<CComBSTR>> converted;
        converted.reserve(tags.size());
        for (const auto& tag : tags)
        {
            std::vector<CComBSTR> row;
            row.reserve(tag.size());
            for (const auto& value : tag)
            {
                row.emplace_back(value.c_str());
            }

            converted.push_back(std::move(row));
        }

        return CreateSafeArrayFromTagMatrix(converted, result);
    }

    HRESULT WriteEventBackToDispatch(IDispatch* dispatch, const NostrJsonSerializer::EventData& event)
    {
        if (!dispatch)
        {
            return E_POINTER;
        }

        ATL::CComDispatchDriver driver(dispatch);
        if (!driver)
        {
            return DISP_E_TYPEMISMATCH;
        }

        HRESULT hr = S_OK;

        CComVariant value(event.id.c_str());
        hr = driver.PutProperty(1, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        value = CComVariant(event.publicKey.c_str());
        hr = driver.PutProperty(2, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        value = CComVariant(event.createdAt);
        hr = driver.PutProperty(3, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        value = CComVariant(event.kind);
        hr = driver.PutProperty(4, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        SAFEARRAY* tagsArray = nullptr;
        hr = CreateSafeArrayFromEventTags(event.tags, &tagsArray);
        if (FAILED(hr))
        {
            return hr;
        }

        CComVariant tagsVariant;
        tagsVariant.vt = VT_ARRAY | VT_VARIANT;
        tagsVariant.parray = tagsArray;
        hr = driver.PutProperty(5, &tagsVariant);
        tagsVariant.parray = nullptr;
        tagsVariant.vt = VT_EMPTY;
        SafeArrayDestroy(tagsArray);
        if (FAILED(hr))
        {
            return hr;
        }

        value = CComVariant(event.content.c_str());
        hr = driver.PutProperty(6, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        value = CComVariant(event.signature.c_str());
        return driver.PutProperty(7, &value);
    }

    HRESULT CreateEventDraftDispatch(const NostrJsonSerializer::EventData& event,
                                     IDispatch** draftDispatch)
    {
        if (!draftDispatch)
        {
            return E_POINTER;
        }

        *draftDispatch = nullptr;

        ATL::CComObject<CNostrEventDraft>* draft = nullptr;
        HRESULT hr = ATL::CComObject<CNostrEventDraft>::CreateInstance(&draft);
        if (FAILED(hr))
        {
            return hr;
        }

        draft->AddRef();
        hr = draft->put_PublicKey(CComBSTR(event.publicKey.c_str()));
        if (SUCCEEDED(hr))
        {
            hr = draft->put_CreatedAt(event.createdAt);
        }

        if (SUCCEEDED(hr))
        {
            hr = draft->put_Kind(event.kind);
        }

        if (SUCCEEDED(hr))
        {
            SAFEARRAY* tagsArray = nullptr;
            hr = CreateSafeArrayFromEventTags(event.tags, &tagsArray);
            if (SUCCEEDED(hr))
            {
                hr = draft->put_Tags(tagsArray);
                SafeArrayDestroy(tagsArray);
            }
        }

        if (SUCCEEDED(hr))
        {
            hr = draft->put_Content(CComBSTR(event.content.c_str()));
        }

        if (SUCCEEDED(hr))
        {
            hr = draft->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(draftDispatch));
        }

        draft->Release();
        return hr;
    }

    HRESULT ComputeEventId(const NostrJsonSerializer::EventData& event, std::wstring& eventId)
    {
        eventId.clear();

        if (!std::isfinite(event.createdAt))
        {
            return hresults::InvalidArgument();
        }

        constexpr double kMaxTimestamp = static_cast<double>((std::numeric_limits<long long>::max)());
        constexpr double kMinTimestamp = static_cast<double>((std::numeric_limits<long long>::min)());
        if (event.createdAt > kMaxTimestamp || event.createdAt < kMinTimestamp)
        {
            return hresults::InvalidArgument();
        }

        const long long integralCreatedAt = static_cast<long long>(std::llround(event.createdAt));

        json payload = json::array();
        payload.push_back(0);
        payload.push_back(WideToUtf8(event.publicKey));
        payload.push_back(integralCreatedAt);
        payload.push_back(event.kind);

        json tags = json::array();
        for (const auto& tag : event.tags)
        {
            json tagArray = json::array();
            for (const auto& value : tag)
            {
                tagArray.push_back(WideToUtf8(value));
            }
            tags.push_back(std::move(tagArray));
        }

        payload.push_back(std::move(tags));
        payload.push_back(WideToUtf8(event.content));

        const std::string serialized = payload.dump();

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;

        NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(status))
        {
            return HRESULT_FROM_NT(status);
        }

        ULONG objectLength = 0;
        ULONG resultLength = 0;
        status = BCryptGetProperty(algorithm,
                                   BCRYPT_OBJECT_LENGTH,
                                   reinterpret_cast<PUCHAR>(&objectLength),
                                   sizeof(objectLength),
                                   &resultLength,
                                   0);
        if (!BCRYPT_SUCCESS(status))
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return HRESULT_FROM_NT(status);
        }

        std::vector<uint8_t> hashObject(objectLength);
        status = BCryptCreateHash(algorithm,
                                  &hash,
                                  hashObject.data(),
                                  static_cast<ULONG>(hashObject.size()),
                                  nullptr,
                                  0,
                                  0);
        if (!BCRYPT_SUCCESS(status))
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return HRESULT_FROM_NT(status);
        }

        ULONG hashLength = 0;
        status = BCryptGetProperty(algorithm,
                                   BCRYPT_HASH_LENGTH,
                                   reinterpret_cast<PUCHAR>(&hashLength),
                                   sizeof(hashLength),
                                   &resultLength,
                                   0);
        if (!BCRYPT_SUCCESS(status))
        {
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return HRESULT_FROM_NT(status);
        }

        std::vector<uint8_t> hashBuffer(hashLength);

        status = BCryptHashData(hash,
                                reinterpret_cast<PUCHAR>(const_cast<char*>(serialized.data())),
                                static_cast<ULONG>(serialized.size()),
                                0);
        if (BCRYPT_SUCCESS(status))
        {
            status = BCryptFinishHash(hash, hashBuffer.data(), static_cast<ULONG>(hashBuffer.size()), 0);
        }

        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);

        if (!BCRYPT_SUCCESS(status))
        {
            return HRESULT_FROM_NT(status);
        }

        static constexpr wchar_t kHexDigits[] = L"0123456789abcdef";
        std::wstring hex;
        hex.reserve(hashBuffer.size() * 2);
        for (uint8_t value : hashBuffer)
        {
            hex.push_back(kHexDigits[(value >> 4) & 0x0F]);
            hex.push_back(kHexDigits[value & 0x0F]);
        }

        eventId = std::move(hex);
        return S_OK;
    }

    HRESULT CallSignerGetPublicKey(INostrSigner* signer, std::wstring& publicKey)
    {
        publicKey.clear();
        if (!signer)
        {
            return hresults::PointerRequired();
        }

        DISPPARAMS params{};
        CComVariant result;
        const HRESULT hr = signer->Invoke(2, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &result, nullptr, nullptr);
        if (FAILED(hr))
        {
            return hr;
        }

        HRESULT converted = result.ChangeType(VT_BSTR);
        if (FAILED(converted))
        {
            return converted;
        }

        if (result.bstrVal)
        {
            publicKey = BstrToWString(result.bstrVal);
        }

        return S_OK;
    }

    HRESULT CallSignerSign(INostrSigner* signer, IDispatch* draft, std::wstring& signature)
    {
        signature.clear();
        if (!signer || !draft)
        {
            return hresults::PointerRequired();
        }

        CComVariant arg(draft);
        if (arg.vt != VT_DISPATCH || !arg.pdispVal)
        {
            return hresults::PointerRequired();
        }

        DISPPARAMS params{};
        params.cArgs = 1;
        params.rgvarg = &arg;

        CComVariant result;
        HRESULT hr = signer->Invoke(1, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &result, nullptr, nullptr);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = result.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }

        if (result.bstrVal)
        {
            signature = BstrToWString(result.bstrVal);
        }

        return S_OK;
    }

    DWORD ResolveOkWaitTimeoutMilliseconds(const RelaySessionData& state)
    {
        const auto& timeout = state.runtimeOptions.ReceiveTimeout();
        if (timeout && timeout->count() > 0)
        {
            const long long value = timeout->count();
            if (value <= 0)
            {
                return 10000;
            }

            if (value > static_cast<long long>((std::numeric_limits<DWORD>::max)()))
            {
                return (std::numeric_limits<DWORD>::max)();
            }

            return static_cast<DWORD>(value);
        }

        return 10000;
    }

    HRESULT WaitForOkResult(const std::shared_ptr<RelaySessionData>& state,
                            uint64_t initialVersion,
                            const std::wstring& expectedEventId,
                            DWORD timeoutMilliseconds,
                            bool& success,
                            std::wstring& message)
    {
        success = false;
        message.clear();

        if (!state)
        {
            return hresults::PointerRequired();
        }

        if (timeoutMilliseconds == 0)
        {
            return hresults::Timeout();
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
        std::unique_lock<std::mutex> lock(state->lastOkMutex);

        for (;;)
        {
            if (state->lastOkVersion > initialVersion)
            {
                if (state->hasLastOk && (expectedEventId.empty() || state->lastOkEventId == expectedEventId))
                {
                    success = state->lastOkSuccess;
                    message = state->lastOkMessageText;
                    return S_OK;
                }

                initialVersion = state->lastOkVersion;
            }

            if (state->stopRequested.load(std::memory_order_acquire))
            {
                return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
            }

            if (std::chrono::steady_clock::now() >= deadline)
            {
                return hresults::Timeout();
            }

            const auto waitResult = state->lastOkCondition.wait_until(lock, deadline);
            if (waitResult == std::cv_status::timeout)
            {
                return hresults::Timeout();
            }
        }
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
    if (!subscription)
    {
        return hresults::PointerRequired();
    }

    *subscription = nullptr;

    if (!relayUrl || !callback)
    {
        return hresults::PointerRequired();
    }

    if (!filters)
    {
        return hresults::InvalidArgument();
    }

    std::wstring normalizedUrl;
    HRESULT hr = NormalizeRelayUrl(BstrToWString(relayUrl), normalizedUrl);
    if (FAILED(hr))
    {
        return hr;
    }

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<NostrJsonSerializer> serializer;
    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (!initialized_ || !dispatcher_ || !serializer_)
        {
            return hresults::E_NOSTR_NOT_INITIALIZED;
        }

        const auto it = relaySessions_.find(normalizedUrl);
        if (it == relaySessions_.end())
        {
            return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
        }

        state = it->second;
        serializer = serializer_;
    }

    if (!state || !state->webSocket)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    if (!state->readEnabled)
    {
        return hresults::InvalidArgument();
    }

    if (!state->dispatcher || !serializer)
    {
        return E_UNEXPECTED;
    }

    std::vector<NostrJsonSerializer::FilterData> parsedFilters;
    hr = serializer->ReadFiltersFromSafeArray(filters, parsedFilters);
    if (FAILED(hr))
    {
        return hr;
    }

    if (parsedFilters.empty())
    {
        return hresults::InvalidArgument();
    }

    std::vector<ATL::CComPtr<IDispatch>> originalFilters;
    hr = CopyFilterDispatchArray(filters, originalFilters);
    if (FAILED(hr))
    {
        return hr;
    }

    RelaySessionData::SubscriptionOptionsData optionData;
    hr = NormalizeSubscriptionOptions(options, optionData);
    if (FAILED(hr))
    {
        return hr;
    }

    auto entry = std::make_shared<RelaySessionData::SubscriptionEntry>();
    if (!entry)
    {
        return E_OUTOFMEMORY;
    }

    entry->callback = callback;
    entry->filters = std::move(parsedFilters);
    entry->options = optionData;
    entry->originalFilters = std::move(originalFilters);
    entry->status = SubscriptionStatus_Pending;

    ATL::CComObject<CNostrSubscription>* subscriptionObject = nullptr;
    hr = ATL::CComObject<CNostrSubscription>::CreateInstance(&subscriptionObject);
    if (FAILED(hr))
    {
        return hr;
    }

    subscriptionObject->AddRef();

    bool inserted = false;
    constexpr int kMaxIdAttempts = 5;
    for (int attempt = 0; attempt < kMaxIdAttempts && !inserted; ++attempt)
    {
        hr = GenerateSubscriptionId(entry->id);
        if (FAILED(hr))
        {
            subscriptionObject->Release();
            return hr;
        }

        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        auto [_, success] = state->subscriptions.emplace(entry->id, entry);
        inserted = success;
    }

    if (!inserted)
    {
        subscriptionObject->Release();
        return E_FAIL;
    }

    hr = subscriptionObject->Initialize(state, entry);
    if (FAILED(hr))
    {
        {
            std::lock_guard<std::mutex> guard(state->subscriptionMutex);
            state->subscriptions.erase(entry->id);
        }

        subscriptionObject->Release();
        return hr;
    }

    NostrJsonSerializer::RequestMessage request;
    request.subscriptionId = entry->id;
    request.filters = entry->filters;
    const auto payload = serializer->SerializeRequest(request);

    hr = state->webSocket->SendText(payload, true);
    if (FAILED(hr))
    {
        {
            std::lock_guard<std::mutex> guard(state->subscriptionMutex);
            state->subscriptions.erase(entry->id);
        }

        subscriptionObject->Release();
        return hr;
    }

    *subscription = subscriptionObject;
    return S_OK;
}
STDMETHODIMP CNostrClient::PublishEvent(BSTR relayUrl, IDispatch* eventPayload)
{
    if (!relayUrl || !eventPayload)
    {
        return hresults::PointerRequired();
    }

    std::wstring normalizedUrl;
    HRESULT hr = NormalizeRelayUrl(BstrToWString(relayUrl), normalizedUrl);
    if (FAILED(hr))
    {
        return hr;
    }

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<NostrJsonSerializer> serializer;
    CComPtr<INostrSigner> signer;

    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (!initialized_ || !serializer_)
        {
            return hresults::E_NOSTR_NOT_INITIALIZED;
        }

        const auto it = relaySessions_.find(normalizedUrl);
        if (it == relaySessions_.end())
        {
            return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
        }

        state = it->second;
        serializer = serializer_;
        signer = signer_;
    }

    if (!state || !state->webSocket)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    if (!state->writeEnabled)
    {
        return hresults::InvalidArgument();
    }

    if (!serializer)
    {
        return E_UNEXPECTED;
    }

    if (!signer)
    {
        return hresults::E_NOSTR_SIGNER_MISSING;
    }

    NostrJsonSerializer::EventData eventData;
    hr = serializer->ReadEventFromDispatch(eventPayload, eventData);
    if (FAILED(hr))
    {
        return hr;
    }

    bool requiresSigning = eventData.id.empty() || eventData.signature.empty() || eventData.publicKey.empty();

    if (requiresSigning)
    {
        if (eventData.publicKey.empty())
        {
            std::wstring publicKey;
            hr = CallSignerGetPublicKey(signer, publicKey);
            if (FAILED(hr))
            {
                return hr;
            }

            if (publicKey.empty())
            {
                return hresults::InvalidArgument();
            }

            eventData.publicKey = std::move(publicKey);
        }

        CComPtr<IDispatch> draftDispatch;
        hr = CreateEventDraftDispatch(eventData, &draftDispatch);
        if (FAILED(hr))
        {
            return hr;
        }

        std::wstring signature;
        hr = CallSignerSign(signer, draftDispatch, signature);
        if (FAILED(hr))
        {
            return hr;
        }

        if (signature.empty())
        {
            return hresults::InvalidArgument();
        }

        std::wstring eventId;
        hr = ComputeEventId(eventData, eventId);
        if (FAILED(hr))
        {
            return hr;
        }

        eventData.id = std::move(eventId);
        eventData.signature = std::move(signature);
    }
    else if (eventData.id.empty())
    {
        std::wstring eventId;
        hr = ComputeEventId(eventData, eventId);
        if (FAILED(hr))
        {
            return hr;
        }

        eventData.id = std::move(eventId);
    }

    hr = WriteEventBackToDispatch(eventPayload, eventData);
    if (FAILED(hr))
    {
        return hr;
    }

    const auto payload = serializer->SerializeEvent(eventData);

    uint64_t initialVersion = 0;
    {
        std::lock_guard<std::mutex> lock(state->lastOkMutex);
        initialVersion = state->lastOkVersion;
    }

    hr = state->webSocket->SendText(payload, true);
    if (FAILED(hr))
    {
        return hr;
    }

    const DWORD timeout = ResolveOkWaitTimeoutMilliseconds(*state);
    bool okSuccess = false;
    std::wstring okMessage;
    hr = WaitForOkResult(state, initialVersion, eventData.id, timeout, okSuccess, okMessage);
    if (FAILED(hr))
    {
        return hr;
    }

    if (!okSuccess)
    {
        return hresults::WebSocketFailure();
    }

    return S_OK;
}

STDMETHODIMP CNostrClient::RespondAuth(BSTR relayUrl, IDispatch* authEvent)
{
    if (!relayUrl || !authEvent)
    {
        return hresults::PointerRequired();
    }

    std::wstring normalizedUrl;
    HRESULT hr = NormalizeRelayUrl(BstrToWString(relayUrl), normalizedUrl);
    if (FAILED(hr))
    {
        return hr;
    }

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<NostrJsonSerializer> serializer;
    CComPtr<INostrSigner> signer;

    {
        ATL::CComCritSecLock<ATL::CComAutoCriticalSection> guard(stateLock_);
        if (disposed_)
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        if (!initialized_ || !serializer_)
        {
            return hresults::E_NOSTR_NOT_INITIALIZED;
        }

        const auto it = relaySessions_.find(normalizedUrl);
        if (it == relaySessions_.end())
        {
            return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
        }

        state = it->second;
        serializer = serializer_;
        signer = signer_;
    }

    if (!state || !state->webSocket)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    if (!state->writeEnabled)
    {
        return hresults::InvalidArgument();
    }

    if (!serializer)
    {
        return E_UNEXPECTED;
    }

    if (!signer)
    {
        return hresults::E_NOSTR_SIGNER_MISSING;
    }

    NostrJsonSerializer::EventData eventData;
    hr = serializer->ReadEventFromDispatch(authEvent, eventData);
    if (FAILED(hr))
    {
        return hr;
    }

    constexpr int kAuthKind = 22242;
    if (eventData.kind == 0)
    {
        eventData.kind = kAuthKind;
    }
    else if (eventData.kind != kAuthKind)
    {
        return hresults::InvalidArgument();
    }

    if (!std::isfinite(eventData.createdAt) || eventData.createdAt <= 0.0)
    {
        const auto now = std::chrono::system_clock::now();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        eventData.createdAt = static_cast<double>(seconds);
    }

    std::optional<std::wstring> storedChallenge;
    std::optional<double> storedExpires;
    {
        std::lock_guard<std::mutex> guard(state->authMutex);
        storedChallenge = state->pendingAuthChallenge;
        storedExpires = state->pendingAuthExpiresAt;
    }

    std::wstring challengeValue;
    bool relayTagFound = false;
    bool challengeTagFound = false;

    for (auto& tag : eventData.tags)
    {
        if (tag.empty())
        {
            continue;
        }

        const std::wstring& label = tag[0];
        if (label == L"relay")
        {
            relayTagFound = true;
            if (tag.size() < 2)
            {
                tag.resize(2);
            }
            tag[1] = state->url;
        }
        else if (label == L"challenge")
        {
            challengeTagFound = true;
            if (tag.size() >= 2)
            {
                challengeValue = tag[1];
            }
        }
    }

    if (!relayTagFound)
    {
        eventData.tags.push_back({ L"relay", state->url });
    }

    if (challengeValue.empty() && storedChallenge && !storedChallenge->empty())
    {
        challengeValue = *storedChallenge;
    }

    if (challengeValue.empty())
    {
        return hresults::InvalidArgument();
    }

    if (storedChallenge && !storedChallenge->empty() && challengeValue != *storedChallenge)
    {
        return hresults::InvalidArgument();
    }

    if (storedExpires && *storedExpires > 0.0)
    {
        const auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (static_cast<double>(nowSeconds) > *storedExpires)
        {
            return hresults::InvalidArgument();
        }
    }

    if (!challengeTagFound)
    {
        eventData.tags.push_back({ L"challenge", challengeValue });
    }
    else
    {
        for (auto& tag : eventData.tags)
        {
            if (!tag.empty() && tag[0] == L"challenge")
            {
                if (tag.size() < 2)
                {
                    tag.resize(2);
                }
                tag[1] = challengeValue;
                break;
            }
        }
    }

    std::wstring publicKey;
    hr = CallSignerGetPublicKey(signer, publicKey);
    if (FAILED(hr))
    {
        return hr;
    }

    if (publicKey.empty())
    {
        return hresults::InvalidArgument();
    }

    eventData.publicKey = std::move(publicKey);
    eventData.signature.clear();
    eventData.id.clear();

    {
        std::lock_guard<std::mutex> guard(state->authMutex);
        state->pendingAuthChallenge = challengeValue;
        state->pendingAuthExpiresAt = storedExpires;
    }

    CComPtr<IDispatch> draftDispatch;
    hr = CreateEventDraftDispatch(eventData, &draftDispatch);
    if (FAILED(hr))
    {
        return hr;
    }

    std::wstring signature;
    hr = CallSignerSign(signer, draftDispatch, signature);
    if (FAILED(hr))
    {
        return hr;
    }

    if (signature.empty())
    {
        return hresults::InvalidArgument();
    }

    std::wstring eventId;
    hr = ComputeEventId(eventData, eventId);
    if (FAILED(hr))
    {
        return hr;
    }

    eventData.id = std::move(eventId);
    eventData.signature = std::move(signature);

    hr = WriteEventBackToDispatch(authEvent, eventData);
    if (FAILED(hr))
    {
        return hr;
    }

    const auto payload = serializer->SerializeAuth(eventData);

    uint64_t initialVersion = 0;
    {
        std::lock_guard<std::mutex> lock(state->lastOkMutex);
        initialVersion = state->lastOkVersion;
    }

    hr = state->webSocket->SendText(payload, true);
    if (FAILED(hr))
    {
        return hr;
    }

    const DWORD timeout = ResolveOkWaitTimeoutMilliseconds(*state);
    bool okSuccess = false;
    std::wstring okMessage;
    hr = WaitForOkResult(state, initialVersion, eventData.id, timeout, okSuccess, okMessage);
    if (FAILED(hr))
    {
        return hr;
    }

    ATL::CComPtr<INostrAuthCallback> authCallback = state->authCallback;

    if (okSuccess)
    {
        {
            std::lock_guard<std::mutex> guard(state->authMutex);
            state->pendingAuthChallenge.reset();
            state->pendingAuthExpiresAt.reset();
        }

        if (authCallback)
        {
            CComVariant arg(state->url.c_str());
            DISPPARAMS params{};
            params.cArgs = 1;
            params.rgvarg = &arg;
            authCallback->Invoke(2, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
        }

        return S_OK;
    }

    if (authCallback)
    {
        CComVariant args[2];
        args[0] = CComVariant(okMessage.c_str());
        args[1] = CComVariant(state->url.c_str());
        DISPPARAMS params{};
        params.cArgs = 2;
        params.rgvarg = args;
        authCallback->Invoke(3, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
    }

    return hresults::WebSocketFailure();
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
    auto state = state_.lock();
    auto entry = entry_.lock();

    if (entry)
    {
        if (state)
        {
            std::lock_guard<std::mutex> guard(state->subscriptionMutex);
            if (entry->owner == this)
            {
                entry->owner = nullptr;
            }
        }
        else if (entry->owner == this)
        {
            entry->owner = nullptr;
        }
    }

    state_.reset();
    entry_.reset();
}

HRESULT CNostrSubscription::Initialize(std::shared_ptr<RelaySessionData> state,
                                       std::shared_ptr<RelaySessionData::SubscriptionEntry> entry)
{
    if (!state || !entry)
    {
        return hresults::PointerRequired();
    }

    state_ = state;
    entry_ = entry;

    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        entry->owner = this;
    }

    return S_OK;
}

HRESULT CNostrSubscription::ResolveStateAndEntry(std::shared_ptr<RelaySessionData>& state,
                                                 std::shared_ptr<RelaySessionData::SubscriptionEntry>& entry) const
{
    state = state_.lock();
    entry = entry_.lock();

    if (!entry)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    if (!state)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        const auto it = state->subscriptions.find(entry->id);
        if (it == state->subscriptions.end() || it->second.get() != entry.get())
        {
            return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
        }
    }

    return S_OK;
}

STDMETHODIMP CNostrSubscription::get_Id(BSTR* value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    *value = nullptr;

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<RelaySessionData::SubscriptionEntry> entry;
    HRESULT hr = ResolveStateAndEntry(state, entry);
    if (FAILED(hr))
    {
        return hr;
    }

    return CComBSTR(entry->id.c_str()).CopyTo(value);
}

STDMETHODIMP CNostrSubscription::get_Status(SubscriptionStatus* value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<RelaySessionData::SubscriptionEntry> entry;
    HRESULT hr = ResolveStateAndEntry(state, entry);
    if (FAILED(hr))
    {
        return hr;
    }

    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        *value = entry->status;
    }

    return S_OK;
}

STDMETHODIMP CNostrSubscription::get_Filters(SAFEARRAY** value)
{
    if (!value)
    {
        return hresults::PointerRequired();
    }

    *value = nullptr;

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<RelaySessionData::SubscriptionEntry> entry;
    HRESULT hr = ResolveStateAndEntry(state, entry);
    if (FAILED(hr))
    {
        return hr;
    }

    std::vector<ATL::CComPtr<IDispatch>> snapshot;
    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        snapshot = entry->originalFilters;
    }

    SAFEARRAY* array = nullptr;
    hr = CreateFilterSafeArray(snapshot, &array);
    if (FAILED(hr))
    {
        return hr;
    }

    *value = array;
    return S_OK;
}

STDMETHODIMP CNostrSubscription::UpdateFilters(SAFEARRAY* filters)
{
    if (!filters)
    {
        return hresults::InvalidArgument();
    }

    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<RelaySessionData::SubscriptionEntry> entry;
    HRESULT hr = ResolveStateAndEntry(state, entry);
    if (FAILED(hr))
    {
        return hr;
    }

    if (!state->serializer || !state->webSocket)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    std::vector<NostrJsonSerializer::FilterData> parsedFilters;
    hr = state->serializer->ReadFiltersFromSafeArray(filters, parsedFilters);
    if (FAILED(hr))
    {
        return hr;
    }

    if (parsedFilters.empty())
    {
        return hresults::InvalidArgument();
    }

    std::vector<ATL::CComPtr<IDispatch>> originalFilters;
    hr = CopyFilterDispatchArray(filters, originalFilters);
    if (FAILED(hr))
    {
        return hr;
    }

    double windowSeconds = 0.0;
    bool hasWindow = false;
    double lastEventTimestamp = 0.0;
    bool hasLastEvent = false;

    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        if (entry->options.autoRequeryWindowSeconds && entry->options.autoRequeryWindowSeconds.value() > 0.0)
        {
            hasWindow = true;
            windowSeconds = entry->options.autoRequeryWindowSeconds.value();
        }
        if (entry->lastEventTimestamp)
        {
            hasLastEvent = true;
            lastEventTimestamp = entry->lastEventTimestamp.value();
        }
    }

    std::vector<NostrJsonSerializer::FilterData> adjustedFilters = parsedFilters;
    if (hasWindow && hasLastEvent)
    {
        double sinceBase = lastEventTimestamp - windowSeconds;
        if (sinceBase < 0.0)
        {
            sinceBase = 0.0;
        }

        for (auto& filter : adjustedFilters)
        {
            if (!filter.since || filter.since.value() < sinceBase)
            {
                filter.since = sinceBase;
            }
        }
    }

    NostrJsonSerializer::RequestMessage request;
    request.subscriptionId = entry->id;
    request.filters = std::move(adjustedFilters);

    const auto payload = state->serializer->SerializeRequest(request);
    hr = state->webSocket->SendText(payload, true);
    if (FAILED(hr))
    {
        return hr;
    }

    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        entry->filters = std::move(request.filters);
        entry->originalFilters = std::move(originalFilters);
        entry->status = SubscriptionStatus_Pending;
    }

    return S_OK;
}

STDMETHODIMP CNostrSubscription::Close()
{
    std::shared_ptr<RelaySessionData> state;
    std::shared_ptr<RelaySessionData::SubscriptionEntry> entry;
    HRESULT hr = ResolveStateAndEntry(state, entry);
    if (FAILED(hr))
    {
        return hr;
    }

    if (!state->serializer || !state->webSocket)
    {
        return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
    }

    bool alreadyClosed = false;
    bool alreadyRequested = false;
    SubscriptionStatus previousStatus = SubscriptionStatus_Pending;

    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        previousStatus = entry->status;
        if (entry->status == SubscriptionStatus_Closed)
        {
            alreadyClosed = true;
        }
        else if (entry->closeRequested)
        {
            alreadyRequested = true;
        }
        else
        {
            entry->closeRequested = true;
            entry->status = SubscriptionStatus_Draining;
        }
    }

    if (alreadyClosed || alreadyRequested)
    {
        return S_OK;
    }

    const std::wstring subscriptionId = entry->id;
    const auto payload = state->serializer->SerializeClose(subscriptionId);
    hr = state->webSocket->SendText(payload, true);
    if (FAILED(hr))
    {
        std::lock_guard<std::mutex> guard(state->subscriptionMutex);
        entry->closeRequested = false;
        entry->status = previousStatus;
        return hr;
    }

    return S_OK;
}
HRESULT CNostrSigner::FinalConstruct() noexcept
{
    return S_OK;
}

void CNostrSigner::FinalRelease() noexcept
{
}
