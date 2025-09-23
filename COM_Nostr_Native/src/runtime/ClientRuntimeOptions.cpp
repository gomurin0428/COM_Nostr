#include "pch.h"

#include "runtime/ClientRuntimeOptions.h"

#include <utility>

namespace com::nostr::native
{
    void ClientRuntimeOptions::SetConnectTimeout(Duration value) noexcept
    {
        connectTimeout_ = value;
    }

    void ClientRuntimeOptions::SetSendTimeout(Duration value) noexcept
    {
        sendTimeout_ = value;
    }

    void ClientRuntimeOptions::SetReceiveTimeout(Duration value) noexcept
    {
        receiveTimeout_ = value;
    }

    void ClientRuntimeOptions::SetUserAgent(std::wstring value)
    {
        userAgent_ = std::move(value);
    }

    void ClientRuntimeOptions::SetWebSocketFactoryProgId(std::wstring value)
    {
        webSocketFactoryProgId_ = std::move(value);
    }

    const std::optional<ClientRuntimeOptions::Duration>& ClientRuntimeOptions::ConnectTimeout() const noexcept
    {
        return connectTimeout_;
    }

    const std::optional<ClientRuntimeOptions::Duration>& ClientRuntimeOptions::SendTimeout() const noexcept
    {
        return sendTimeout_;
    }

    const std::optional<ClientRuntimeOptions::Duration>& ClientRuntimeOptions::ReceiveTimeout() const noexcept
    {
        return receiveTimeout_;
    }

    const std::wstring& ClientRuntimeOptions::UserAgent() const noexcept
    {
        return userAgent_;
    }

    const std::wstring& ClientRuntimeOptions::WebSocketFactoryProgId() const noexcept
    {
        return webSocketFactoryProgId_;
    }
}
