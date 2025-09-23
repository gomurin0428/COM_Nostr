#include "pch.h"

#include "runtime/NativeClientResources.h"

#include <utility>

namespace com::nostr::native
{
    NativeClientResources::NativeClientResources(ClientRuntimeOptions options,
                                                 HttpClientFactory httpFactory,
                                                 WebSocketFactory webSocketFactory,
                                                 SerializerPtr serializer)
        : options_(std::move(options))
        , httpFactory_(std::move(httpFactory))
        , webSocketFactory_(std::move(webSocketFactory))
        , serializer_(std::move(serializer))
    {
    }

    const ClientRuntimeOptions& NativeClientResources::Options() const noexcept
    {
        return options_;
    }

    std::unique_ptr<NativeHttpClient> NativeClientResources::CreateHttpClient() const
    {
        if (!httpFactory_)
        {
            return nullptr;
        }

        return httpFactory_();
    }

    std::unique_ptr<INativeWebSocket> NativeClientResources::CreateWebSocket() const
    {
        if (!webSocketFactory_)
        {
            return nullptr;
        }

        return webSocketFactory_();
    }

    NativeClientResources::SerializerPtr NativeClientResources::Serializer() const noexcept
    {
        return serializer_;
    }
}
