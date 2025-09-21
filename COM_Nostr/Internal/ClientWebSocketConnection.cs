using System;
using System.Net.WebSockets;
using System.Threading;
using System.Threading.Tasks;

namespace COM_Nostr.Internal;

internal sealed class ClientWebSocketConnection : IWebSocketConnection
{
    private readonly ClientWebSocket _socket;

    public ClientWebSocketConnection(ClientWebSocket socket)
    {
        _socket = socket ?? throw new ArgumentNullException(nameof(socket));
    }

    public WebSocketState State => _socket.State;

    public Task ConnectAsync(Uri uri, CancellationToken cancellationToken)
    {
        if (uri is null)
        {
            throw new ArgumentNullException(nameof(uri));
        }

        return _socket.ConnectAsync(uri, cancellationToken);
    }

    public Task SendAsync(ReadOnlyMemory<byte> payload, WebSocketMessageType messageType, bool endOfMessage, CancellationToken cancellationToken)
        => _socket.SendAsync(payload, messageType, endOfMessage, cancellationToken).AsTask();

    public ValueTask<ValueWebSocketReceiveResult> ReceiveAsync(Memory<byte> buffer, CancellationToken cancellationToken)
        => _socket.ReceiveAsync(buffer, cancellationToken);

    public Task CloseAsync(WebSocketCloseStatus closeStatus, string? statusDescription, CancellationToken cancellationToken)
        => _socket.CloseAsync(closeStatus, statusDescription, cancellationToken);

    public Task CloseOutputAsync(WebSocketCloseStatus closeStatus, string? statusDescription, CancellationToken cancellationToken)
        => _socket.CloseOutputAsync(closeStatus, statusDescription, cancellationToken);

    public ValueTask DisposeAsync()
    {
        _socket.Dispose();
        return ValueTask.CompletedTask;
    }
}
