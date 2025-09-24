using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrClientTests
{
    private const string TestPrivateKey = "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

    private StrfryRelayHost? _relayHost;

    [TestInitialize]
    public async Task InitializeAsync()
    {
        Environment.SetEnvironmentVariable("NOSTR_PRIVATE_KEY", TestPrivateKey, EnvironmentVariableTarget.Process);
        _relayHost = await StrfryRelayHost.StartAsync().ConfigureAwait(false);
    }

    [TestCleanup]
    public async Task CleanupAsync()
    {
        Environment.SetEnvironmentVariable("NOSTR_PRIVATE_KEY", null, EnvironmentVariableTarget.Process);

        if (_relayHost is not null)
        {
            await _relayHost.DisposeAsync().ConfigureAwait(false);
            _relayHost = null;
        }
    }

    [TestMethod]
    public void InitializeTwice_ThrowsAlreadyInitialized()
    {
        var client = new COMNostrNativeLib.NostrClient();
        COMNostrNativeLib.ClientOptions? firstOptions = null;
        COMNostrNativeLib.ClientOptions? secondOptions = null;

        try
        {
            firstOptions = CreateDefaultClientOptions();
            client.Initialize(firstOptions);

            secondOptions = CreateDefaultClientOptions();
            var ex = Assert.ThrowsException<COMException>(() => client.Initialize(secondOptions));
            Assert.AreEqual(unchecked((int)0x88990005), ex.HResult, "二重初期化時のHRESULTが不正です。");
        }
        finally
        {
            ReleaseComObject(secondOptions);
            ReleaseComObject(firstOptions);
            ReleaseComObject(client);
        }
    }

    [TestMethod]
    public async Task ConnectRelay_ListRelaysReflectStateAsync()
    {
        Assert.IsNotNull(_relayHost, "Strfry relay host is not initialised.");

        var client = new COMNostrNativeLib.NostrClient();
        COMNostrNativeLib.ClientOptions? options = null;
        COMNostrNativeLib.RelayDescriptor? relayDescriptor = null;
        COMNostrNativeLib.INostrRelaySession? session = null;

        try
        {
            options = CreateDefaultClientOptions();
            client.Initialize(options);

            relayDescriptor = new COMNostrNativeLib.RelayDescriptor
            {
                Url = _relayHost!.RelayWebSocketUrl,
                ReadEnabled = true,
                WriteEnabled = true
            };

            var authCallback = new NullAuthCallback();
            session = client.ConnectRelay(relayDescriptor, authCallback);
            await WaitForSessionStateAsync(session, COMNostrNativeLib.RelaySessionState.RelaySessionState_Connected, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            Assert.IsTrue(client.HasRelay(_relayHost.RelayWebSocketUrl), "HasRelay が接続済みURLを返しません。");

            var relays = client.ListRelays() ?? Array.Empty<string>();
            CollectionAssert.Contains(relays, _relayHost.RelayWebSocketUrl, "ListRelays に接続済みURLが含まれていません。");

            client.DisconnectRelay(_relayHost.RelayWebSocketUrl);
            await WaitForSessionStateAsync(session, COMNostrNativeLib.RelaySessionState.RelaySessionState_Disconnected, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            Assert.IsFalse(client.HasRelay(_relayHost.RelayWebSocketUrl), "DisconnectRelay 後も HasRelay が true のままです。");

            relays = client.ListRelays() ?? Array.Empty<string>();
            CollectionAssert.DoesNotContain(relays, _relayHost.RelayWebSocketUrl, "DisconnectRelay 後も ListRelays に URL が残っています。");
        }
        finally
        {
            ReleaseComObject(session);
            ReleaseComObject(relayDescriptor);
            ReleaseComObject(options);
            ReleaseComObject(client);
        }
    }

    [TestMethod]
    public async Task PublishEvent_WithoutSigner_ThrowsSignerMissingAsync()
    {
        Assert.IsNotNull(_relayHost, "Strfry relay host is not initialised.");

        var client = new COMNostrNativeLib.NostrClient();
        COMNostrNativeLib.ClientOptions? options = null;
        COMNostrNativeLib.RelayDescriptor? relayDescriptor = null;
        COMNostrNativeLib.NostrEvent? nostrEvent = null;
        COMNostrNativeLib.INostrRelaySession? session = null;

        try
        {
            options = CreateDefaultClientOptions();
            client.Initialize(options);

            relayDescriptor = new COMNostrNativeLib.RelayDescriptor
            {
                Url = _relayHost!.RelayWebSocketUrl,
                ReadEnabled = true,
                WriteEnabled = true
            };

            var authCallback = new NullAuthCallback();
            session = client.ConnectRelay(relayDescriptor, authCallback);
            await WaitForSessionStateAsync(session, COMNostrNativeLib.RelaySessionState.RelaySessionState_Connected, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            nostrEvent = new COMNostrNativeLib.NostrEvent
            {
                Kind = 1,
                CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                Content = $"missing signer test {Guid.NewGuid():N}"
            };

            var ex = Assert.ThrowsException<COMException>(() => client.PublishEvent(_relayHost.RelayWebSocketUrl, nostrEvent));
            Assert.AreEqual(unchecked((int)0x88990001), ex.HResult, "署名者未設定時のHRESULTが不正です。");
        }
        finally
        {
            if (session is not null)
            {
                try
                {
                    client.DisconnectRelay(_relayHost!.RelayWebSocketUrl);
                }
                catch
                {
                }
            }

            ReleaseComObject(session);
            ReleaseComObject(nostrEvent);
            ReleaseComObject(relayDescriptor);
            ReleaseComObject(options);
            ReleaseComObject(client);
        }
    }

    [TestMethod]
    public async Task OpenSubscription_StatusTransitionsToActiveAfterEndOfStoredEventsAsync()
    {
        Assert.IsNotNull(_relayHost, "Strfry relay host is not initialised.");

        var client = new COMNostrNativeLib.NostrClient();
        COMNostrNativeLib.ClientOptions? options = null;
        COMNostrNativeLib.NostrSigner? signer = null;
        COMNostrNativeLib.RelayDescriptor? relayDescriptor = null;
        COMNostrNativeLib.SubscriptionOptions? subscriptionOptions = null;
        COMNostrNativeLib.NostrFilter[]? filters = null;
        COMNostrNativeLib.INostrSubscription? subscription = null;
        COMNostrNativeLib.INostrRelaySession? session = null;

        try
        {
            options = CreateDefaultClientOptions();
            client.Initialize(options);

            signer = new COMNostrNativeLib.NostrSigner();
            client.SetSigner(signer);

            relayDescriptor = new COMNostrNativeLib.RelayDescriptor
            {
                Url = _relayHost!.RelayWebSocketUrl,
                ReadEnabled = true,
                WriteEnabled = true
            };

            var authCallback = new NullAuthCallback();
            session = client.ConnectRelay(relayDescriptor, authCallback);
            await WaitForSessionStateAsync(session, COMNostrNativeLib.RelaySessionState.RelaySessionState_Connected, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            subscriptionOptions = new COMNostrNativeLib.SubscriptionOptions
            {
                KeepAlive = true
            };

            var publicKey = signer.GetPublicKey();
            filters = new[]
            {
                new COMNostrNativeLib.NostrFilter
                {
                    Authors = new[] { publicKey },
                    Kinds = new[] { 1 },
                    Since = DateTimeOffset.UtcNow.AddMinutes(-1).ToUnixTimeSeconds()
                }
            };

            var callback = new RecordingEventCallback();
            subscription = client.OpenSubscription(_relayHost.RelayWebSocketUrl, filters, callback, subscriptionOptions);

            await callback.WaitForEndOfStoredEventsAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            Assert.IsFalse(string.IsNullOrWhiteSpace(subscription.Id), "購読IDが空です。");
            Assert.AreEqual(COMNostrNativeLib.SubscriptionStatus.SubscriptionStatus_Active, subscription.Status, "EOSE 受信後に購読ステータスがActiveになっていません。");
        }
        finally
        {
            if (subscription is not null)
            {
                try
                {
                    subscription.Close();
                }
                catch
                {
                }
            }

            ReleaseComObject(subscription);

            if (filters is not null)
            {
                foreach (var filter in filters)
                {
                    ReleaseComObject(filter);
                }
            }

            ReleaseComObject(subscriptionOptions);

            if (session is not null)
            {
                try
                {
                    client.DisconnectRelay(_relayHost!.RelayWebSocketUrl);
                }
                catch
                {
                }
            }

            ReleaseComObject(session);
            ReleaseComObject(relayDescriptor);
            ReleaseComObject(signer);
            ReleaseComObject(options);
            ReleaseComObject(client);
        }
    }

    [TestMethod]
    public async Task PublishEvent_ReceivesEventViaSubscription()
    {
        Assert.IsNotNull(_relayHost, "Strfry relay host is not initialised.");

        var client = new COMNostrNativeLib.NostrClient();
        COMNostrNativeLib.ClientOptions? options = null;
        COMNostrNativeLib.NostrSigner? signer = null;
        COMNostrNativeLib.RelayDescriptor? relayDescriptor = null;
        COMNostrNativeLib.SubscriptionOptions? subscriptionOptions = null;
        COMNostrNativeLib.NostrFilter[]? filters = null;
        COMNostrNativeLib.INostrSubscription? subscription = null;
        COMNostrNativeLib.NostrEvent? nostrEvent = null;
        COMNostrNativeLib.INostrRelaySession? session = null;

        try
        {
            options = CreateDefaultClientOptions();
            client.Initialize(options);

            signer = new COMNostrNativeLib.NostrSigner();
            client.SetSigner(signer);

            relayDescriptor = new COMNostrNativeLib.RelayDescriptor
            {
                Url = _relayHost!.RelayWebSocketUrl,
                ReadEnabled = true,
                WriteEnabled = true
            };

            var authCallback = new NullAuthCallback();
            session = client.ConnectRelay(relayDescriptor, authCallback);
            await WaitForSessionStateAsync(session, COMNostrNativeLib.RelaySessionState.RelaySessionState_Connected, TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            subscriptionOptions = new COMNostrNativeLib.SubscriptionOptions
            {
                KeepAlive = true
            };

            var publicKey = signer.GetPublicKey();
            filters = new[]
            {
                new COMNostrNativeLib.NostrFilter
                {
                    Authors = new[] { publicKey },
                    Kinds = new[] { 1 },
                    Since = DateTimeOffset.UtcNow.AddMinutes(-1).ToUnixTimeSeconds()
                }
            };

            var callback = new RecordingEventCallback();
            subscription = client.OpenSubscription(_relayHost.RelayWebSocketUrl, filters, callback, subscriptionOptions);

            await callback.WaitForEndOfStoredEventsAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);

            var noteContent = $"test note {Guid.NewGuid():N}";
            nostrEvent = new COMNostrNativeLib.NostrEvent
            {
                Kind = 1,
                CreatedAt = DateTimeOffset.UtcNow.ToUnixTimeSeconds(),
                Content = noteContent
            };

            client.PublishEvent(_relayHost.RelayWebSocketUrl, nostrEvent);

            var received = await callback.WaitForEventAsync(TimeSpan.FromSeconds(10)).ConfigureAwait(false);
            Assert.AreEqual(noteContent, received.Content, "受信したコンテンツが一致しません。");
            Assert.AreEqual(publicKey, received.PublicKey, "公開鍵が一致しません。");
            Assert.AreEqual(noteContent, nostrEvent.Content, "送信後も元イベントのコンテンツは維持されるべきです。");

            var ok = session.LastOkResult;
            Assert.IsTrue(ok.Success, $"relayのACKが失敗しました: {ok.Message}");
            Assert.AreEqual(nostrEvent.Id, ok.EventId, "ACKのEventIdが一致しません。");
        }
        finally
        {
            if (subscription is not null)
            {
                try
                {
                    subscription.Close();
                }
                catch
                {
                }
            }

            ReleaseComObject(subscription);

            if (session is not null)
            {
                try
                {
                    client.DisconnectRelay(_relayHost!.RelayWebSocketUrl);
                }
                catch
                {
                }
            }

            ReleaseComObject(session);
            ReleaseComObject(nostrEvent);

            if (filters is not null)
            {
                foreach (var filter in filters)
                {
                    ReleaseComObject(filter);
                }
            }

            ReleaseComObject(subscriptionOptions);
            ReleaseComObject(relayDescriptor);
            ReleaseComObject(signer);
            ReleaseComObject(options);
            ReleaseComObject(client);
        }
    }

    private static async Task WaitForSessionStateAsync(COMNostrNativeLib.INostrRelaySession session, COMNostrNativeLib.RelaySessionState expected, TimeSpan timeout)
    {
        
        var deadline = DateTime.UtcNow + timeout;
        while (DateTime.UtcNow < deadline)
        {
            if (session.State == expected)
            {
                return;
            }

            await Task.Delay(200).ConfigureAwait(false);
        }

        Assert.AreEqual(expected, session.State, "セッションが期待した状態になりませんでした。");
    }

    private static COMNostrNativeLib.ClientOptions CreateDefaultClientOptions()
    {
        return new COMNostrNativeLib.ClientOptions
        {
            ConnectTimeoutSeconds = 5d,
            SendTimeoutSeconds = 5d,
            ReceiveTimeoutSeconds = 5d,
            UserAgent = "UnitTest_COMNostrNativeLib/1.0"
        };
    }

    private static void ReleaseComObject(object? value)
    {
        if (value is not null && Marshal.IsComObject(value))
        {
            Marshal.FinalReleaseComObject(value);
        }
    }

    private sealed class NullAuthCallback : COMNostrNativeLib.INostrAuthCallback
    {
        public void OnAuthFailed(string relayUrl, string reason)
        {
            Assert.Fail($"AUTHに失敗しました: {relayUrl} ({reason})");
        }

        public void OnAuthRequired(Object challenge)
        {
            // 認証が必要になった場合はテスト失敗とする。
            Assert.Fail($"想定外のAUTH要求を受信しました: {challenge.ToString()}");
        }

        public void OnAuthSucceeded(string relayUrl)
        {
            // 今回のテストではAUTHを使用しない。
        }
    }

    private sealed class RecordingEventCallback : COMNostrNativeLib.INostrEventCallback
    {
        private readonly TaskCompletionSource<COMNostrNativeLib.NostrEvent> _eventSource = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly TaskCompletionSource<string> _eoseSource = new(TaskCreationOptions.RunContinuationsAsynchronously);

        public void OnClosed(string relayUrl, string subscriptionId, string reason)
        {
            _eventSource.TrySetException(new InvalidOperationException($"CLOSED受信: {relayUrl} ({reason})"));
        }

        public void OnEndOfStoredEvents(string relayUrl, string subscriptionId)
        {
            _eoseSource.TrySetResult(subscriptionId);
        }

        public void OnEvent(string relayUrl, Object @event)
        {
            if (@event is COMNostrNativeLib.NostrEvent nostrEvent)
            {
                _eventSource.TrySetResult(nostrEvent);
            }
            else
            {
                _eventSource.TrySetException(new InvalidCastException($"EVENT ペイロードを COMNostrNativeLib.NostrEvent へ変換できません: {relayUrl}."));
            }
        }

        public void OnNotice(string relayUrl, string message)
        {
            _eventSource.TrySetException(new InvalidOperationException($"NOTICE受信: {relayUrl} ({message})"));
        }

        public Task<COMNostrNativeLib.NostrEvent> WaitForEventAsync(TimeSpan timeout) => WaitWithTimeoutAsync(_eventSource.Task, timeout, "EVENT");

        public Task<string> WaitForEndOfStoredEventsAsync(TimeSpan timeout) => WaitWithTimeoutAsync(_eoseSource.Task, timeout, "EOSE");

        private static async Task<T> WaitWithTimeoutAsync<T>(Task<T> task, TimeSpan timeout, string name)
        {
            var completed = await Task.WhenAny(task, Task.Delay(timeout)).ConfigureAwait(false);
            if (completed != task)
            {
                throw new TimeoutException($"{name} が {timeout} 以内に到着しませんでした。");
            }

            return await task.ConfigureAwait(false);
        }
    }
}
