using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using COM_Nostr.Contracts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrPublishEventTests
{
    private const string TestPrivateKeyHex = "1111111111111111111111111111111111111111111111111111111111111111";
    private const int E_NOSTR_WEBSOCKET_ERROR = unchecked((int)0x80200010);

    [TestMethod]
    public async Task PublishEvent_AutoSignsAndReceivesOk()
    {
        await using var host = await StrfryRelayHost.StartAsync().ConfigureAwait(false);

        var client = new NostrClient();
        client.Initialize(null!);
        var signer = new NostrSigner(TestPrivateKeyHex);
        client.SetSigner(signer);

        var session = client.ConnectRelay(new RelayDescriptor
        {
            Url = host.RelayWebSocketUrl,
            ReadEnabled = true,
            WriteEnabled = true
        }, new NullAuthCallback());

        Assert.AreEqual(RelaySessionState.Connected, session.State);

        var note = new NostrEvent
        {
            CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
            Kind = 1,
            Content = $"hello from unit test {Guid.NewGuid():N}",
            Tags = new object[]
            {
                new[] { "t", "unit-test" }
            }
        };

        client.PublishEvent(host.RelayWebSocketUrl, note);

        Assert.IsFalse(string.IsNullOrEmpty(note.Id));
        Assert.IsFalse(string.IsNullOrEmpty(note.Signature));
        Assert.AreEqual(note.Id, session.LastOkResult.EventId);
        Assert.IsTrue(session.LastOkResult.Success);

        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    [TestMethod]
    public async Task PublishEvent_InvalidSignatureTriggersNotice()
    {
        await using var host = await StrfryRelayHost.StartAsync().ConfigureAwait(false);

        var client = new NostrClient();
        client.Initialize(null!);
        var signer = new NostrSigner(TestPrivateKeyHex);
        client.SetSigner(signer);

        var session = client.ConnectRelay(new RelayDescriptor
        {
            Url = host.RelayWebSocketUrl,
            ReadEnabled = true,
            WriteEnabled = true
        }, new NullAuthCallback());

        var callback = new NoticeRecordingCallback();
        var subscription = client.OpenSubscription(
            host.RelayWebSocketUrl,
            new[] { new NostrFilter { Kinds = new[] { 1 } } },
            callback,
            new SubscriptionOptions());

        await callback.WaitForEoseAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);

        var invalidEvent = new NostrEvent
        {
            PublicKey = signer.GetPublicKey(),
            CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
            Kind = 1,
            Content = "invalid signature payload",
            Tags = new object[] { new[] { "t", "invalid-signature" } },
            Signature = new string('0', 128)
        };

        var ex = Assert.ThrowsException<COMException>(() =>
        {
            client.PublishEvent(host.RelayWebSocketUrl, invalidEvent);
        });

        Assert.AreEqual(E_NOSTR_WEBSOCKET_ERROR, ex.ErrorCode);
        Assert.IsFalse(session.LastOkResult.Success);
        Assert.AreEqual(invalidEvent.Id, session.LastOkResult.EventId);
        var notice = await callback.WaitForNoticeAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);
        Assert.IsFalse(string.IsNullOrEmpty(notice));

        subscription.Close();
        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    private sealed class NoticeRecordingCallback : INostrEventCallback
    {
        private readonly TaskCompletionSource<(string Relay, string Subscription)> _eoseSource = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly TaskCompletionSource<string> _noticeSource = new(TaskCreationOptions.RunContinuationsAsynchronously);

        public Task WaitForEoseAsync(TimeSpan timeout) => _eoseSource.Task.WaitAsync(timeout);

        public Task<string> WaitForNoticeAsync(TimeSpan timeout) => _noticeSource.Task.WaitAsync(timeout);

        public void OnEvent(string relayUrl, NostrEvent @event)
        {
        }

        public void OnEndOfStoredEvents(string relayUrl, string subscriptionId)
        {
            _eoseSource.TrySetResult((relayUrl, subscriptionId));
        }

        public void OnNotice(string relayUrl, string message)
        {
            _noticeSource.TrySetResult(message);
        }

        public void OnClosed(string relayUrl, string subscriptionId, string reason)
        {
        }
    }

    private sealed class NullAuthCallback : INostrAuthCallback
    {
        public void OnAuthFailed(string relayUrl, string reason)
        {
        }

        public void OnAuthRequired(AuthChallenge challenge)
        {
        }

        public void OnAuthSucceeded(string relayUrl)
        {
        }
    }
}
