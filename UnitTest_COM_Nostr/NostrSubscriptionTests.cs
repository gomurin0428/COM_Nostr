using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using COM_Nostr.Contracts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrSubscriptionTests
{
    private const string TestPrivateKeyHex = "1111111111111111111111111111111111111111111111111111111111111111";

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

    [TestMethod]
    public async Task SubscriptionQueueOverflow_DropOldestKeepsSubscriptionActive()
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

        var callback = new OverflowTestEventCallback();
        callback.SetDelay(TimeSpan.FromMilliseconds(500));

        var subscription = client.OpenSubscription(
            host.RelayWebSocketUrl,
            new[] { new NostrFilter { Kinds = new[] { 1 } } },
            callback,
            new SubscriptionOptions
            {
                MaxQueueLength = 2,
                QueueOverflowStrategy = "drop"
            });

        await callback.WaitForEoseAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);

        for (var i = 0; i < 5; i++)
        {
            client.PublishEvent(host.RelayWebSocketUrl, CreateNote($"drop-{i}"));
        }

        await Task.Delay(TimeSpan.FromSeconds(2)).ConfigureAwait(false);
        callback.SetDelay(TimeSpan.Zero);

        await callback.WaitForEventsAsync(2, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

        Assert.AreEqual(SubscriptionStatus.Active, subscription.Status);
        Assert.IsFalse(callback.Closed);
        Assert.IsTrue(callback.Events.Count >= 2);

        subscription.Close();
        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    [TestMethod]
    public async Task SubscriptionQueueOverflow_ThrowClosesSubscription()
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

        var callback = new OverflowTestEventCallback();
        callback.SetDelay(TimeSpan.FromMilliseconds(500));

        var subscription = client.OpenSubscription(
            host.RelayWebSocketUrl,
            new[] { new NostrFilter { Kinds = new[] { 1 } } },
            callback,
            new SubscriptionOptions
            {
                MaxQueueLength = 1,
                QueueOverflowStrategy = "throw"
            });

        await callback.WaitForEoseAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);

        for (var i = 0; i < 3; i++)
        {
            client.PublishEvent(host.RelayWebSocketUrl, CreateNote($"throw-{i}"));
        }

        await Task.Delay(TimeSpan.FromSeconds(1)).ConfigureAwait(false);
        callback.SetDelay(TimeSpan.Zero);

        var reason = await callback.WaitForClosedAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);
        Assert.AreEqual("Subscription queue overflow.", reason);
        Assert.AreEqual(SubscriptionStatus.Closed, subscription.Status);
    }

    [TestMethod]
    public async Task Subscription_ReconnectsAfterRelayRestart()
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

        var callback = new ReconnectEventCallback();

        var subscription = client.OpenSubscription(
            host.RelayWebSocketUrl,
            new[] { new NostrFilter { Kinds = new[] { 1 } } },
            callback,
            new SubscriptionOptions
            {
                KeepAlive = true,
                AutoRequeryWindowSeconds = 30
            });

        await callback.WaitForEoseCountAsync(1, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

        await host.RestartAsync().ConfigureAwait(false);

        await callback.WaitForEoseCountAsync(2, TimeSpan.FromSeconds(30)).ConfigureAwait(false);

        var eventTask = callback.WaitForEventAsync(TimeSpan.FromSeconds(10));
        var note = CreateNote($"reconnect-{Guid.NewGuid():N}");
        client.PublishEvent(host.RelayWebSocketUrl, note);
        var received = await eventTask.ConfigureAwait(false);

        Assert.AreEqual(note.Content, received.Content);
        Assert.AreEqual(SubscriptionStatus.Active, subscription.Status);

        subscription.Close();
        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    private static NostrEvent CreateNote(string content)
    {
        return new NostrEvent
        {
            CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
            Kind = 1,
            Content = content,
            Tags = Array.Empty<object>()
        };
    }

    private static NostrEvent CloneEvent(NostrEvent source)
    {
        return new NostrEvent
        {
            Id = source.Id,
            PublicKey = source.PublicKey,
            CreatedAt = source.CreatedAt,
            Kind = source.Kind,
            Tags = source.Tags?.ToArray() ?? Array.Empty<object>(),
            Content = source.Content,
            Signature = source.Signature
        };
    }

    private sealed class OverflowTestEventCallback : INostrEventCallback
    {
        private readonly TaskCompletionSource<(string Relay, string Subscription)> _eoseSource = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly TaskCompletionSource<string> _closedSource = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly object _sync = new();
        private readonly List<NostrEvent> _events = new();
        private TaskCompletionSource<bool>? _eventWaiter;
        private int _eventTarget;
        private int _delayMilliseconds;

        public bool Closed { get; private set; }

        public IReadOnlyList<NostrEvent> Events
        {
            get
            {
                lock (_sync)
                {
                    return _events.ToList();
                }
            }
        }

        public void SetDelay(TimeSpan delay)
        {
            Interlocked.Exchange(ref _delayMilliseconds, (int)delay.TotalMilliseconds);
        }

        public Task WaitForEoseAsync(TimeSpan timeout) => _eoseSource.Task.WaitAsync(timeout);

        public Task WaitForEventsAsync(int expectedCount, TimeSpan timeout)
        {
            lock (_sync)
            {
                if (_events.Count >= expectedCount)
                {
                    return Task.CompletedTask;
                }

                _eventTarget = expectedCount;
                _eventWaiter = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
                return _eventWaiter.Task.WaitAsync(timeout);
            }
        }

        public Task<string> WaitForClosedAsync(TimeSpan timeout) => _closedSource.Task.WaitAsync(timeout);

        public void OnEvent(string relayUrl, NostrEvent @event)
        {
            lock (_sync)
            {
                _events.Add(CloneEvent(@event));
                if (_eventWaiter is not null && _events.Count >= _eventTarget)
                {
                    _eventWaiter.TrySetResult(true);
                    _eventWaiter = null;
                }
            }

            var delay = Volatile.Read(ref _delayMilliseconds);
            if (delay > 0)
            {
                Thread.Sleep(delay);
            }
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
            Closed = true;
            _closedSource.TrySetResult(reason);
        }
    }

    private sealed class ReconnectEventCallback : INostrEventCallback
    {
        private readonly object _sync = new();
        private TaskCompletionSource<int>? _eoseTargetSource;
        private int _eoseTarget;
        private int _eoseCount;
        private TaskCompletionSource<NostrEvent>? _eventSource;

        public Task WaitForEoseCountAsync(int expectedCount, TimeSpan timeout)
        {
            lock (_sync)
            {
                if (_eoseCount >= expectedCount)
                {
                    return Task.CompletedTask;
                }

                _eoseTarget = expectedCount;
                _eoseTargetSource = new TaskCompletionSource<int>(TaskCreationOptions.RunContinuationsAsynchronously);
                return _eoseTargetSource.Task.WaitAsync(timeout);
            }
        }

        public Task<NostrEvent> WaitForEventAsync(TimeSpan timeout)
        {
            lock (_sync)
            {
                _eventSource = new TaskCompletionSource<NostrEvent>(TaskCreationOptions.RunContinuationsAsynchronously);
                return _eventSource.Task.WaitAsync(timeout);
            }
        }

        public void OnEvent(string relayUrl, NostrEvent @event)
        {
            TaskCompletionSource<NostrEvent>? source;
            lock (_sync)
            {
                source = _eventSource;
                _eventSource = null;
            }

            source?.TrySetResult(CloneEvent(@event));
        }

        public void OnEndOfStoredEvents(string relayUrl, string subscriptionId)
        {
            TaskCompletionSource<int>? source = null;

            lock (_sync)
            {
                _eoseCount++;
                if (_eoseTargetSource is not null && _eoseCount >= _eoseTarget)
                {
                    source = _eoseTargetSource;
                    _eoseTargetSource = null;
                }
            }

            source?.TrySetResult(_eoseCount);
        }

        public void OnNotice(string relayUrl, string message)
        {
        }

        public void OnClosed(string relayUrl, string subscriptionId, string reason)
        {
        }
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
