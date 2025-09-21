using System;

namespace COM_Nostr.Internal;

internal sealed class ClientRuntimeOptions
{
    public TimeSpan? ConnectTimeout { get; init; }

    public TimeSpan? SendTimeout { get; init; }

    public TimeSpan? ReceiveTimeout { get; init; }

    public string? UserAgent { get; init; }

    public string? WebSocketFactoryProgId { get; init; }
}
