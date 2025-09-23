#pragma once

#include "runtime/ClientRuntimeOptions.h"
#include "runtime/NativeHttpClient.h"
#include "runtime/INativeWebSocket.h"
#include "NostrJsonSerializer.h"

#include <functional>
#include <memory>

namespace com::nostr::native
{
    class NativeClientResources
    {
    public:
        using HttpClientFactory = std::function<std::unique_ptr<NativeHttpClient>()>;
        using WebSocketFactory = std::function<std::unique_ptr<INativeWebSocket>()>;
        using SerializerPtr = std::shared_ptr<NostrJsonSerializer>;

        NativeClientResources(ClientRuntimeOptions options,
                              HttpClientFactory httpFactory,
                              WebSocketFactory webSocketFactory,
                              SerializerPtr serializer);

        [[nodiscard]] const ClientRuntimeOptions& Options() const noexcept;
        [[nodiscard]] std::unique_ptr<NativeHttpClient> CreateHttpClient() const;
        [[nodiscard]] std::unique_ptr<INativeWebSocket> CreateWebSocket() const;
        [[nodiscard]] SerializerPtr Serializer() const noexcept;

    private:
        ClientRuntimeOptions options_;
        HttpClientFactory httpFactory_;
        WebSocketFactory webSocketFactory_;
        SerializerPtr serializer_;
    };
}
