using System;
using System.Net.WebSockets;
using System.Threading;
using System.Threading.Tasks;

namespace COM_Nostr.Internal;

internal interface IWebSocketConnection : IAsyncDisposable
{
    WebSocketState State { get; }

    Task ConnectAsync(Uri uri, CancellationToken cancellationToken);

    Task SendAsync(ReadOnlyMemory<byte> payload, WebSocketMessageType messageType, bool endOfMessage, CancellationToken cancellationToken);

    ValueTask<ValueWebSocketReceiveResult> ReceiveAsync(Memory<byte> buffer, CancellationToken cancellationToken);

    Task CloseAsync(WebSocketCloseStatus closeStatus, string? statusDescription, CancellationToken cancellationToken);

    Task CloseOutputAsync(WebSocketCloseStatus closeStatus, string? statusDescription, CancellationToken cancellationToken);
}
