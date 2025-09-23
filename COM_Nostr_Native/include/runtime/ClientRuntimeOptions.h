#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace com::nostr::native
{
    class ClientRuntimeOptions
    {
    public:
        using Duration = std::chrono::milliseconds;

        void SetConnectTimeout(Duration value) noexcept;
        void SetSendTimeout(Duration value) noexcept;
        void SetReceiveTimeout(Duration value) noexcept;
        void SetUserAgent(std::wstring value);
        void SetWebSocketFactoryProgId(std::wstring value);

        [[nodiscard]] const std::optional<Duration>& ConnectTimeout() const noexcept;
        [[nodiscard]] const std::optional<Duration>& SendTimeout() const noexcept;
        [[nodiscard]] const std::optional<Duration>& ReceiveTimeout() const noexcept;
        [[nodiscard]] const std::wstring& UserAgent() const noexcept;
        [[nodiscard]] const std::wstring& WebSocketFactoryProgId() const noexcept;

    private:
        std::optional<Duration> connectTimeout_;
        std::optional<Duration> sendTimeout_;
        std::optional<Duration> receiveTimeout_;
        std::wstring userAgent_;
        std::wstring webSocketFactoryProgId_;
    };
}
