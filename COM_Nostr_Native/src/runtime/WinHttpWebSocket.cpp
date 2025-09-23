#include "pch.h"

#include "runtime/WinHttpWebSocket.h"

#include "NostrHResults.h"
#include "ComValueHelpers.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
#include <winhttp.h>

namespace com::nostr::native
{
    namespace
    {
        constexpr DWORD kDefaultReceiveBufferSize = 4096;
        constexpr int kNoTimeout = -1;
#ifdef WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE
        constexpr WINHTTP_WEB_SOCKET_BUFFER_TYPE kPingBufferType = WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE;
        constexpr WINHTTP_WEB_SOCKET_BUFFER_TYPE kPongBufferType = WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE;
#else
        constexpr WINHTTP_WEB_SOCKET_BUFFER_TYPE kPingBufferType = static_cast<WINHTTP_WEB_SOCKET_BUFFER_TYPE>(5);
        constexpr WINHTTP_WEB_SOCKET_BUFFER_TYPE kPongBufferType = static_cast<WINHTTP_WEB_SOCKET_BUFFER_TYPE>(6);
#endif
#ifdef WINHTTP_WEB_SOCKET_ABORTED_CLOSE_STATUS
        constexpr USHORT kAbortedCloseStatus = WINHTTP_WEB_SOCKET_ABORTED_CLOSE_STATUS;
#else
        constexpr USHORT kAbortedCloseStatus = 1006;
#endif

        int ToTimeoutValue(const std::optional<ClientRuntimeOptions::Duration>& value)
        {
            if (!value)
            {
                return kNoTimeout;
            }

            const auto milliseconds = value->count();
            const auto maxInt = (std::numeric_limits<int>::max)();
            if (milliseconds > static_cast<long long>(maxInt))
            {
                return maxInt;
            }

            return static_cast<int>(milliseconds);
        }

        WINHTTP_WEB_SOCKET_BUFFER_TYPE NormalizeType(WINHTTP_WEB_SOCKET_BUFFER_TYPE type)
        {
            switch (type)
            {
            case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
                return WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
            case WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
                return WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
            default:
                return type;
            }
        }
    }

    WinHttpWebSocket::InternetHandle::InternetHandle(HINTERNET handle) noexcept
        : handle_(handle)
    {
    }

    WinHttpWebSocket::InternetHandle::InternetHandle(InternetHandle&& other) noexcept
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    WinHttpWebSocket::InternetHandle& WinHttpWebSocket::InternetHandle::operator=(InternetHandle&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }

