using System;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using COM_Nostr.Contracts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrSubscriptionTests
{
    [TestMethod]
    public async Task OpenSubscription_ReceivesEoseAndManualClose()
    {
        await using var host = await StrfryRelayHost.StartAsync().ConfigureAwait(false);

        var client = new NostrClient();
        client.Initialize(null!);

        var subscriptionFilters = new[]
        {
            new NostrFilter
            {
                Kinds = new[] { 1 }
            }
        };

        var session = client.ConnectRelay(new RelayDescriptor
        {
            Url = host.RelayWebSocketUrl,
            ReadEnabled = true,
            WriteEnabled = true
        }, new NullAuthCallback());
        Assert.AreEqual(RelaySessionState.Connected, session.State);

        var callback = new RecordingEventCallback();
        var subscription = client.OpenSubscription(
            host.RelayWebSocketUrl,
            subscriptionFilters,
            callback,
            new SubscriptionOptions { KeepAlive = true });

        Assert.AreEqual(SubscriptionStatus.Pending, subscription.Status);

        await callback.WaitForEoseAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);
        Assert.AreEqual(SubscriptionStatus.Active, subscription.Status);

        subscription.Close();
        var reason = await callback.WaitForClosedAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);
        Assert.AreEqual("Subscription closed by client.", reason);
        Assert.AreEqual(SubscriptionStatus.Closed, subscription.Status);

        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    [TestMethod]
    public async Task OpenSubscription_KeepAliveFalseDrainsAfterEose()
    {
        await using var host = await StrfryRelayHost.StartAsync().ConfigureAwait(false);

        var client = new NostrClient();
        client.Initialize(null!);

        var session = client.ConnectRelay(new RelayDescriptor
        {
            Url = host.RelayWebSocketUrl,
            ReadEnabled = true,
            WriteEnabled = true
        }, new NullAuthCallback());
        Assert.AreEqual(RelaySessionState.Connected, session.State);

        var callback = new RecordingEventCallback();
        var subscription = client.OpenSubscription(
            host.RelayWebSocketUrl,
            new[] { new NostrFilter() },
            callback,
            new SubscriptionOptions { KeepAlive = false });

        await callback.WaitForEoseAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);

        var reason = await callback.WaitForClosedAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);
        Assert.AreEqual("KeepAlive=false auto close.", reason);
        Assert.AreEqual(SubscriptionStatus.Closed, subscription.Status);

        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    private sealed class RecordingEventCallback : INostrEventCallback
    {
        private readonly TaskCompletionSource<(string Relay, string Subscription)> _eoseSource = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly TaskCompletionSource<string> _closedSource = new(TaskCreationOptions.RunContinuationsAsynchronously);

        public Task WaitForEoseAsync(TimeSpan timeout)
            => _eoseSource.Task.WaitAsync(timeout);

            public Task<string> WaitForClosedAsync(TimeSpan timeout)
            => _closedSource.Task.WaitAsync(timeout);

        public void OnEvent(string relayUrl, NostrEvent @event)
        {
        }

        public void OnEndOfStoredEvents(string relayUrl, string subscriptionId)
        {
            _eoseSource.TrySetResult((relayUrl, subscriptionId));
        }

        public void OnNotice(string relayUrl, string message)
        {
        }

        public void OnClosed(string relayUrl, string subscriptionId, string reason)
        {
            _closedSource.TrySetResult(reason);
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
