using System;
using System.Net.Http;

namespace COM_Nostr.Internal;

internal sealed class NostrClientResources
{
    public NostrClientResources(
        ClientRuntimeOptions options,
        Func<HttpClient> httpClientFactory,
        Func<IWebSocketConnection> webSocketFactory,
        NostrJsonSerializer serializer)
    {
        Options = options ?? throw new ArgumentNullException(nameof(options));
        HttpClientFactory = httpClientFactory ?? throw new ArgumentNullException(nameof(httpClientFactory));
        WebSocketFactory = webSocketFactory ?? throw new ArgumentNullException(nameof(webSocketFactory));
        Serializer = serializer ?? throw new ArgumentNullException(nameof(serializer));
    }

    public ClientRuntimeOptions Options { get; }

    public Func<HttpClient> HttpClientFactory { get; }

    public Func<IWebSocketConnection> WebSocketFactory { get; }

    public NostrJsonSerializer Serializer { get; }
}
