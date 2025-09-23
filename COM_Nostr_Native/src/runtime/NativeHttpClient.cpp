#include "pch.h"

#include "runtime/NativeHttpClient.h"

#include "NostrHResults.h"
#include "ComValueHelpers.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cwchar>
#include <limits>
#include <winhttp.h>

namespace com::nostr::native
{
    namespace
    {
        class InternetHandle
        {
        public:
            InternetHandle() noexcept = default;
            explicit InternetHandle(HINTERNET handle) noexcept : handle_(handle) {}
            InternetHandle(const InternetHandle&) = delete;
            InternetHandle& operator=(const InternetHandle&) = delete;
            InternetHandle(InternetHandle&& other) noexcept : handle_(other.handle_)
            {
                other.handle_ = nullptr;
            }
            InternetHandle& operator=(InternetHandle&& other) noexcept
            {
                if (this != &other)
                {
                    Reset();
                    handle_ = other.handle_;
                    other.handle_ = nullptr;
                }
                return *this;
            }
            ~InternetHandle()
            {
                Reset();
            }

            void Reset(HINTERNET handle = nullptr) noexcept
            {
                if (handle_)
                {
                    WinHttpCloseHandle(handle_);
                }
                handle_ = handle;
            }

            [[nodiscard]] HINTERNET Get() const noexcept
            {
                return handle_;
            }

            explicit operator bool() const noexcept
            {
                return handle_ != nullptr;
            }

        private:
            HINTERNET handle_ = nullptr;
        };

        constexpr int kNoTimeout = -1;

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

        HRESULT QueryContentType(HINTERNET request, std::wstring& contentType)
        {
            DWORD size = 0;
            if (!WinHttpQueryHeaders(request,
                                      WINHTTP_QUERY_CONTENT_TYPE,
                                      WINHTTP_HEADER_NAME_BY_INDEX,
                                      nullptr,
                                      &size,
                                      WINHTTP_NO_HEADER_INDEX))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_WINHTTP_HEADER_NOT_FOUND)
                {
                    contentType.clear();
                    return S_OK;
                }

                if (error != ERROR_INSUFFICIENT_BUFFER)
                {
                    return hresults::FromWin32(error);
                }
            }

            std::wstring buffer;
            buffer.resize(size / sizeof(wchar_t));
            if (!WinHttpQueryHeaders(request,
                                      WINHTTP_QUERY_CONTENT_TYPE,
                                      WINHTTP_HEADER_NAME_BY_INDEX,
                                      buffer.data(),
                                      &size,
                                      WINHTTP_NO_HEADER_INDEX))
            {
                return hresults::FromWin32(GetLastError());
            }

            if (!buffer.empty() && buffer.back() == L'\0')
            {
                buffer.pop_back();
            }

