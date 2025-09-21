using System;
using System.Net.WebSockets;

namespace COM_Nostr.Internal;

internal static class WebSocketFactoryResolver
{

    public static Func<IWebSocketConnection> Create(ClientRuntimeOptions options)
    {
        if (options is null)
        {
            throw new ArgumentNullException(nameof(options));
        }

        if (string.IsNullOrWhiteSpace(options.WebSocketFactoryProgId))
        {
            return () => CreateDefaultClientWebSocket(options.UserAgent);
        }

        var factoryType = Type.GetTypeFromProgID(options.WebSocketFactoryProgId, throwOnError: false);
        if (factoryType is null)
        {
            throw HResults.Exception($"WebSocket factory ProgID '{options.WebSocketFactoryProgId}' is not registered.", HResults.E_CLASSNOTREGISTERED);
        }

        if (typeof(IWebSocketFactory).IsAssignableFrom(factoryType))
        {
            var factoryInstance = (IWebSocketFactory?)Activator.CreateInstance(factoryType)
                ?? throw new InvalidOperationException($"Unable to create instance from ProgID '{options.WebSocketFactoryProgId}'.");

            return () => Adapt(factoryInstance.Create(), options.UserAgent, options.WebSocketFactoryProgId);
        }

        return () => Adapt(Activator.CreateInstance(factoryType), options.UserAgent, options.WebSocketFactoryProgId);
    }

    private static IWebSocketConnection CreateDefaultClientWebSocket(string? userAgent)
    {
        var client = new ClientWebSocket();
        ApplyUserAgent(client, userAgent);
        return new ClientWebSocketConnection(client);
    }

    private static IWebSocketConnection Adapt(object? instance, string? userAgent, string? sourceProgId)
    {
        if (instance is null)
        {
            throw new InvalidOperationException($"WebSocket factory '{sourceProgId}' returned null.");
        }

        if (instance is IWebSocketConnection connection)
        {
            return connection;
        }

        if (instance is ClientWebSocket clientWebSocket)
        {
            ApplyUserAgent(clientWebSocket, userAgent);
            return new ClientWebSocketConnection(clientWebSocket);
        }

        if (instance is WebSocket)
        {
            throw new InvalidOperationException($"WebSocket factory '{sourceProgId}' returned an unsupported WebSocket type. Only ClientWebSocket is supported.");
        }

        throw new InvalidCastException($"WebSocket factory '{sourceProgId}' must return either IWebSocketConnection or ClientWebSocket.");
    }

    private static void ApplyUserAgent(ClientWebSocket client, string? userAgent)
    {
        if (string.IsNullOrWhiteSpace(userAgent))
        {
            return;
        }

        try
        {
            client.Options.SetRequestHeader("User-Agent", userAgent);
        }
        catch (ArgumentException ex)
        {
            throw new InvalidOperationException("Failed to set User-Agent on ClientWebSocket options.", ex);
        }
    }
}