        return *this;
    }

    WinHttpWebSocket::InternetHandle::~InternetHandle()
    {
        Reset();
    }

    void WinHttpWebSocket::InternetHandle::Reset(HINTERNET handle) noexcept
    {
        if (handle_)
        {
            WinHttpCloseHandle(handle_);
        }
        handle_ = handle;
    }

    HINTERNET WinHttpWebSocket::InternetHandle::Get() const noexcept
    {
        return handle_;
    }

    WinHttpWebSocket::InternetHandle::operator bool() const noexcept
    {
        return handle_ != nullptr;
    }
    WinHttpWebSocket::WinHttpWebSocket()
    {
        messageEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    }

    WinHttpWebSocket::~WinHttpWebSocket()
    {
        ShutdownReceiveLoop(true);
        if (messageEvent_)
        {
            CloseHandle(messageEvent_);
            messageEvent_ = nullptr;
        }
        if (receiveWork_)
        {
            CloseThreadpoolWork(receiveWork_);
            receiveWork_ = nullptr;
        }
    }
    HRESULT WinHttpWebSocket::Connect(const std::wstring& url, const ClientRuntimeOptions& options)
    {
        if (!messageEvent_)
        {
            return E_OUTOFMEMORY;
        }

        ShutdownReceiveLoop(true);
        stopRequested_.store(false, std::memory_order_release);
        ResetEvent(messageEvent_);

        HRESULT hr = InitializeHandshake(url, options);
        if (FAILED(hr))
        {
            ShutdownReceiveLoop(false);
            return hr;
        }

        if (!receiveWork_)
        {
            receiveWork_ = CreateThreadpoolWork(&WinHttpWebSocket::ReceiveWorkCallback, this, nullptr);
            if (!receiveWork_)
            {
                return hresults::FromWin32(GetLastError());
            }
        }

        StartReceiveLoop();
        return S_OK;
    }
    HRESULT WinHttpWebSocket::InitializeHandshake(const std::wstring& url, const ClientRuntimeOptions& options)
    {
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port = 0;
        bool useTls = false;
        HRESULT hr = ParseUrl(url, host, path, port, useTls);
        if (FAILED(hr))
        {
            return hr;
        }

        const std::wstring userAgent = options.UserAgent().empty() ? std::wstring(L"COM_Nostr/1.0") : options.UserAgent();
        session_.Reset(WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session_)
        {
            return hresults::FromWin32(GetLastError());
        }

        const int resolveTimeout = kNoTimeout;
        const int connectTimeout = ToTimeoutValue(options.ConnectTimeout());
        const int sendTimeout = ToTimeoutValue(options.SendTimeout());
        const int receiveTimeout = ToTimeoutValue(options.ReceiveTimeout());
        if (!WinHttpSetTimeouts(session_.Get(), resolveTimeout, connectTimeout, sendTimeout, receiveTimeout))
        {
            return hresults::FromWin32(GetLastError());
        }

        connection_.Reset(WinHttpConnect(session_.Get(), host.c_str(), port, 0));
        if (!connection_)
        {
            return hresults::FromWin32(GetLastError());
        }

        DWORD flags = WINHTTP_FLAG_REFRESH;
        if (useTls)
        {
            flags |= WINHTTP_FLAG_SECURE;
        }

        request_.Reset(WinHttpOpenRequest(connection_.Get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
        if (!request_)
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpSetTimeouts(request_.Get(), resolveTimeout, connectTimeout, sendTimeout, receiveTimeout))
        {
            return hresults::FromWin32(GetLastError());
        }

        static constexpr wchar_t protocolHeader[] = L"Sec-WebSocket-Protocol: nostr\r\n";
        if (!WinHttpAddRequestHeaders(request_.Get(), protocolHeader, static_cast<DWORD>(wcslen(protocolHeader)), WINHTTP_ADDREQ_FLAG_ADD))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpSetOption(request_.Get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpSendRequest(request_.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpReceiveResponse(request_.Get(), nullptr))
        {
            return hresults::FromWin32(GetLastError());
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(request_.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (statusCode != HTTP_STATUS_SWITCH_PROTOCOLS)
        {
            return hresults::FromWin32(ERROR_WINHTTP_INVALID_SERVER_RESPONSE);
        }

        InternetHandle upgraded(WinHttpWebSocketCompleteUpgrade(request_.Get(), 0));
        if (!upgraded)
        {
            return hresults::FromWin32(GetLastError());
        }

        request_.Reset();
        webSocket_ = std::move(upgraded);
        return S_OK;
    }
    HRESULT WinHttpWebSocket::ParseUrl(const std::wstring& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& useTls) const
    {
        host.clear();
        path.clear();
        port = 0;
        useTls = false;

        if (url.empty())
        {
            return hresults::InvalidArgument();
        }

        std::wstring urlCopy = url;
        if (urlCopy.rfind(L"ws://", 0) == 0)
        {
            urlCopy.replace(0, 5, L"http://");
        }
        else if (urlCopy.rfind(L"wss://", 0) == 0)
        {
            urlCopy.replace(0, 6, L"https://");
        }

        URL_COMPONENTS components;
        ZeroMemory(&components, sizeof(components));
        components.dwStructSize = sizeof(components);
        components.dwSchemeLength = static_cast<DWORD>(-1);
        components.dwHostNameLength = static_cast<DWORD>(-1);
        components.dwUrlPathLength = static_cast<DWORD>(-1);
        components.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(urlCopy.data(), static_cast<DWORD>(urlCopy.length()), 0, &components))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (components.lpszHostName && components.dwHostNameLength > 0)
        {
            host.assign(components.lpszHostName, components.dwHostNameLength);
        }

        if (host.empty())
        {
            return hresults::InvalidArgument();
        }

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

        port = components.nPort;
        switch (components.nScheme)
        {
        case INTERNET_SCHEME_HTTP:
            if (port == 0)
            {
                port = INTERNET_DEFAULT_HTTP_PORT;
            }
            break;
        case INTERNET_SCHEME_HTTPS:
            useTls = true;
            if (port == 0)
            {
                port = INTERNET_DEFAULT_HTTPS_PORT;
            }
            break;
        default:
        {
            std::wstring scheme;
            if (components.lpszScheme && components.dwSchemeLength > 0)
            {
                scheme.assign(components.lpszScheme, components.dwSchemeLength);
                std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::towlower);
            }

            if (scheme == L"ws")
            {
                if (port == 0)
                {
                    port = INTERNET_DEFAULT_HTTP_PORT;
                }
            }
            else if (scheme == L"wss")
            {
                useTls = true;
                if (port == 0)
                {
                    port = INTERNET_DEFAULT_HTTPS_PORT;
                }
            }
            else
            {
                return hresults::InvalidArgument();
            }
            break;
        }
        }

        return S_OK;
    }
    void WinHttpWebSocket::StartReceiveLoop()
    {
        if (receiveWork_)
        {
            SubmitThreadpoolWork(receiveWork_);
        }
    }

    void CALLBACK WinHttpWebSocket::ReceiveWorkCallback(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WORK)
    {
        auto* self = static_cast<WinHttpWebSocket*>(context);
        if (self)
        {
            self->ReceiveLoop();
        }
    }

    void WinHttpWebSocket::ReceiveLoop()
    {
        if (!webSocket_)
        {
            return;
        }

        std::vector<uint8_t> buffer(kDefaultReceiveBufferSize);
        std::vector<uint8_t> accumulator;
        std::optional<WINHTTP_WEB_SOCKET_BUFFER_TYPE> pendingType;

        while (!stopRequested_.load(std::memory_order_acquire))
        {
            DWORD bytesRead = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE frameType = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
            const HRESULT hr = WinHttpWebSocketReceive(webSocket_.Get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, &frameType);
            if (FAILED(hr))
            {
                if (hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_TIMEOUT))
                {
                    continue;
                }

                break;
            }

            if (frameType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
            {
                stopRequested_.store(true, std::memory_order_release);
                break;
            }

            if (frameType == kPingBufferType)
            {
                WinHttpWebSocketSend(webSocket_.Get(), kPongBufferType, buffer.data(), bytesRead);
                continue;
            }

            if (frameType == kPongBufferType)
            {
                continue;
            }

            if (bytesRead > 0)
            {
                accumulator.insert(accumulator.end(), buffer.begin(), buffer.begin() + bytesRead);
            }

            if (!pendingType)
            {
                pendingType = NormalizeType(frameType);
            }

            const bool finalFrame = frameType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                                    frameType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;

            if (finalFrame)
            {
                NativeWebSocketMessage message;
                message.bufferType = pendingType.value_or(NormalizeType(frameType));
                message.endOfMessage = true;
                message.payload = std::move(accumulator);
                EnqueueMessage(std::move(message));
                accumulator.clear();
                pendingType.reset();
            }
        }
    }

    void WinHttpWebSocket::EnqueueMessage(NativeWebSocketMessage&& message)
    {
        std::lock_guard<std::mutex> guard(queueMutex_);
        messageQueue_.push_back(std::move(message));
        SetEvent(messageEvent_);
    }

    void WinHttpWebSocket::ShutdownReceiveLoop(bool waitForCallbacks)
    {
        stopRequested_.store(true, std::memory_order_release);

        if (webSocket_)
        {
            WinHttpWebSocketClose(webSocket_.Get(), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        }

        if (receiveWork_ && waitForCallbacks)
        {
            WaitForThreadpoolWorkCallbacks(receiveWork_, TRUE);
        }

        webSocket_.Reset();
        request_.Reset();
        connection_.Reset();
        session_.Reset();

        {
            std::lock_guard<std::mutex> guard(queueMutex_);
            messageQueue_.clear();
        }

        ResetEvent(messageEvent_);
    }

    HRESULT WinHttpWebSocket::EnsureWebSocketHandle() const
    {
        if (!webSocket_)
        {
            return hresults::E_NOSTR_RELAY_NOT_CONNECTED;
        }

        return S_OK;
    }

    HRESULT WinHttpWebSocket::SendInternal(WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType, const std::vector<uint8_t>& payload, bool endOfMessage) const
    {
        HRESULT hr = EnsureWebSocketHandle();
        if (FAILED(hr))
        {
            return hr;
        }

        WINHTTP_WEB_SOCKET_BUFFER_TYPE typeToSend = bufferType;
        if (!endOfMessage)
        {
            if (bufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
            {
                typeToSend = WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE;
            }
            else if (bufferType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)
            {
                typeToSend = WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE;
            }
        }

        hr = WinHttpWebSocketSend(webSocket_.Get(), typeToSend, const_cast<uint8_t*>(payload.data()), static_cast<DWORD>(payload.size()));
        if (FAILED(hr))
        {
            return hresults::WebSocketFailure();
        }

        if (!endOfMessage)
        {
            return S_OK;
        }

        return S_OK;
    }
    HRESULT WinHttpWebSocket::SendText(const std::vector<uint8_t>& payload, bool endOfMessage)
    {
        return SendInternal(WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, payload, endOfMessage);
    }

    HRESULT WinHttpWebSocket::SendBinary(const std::vector<uint8_t>& payload, bool endOfMessage)
    {
        return SendInternal(WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, payload, endOfMessage);
    }

    HRESULT WinHttpWebSocket::Receive(DWORD timeoutMilliseconds, NativeWebSocketMessage& message)
    {
        if (!messageEvent_)
        {
            return hresults::FromWin32(ERROR_INVALID_HANDLE);
        }

        const DWORD waitResult = WaitForSingleObject(messageEvent_, timeoutMilliseconds);
        if (waitResult == WAIT_TIMEOUT)
        {
            return hresults::Timeout();
        }
        if (waitResult == WAIT_FAILED)
        {
            return hresults::FromWin32(GetLastError());
        }

        std::lock_guard<std::mutex> guard(queueMutex_);
        if (messageQueue_.empty())
        {
            ResetEvent(messageEvent_);
            return hresults::FromWin32(ERROR_NO_MORE_ITEMS);
        }

        message = std::move(messageQueue_.front());
        messageQueue_.pop_front();
        if (messageQueue_.empty())
        {
            ResetEvent(messageEvent_);
        }

        return S_OK;
    }

    HRESULT WinHttpWebSocket::Close(USHORT closeStatus, const std::wstring& reason)
    {
        HRESULT hr = EnsureWebSocketHandle();
        if (FAILED(hr))
        {
            return hr;
        }

        const std::string reasonUtf8 = WideToUtf8(reason);
        hr = WinHttpWebSocketClose(webSocket_.Get(), closeStatus, reasonUtf8.empty() ? nullptr : reinterpret_cast<void*>(const_cast<char*>(reasonUtf8.data())), static_cast<DWORD>(reasonUtf8.size()));
        if (FAILED(hr))
        {
            return hresults::WebSocketFailure();
        }

        stopRequested_.store(true, std::memory_order_release);
        return S_OK;
    }

    HRESULT WinHttpWebSocket::CloseOutput(USHORT closeStatus, const std::wstring& reason)
    {
        HRESULT hr = EnsureWebSocketHandle();
        if (FAILED(hr))
        {
            return hr;
        }

        const std::string reasonUtf8 = WideToUtf8(reason);
        hr = WinHttpWebSocketShutdown(webSocket_.Get(), closeStatus, reasonUtf8.empty() ? nullptr : reinterpret_cast<void*>(const_cast<char*>(reasonUtf8.data())), static_cast<DWORD>(reasonUtf8.size()));
        if (FAILED(hr))
        {
            return hresults::WebSocketFailure();
        }

        return S_OK;
    }

    HRESULT WinHttpWebSocket::Abort()
    {
        HRESULT hr = EnsureWebSocketHandle();
        if (FAILED(hr))
        {
            return hr;
        }

        WinHttpWebSocketClose(webSocket_.Get(), kAbortedCloseStatus, nullptr, 0);
        stopRequested_.store(true, std::memory_order_release);
        return S_OK;
    }
}
