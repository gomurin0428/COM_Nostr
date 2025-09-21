using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.WebSockets;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using COM_Nostr.Contracts;
using COM_Nostr.Internal;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrAuthTests
{
    [TestMethod]
    public void HandleAuthChallenge_NotifiesRequired()
    {
        var callback = new RecordingAuthCallback();
        var session = CreateSession(callback);

        InvokeHandleAuthChallenge(session, new NostrAuthChallengeMessage("challenge-123", 12345));

        Assert.AreEqual(1, callback.Required.Count);
        Assert.AreEqual("challenge-123", callback.Required[0].Challenge);
        Assert.AreEqual(session.Url, callback.Required[0].RelayUrl);
        Assert.AreEqual(12345d, Convert.ToDouble(callback.Required[0].ExpiresAt));
    }

    [TestMethod]
    public void AuthOkSuccess_NotifiesSucceededAndClearsChallenge()
    {
        var callback = new RecordingAuthCallback();
        var session = CreateSession(callback);
        InvokeHandleAuthChallenge(session, new NostrAuthChallengeMessage("challenge", null));
        callback.Reset();

        session.ProcessAuthAck(new NostrOkMessage("event-id", success: true, message: string.Empty));

        Assert.AreEqual(1, callback.Succeeded.Count);
        Assert.AreEqual(session.Url, callback.Succeeded[0]);
        Assert.IsFalse(session.TryGetAuthChallenge(out _, out _), "Challenge should be cleared after success.");
    }

    [TestMethod]
    public void AuthOkFailure_RequeuesChallengeAndNotifiesFailure()
    {
        var callback = new RecordingAuthCallback();
        var session = CreateSession(callback);
        InvokeHandleAuthChallenge(session, new NostrAuthChallengeMessage("challenge", null));
        callback.Reset();

        session.ProcessAuthAck(new NostrOkMessage("event-id", success: false, message: "auth-required: login"));

        Assert.AreEqual(1, callback.Failed.Count);
        Assert.AreEqual("login", callback.Failed[0].Reason);
        Assert.AreEqual(session.Url, callback.Failed[0].Relay);
        Assert.AreEqual(1, callback.Required.Count, "Challenge should be requested again.");
        Assert.AreEqual("challenge", callback.Required[0].Challenge);
    }

    [TestMethod]
    public void NoticeAuthFailed_NotifiesFailureOnly()
    {
        var callback = new RecordingAuthCallback();
        var session = CreateSession(callback);
        InvokeHandleAuthChallenge(session, new NostrAuthChallengeMessage("challenge", null));
        callback.Reset();

        InvokeHandleAuthReason(session, "auth-failed: invalid signature");

        Assert.AreEqual(1, callback.Failed.Count);
        Assert.AreEqual("invalid signature", callback.Failed[0].Reason);
        Assert.AreEqual(session.Url, callback.Failed[0].Relay);
        Assert.AreEqual(0, callback.Required.Count);
        Assert.AreEqual(0, callback.Succeeded.Count);
    }

    [TestMethod]
    public void NoticeAuthSuccess_NotifiesSucceededOnce()
    {
        var callback = new RecordingAuthCallback();
        var session = CreateSession(callback);
        InvokeHandleAuthChallenge(session, new NostrAuthChallengeMessage("challenge", null));
        callback.Reset();

        InvokeHandleAuthReason(session, "auth-success: welcome");

        Assert.AreEqual(1, callback.Succeeded.Count);
        Assert.AreEqual(session.Url, callback.Succeeded[0]);
        Assert.IsFalse(session.TryGetAuthChallenge(out _, out _));
        Assert.AreEqual(0, callback.Failed.Count);
    }

    private static void InvokeHandleAuthChallenge(NostrRelaySession session, NostrAuthChallengeMessage message)
    {
        var method = typeof(NostrRelaySession).GetMethod("HandleAuthChallenge", BindingFlags.NonPublic | BindingFlags.Instance);
        method!.Invoke(session, new object[] { message });
    }

    private static void InvokeHandleAuthReason(NostrRelaySession session, string reason)
    {
        var method = typeof(NostrRelaySession).GetMethod("HandleAuthSignalFromReason", BindingFlags.NonPublic | BindingFlags.Instance);
        method!.Invoke(session, new object[] { reason });
    }

    private static NostrRelaySession CreateSession(INostrAuthCallback callback)
    {
        var options = new ClientRuntimeOptions();
        Func<HttpClient> httpFactory = () => new HttpClient(new HttpClientHandler());
        Func<IWebSocketConnection> socketFactory = () => new StubWebSocketConnection();
        var serializer = new NostrJsonSerializer();
        var resources = new NostrClientResources(options, httpFactory, socketFactory, serializer);
        var httpClient = new NostrHttpClient(httpFactory);
        var descriptor = new RelayDescriptor
        {
            Url = "wss://example.com",
            ReadEnabled = true,
            WriteEnabled = true
        };

        return new NostrRelaySession(new Uri("wss://example.com"), descriptor, resources, httpClient, callback, null);
    }

    private sealed class RecordingAuthCallback : INostrAuthCallback
    {
        public List<AuthChallenge> Required { get; } = new();
        public List<(string Relay, string Reason)> Failed { get; } = new();
        public List<string> Succeeded { get; } = new();

        public void OnAuthRequired(AuthChallenge challenge)
        {
            Required.Add(challenge);
        }

        public void OnAuthFailed(string relayUrl, string reason)
        {
            Failed.Add((relayUrl, reason));
        }

        public void OnAuthSucceeded(string relayUrl)
        {
            Succeeded.Add(relayUrl);
        }

        public void Reset()
        {
            Required.Clear();
            Failed.Clear();
            Succeeded.Clear();
        }
    }

    private sealed class StubWebSocketConnection : IWebSocketConnection
    {
        public WebSocketState State => WebSocketState.None;

        public ValueTask DisposeAsync() => ValueTask.CompletedTask;

        public Task CloseAsync(WebSocketCloseStatus closeStatus, string? statusDescription, CancellationToken cancellationToken) => Task.CompletedTask;

        public Task CloseOutputAsync(WebSocketCloseStatus closeStatus, string? statusDescription, CancellationToken cancellationToken) => Task.CompletedTask;

        public Task ConnectAsync(Uri uri, CancellationToken cancellationToken) => Task.CompletedTask;

        public ValueTask<ValueWebSocketReceiveResult> ReceiveAsync(Memory<byte> buffer, CancellationToken cancellationToken) =>
            ValueTask.FromResult(new ValueWebSocketReceiveResult(0, WebSocketMessageType.Text, true));

        public Task SendAsync(ReadOnlyMemory<byte> payload, WebSocketMessageType messageType, bool endOfMessage, CancellationToken cancellationToken) => Task.CompletedTask;
    }
}