            contentType.assign(buffer.begin(), buffer.end());
            return S_OK;
        }

        bool IsSupportedContentType(const std::wstring& mediaType)
        {
            if (mediaType.empty())
            {
                return true;
            }

            const std::wstring lowerMediaType = [&]() {
                std::wstring copy = mediaType;
                std::transform(copy.begin(), copy.end(), copy.begin(), ::towlower);
                return copy;
            }();

            return lowerMediaType == L"application/nostr+json" || lowerMediaType == L"application/json";
        }

        std::wstring DefaultUserAgent()
        {
            return std::wstring(L"COM_Nostr/1.0");
        }
    }
    NativeHttpClient::NativeHttpClient(ClientRuntimeOptions options)
        : options_(std::move(options))
    {
    }

    HRESULT NativeHttpClient::FetchRelayInformation(const std::wstring& url, RelayInformation& information) const
    {
        information = RelayInformation{};
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

        std::wstring host;
        if (components.lpszHostName && components.dwHostNameLength > 0)
        {
            host.assign(components.lpszHostName, components.dwHostNameLength);
        }

        if (host.empty())
        {
            return hresults::InvalidArgument();
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

        INTERNET_PORT port = components.nPort;
        bool useTls = false;
        if (components.nScheme == INTERNET_SCHEME_HTTP)
        {
            if (port == 0)
            {
                port = INTERNET_DEFAULT_HTTP_PORT;
            }
        }
        else if (components.nScheme == INTERNET_SCHEME_HTTPS)
        {
            useTls = true;
            if (port == 0)
            {
                port = INTERNET_DEFAULT_HTTPS_PORT;
            }
        }
        else
        {
            std::wstring scheme;
            if (components.lpszScheme && components.dwSchemeLength > 0)
            {
                scheme.assign(components.lpszScheme, components.dwSchemeLength);
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
        }

        return PerformGet(host, port, path, useTls, information);
    }
    HRESULT NativeHttpClient::PerformGet(const std::wstring& host,
                                         INTERNET_PORT port,
                                         const std::wstring& path,
                                         bool useTls,
                                         RelayInformation& information) const
    {
        const std::wstring userAgent = options_.UserAgent().empty() ? DefaultUserAgent() : options_.UserAgent();
        InternetHandle session(WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session)
        {
            return hresults::FromWin32(GetLastError());
        }

        const int resolveTimeout = kNoTimeout;
        const int connectTimeout = ToTimeoutValue(options_.ConnectTimeout());
        const int sendTimeout = ToTimeoutValue(options_.SendTimeout());
        const int receiveTimeout = ToTimeoutValue(options_.ReceiveTimeout());
        if (!WinHttpSetTimeouts(session.Get(), resolveTimeout, connectTimeout, sendTimeout, receiveTimeout))
        {
            return hresults::FromWin32(GetLastError());
        }

        InternetHandle connection(WinHttpConnect(session.Get(), host.c_str(), port, 0));
        if (!connection)
        {
            return hresults::FromWin32(GetLastError());
        }

        DWORD flags = WINHTTP_FLAG_REFRESH;
        if (useTls)
        {
            flags |= WINHTTP_FLAG_SECURE;
        }

        InternetHandle request(WinHttpOpenRequest(connection.Get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
        if (!request)
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpSetTimeouts(request.Get(), resolveTimeout, connectTimeout, sendTimeout, receiveTimeout))
        {
            return hresults::FromWin32(GetLastError());
        }

        static constexpr wchar_t acceptHeader[] = L"Accept: application/nostr+json\r\nAccept: application/json\r\n";
        if (!WinHttpAddRequestHeaders(request.Get(), acceptHeader, static_cast<DWORD>(wcslen(acceptHeader)), WINHTTP_ADDREQ_FLAG_ADD))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpSendRequest(request.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (!WinHttpReceiveResponse(request.Get(), nullptr))
        {
            return hresults::FromWin32(GetLastError());
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        if (!WinHttpQueryHeaders(request.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX))
        {
            return hresults::FromWin32(GetLastError());
        }

        if (statusCode != HTTP_STATUS_OK)
        {
            return hresults::FromWin32(ERROR_WINHTTP_INVALID_SERVER_RESPONSE);
        }

        std::wstring mediaType;
        HRESULT hr = QueryContentType(request.Get(), mediaType);
        if (FAILED(hr))
        {
            return hr;
        }

        if (!IsSupportedContentType(mediaType))
        {
            return hresults::InvalidArgument();
        }

        std::string body;
        for (;;)
        {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request.Get(), &available))
            {
                return hresults::FromWin32(GetLastError());
            }

            if (available == 0)
            {
                break;
            }

            std::string chunk;
            chunk.resize(available);
            DWORD bytesRead = 0;
            if (!WinHttpReadData(request.Get(), chunk.data(), available, &bytesRead))
            {
                return hresults::FromWin32(GetLastError());
            }

            body.append(chunk.data(), bytesRead);

            if (bytesRead < available)
            {
                // No more data available in this iteration.
                continue;
            }
        }

        try
        {
            const auto json = nlohmann::json::parse(body);
            if (json.is_object())
            {
                const auto& supported = json.find("supported_nips");
                if (supported != json.end() && supported->is_array())
                {
                    for (const auto& element : *supported)
                    {
                        if (element.is_number_integer())
                        {
                            information.supportedNips.push_back(element.get<int>());
                        }
                    }
                }
            }
        }
        catch (const std::exception&)
        {
            return hresults::InvalidArgument();
        }

        information.metadataJson = Utf8ToWide(body);
        return S_OK;
    }
}
