using System.Linq;
using System.Threading.Tasks;
using COM_Nostr.Contracts;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace UnitTest_COM_Nostr;

[TestClass]
public sealed class NostrRelaySessionTests
{
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

    [TestMethod]
    public async Task ConnectRelay_FetchesMetadataAndUpdatesSession()
    {
        await using var host = await StrfryRelayHost.StartAsync();

        var client = new NostrClient();
        client.Initialize(null!);

        var descriptor = new RelayDescriptor
        {
            Url = host.RelayWebSocketUrl,
            ReadEnabled = true,
            WriteEnabled = true
        };

        var session = client.ConnectRelay(descriptor, new NullAuthCallback());

        Assert.AreEqual(host.RelayWebSocketUrl, session.Url);
        Assert.AreEqual(RelaySessionState.Connected, session.State);
        Assert.IsTrue(client.HasRelay(host.RelayWebSocketUrl));

        var metadataJson = session.GetDescriptor().Metadata as string;
        Assert.IsFalse(string.IsNullOrEmpty(metadataJson), "Metadata should be populated by NIP-11 fetch.");

        var supported = session.SupportedNips;
        Assert.IsNotNull(supported);
        Assert.IsTrue(supported.Contains(11), "Supported NIPs should include 11 from strfry metadata.");

        client.DisconnectRelay(host.RelayWebSocketUrl);
    }

    [TestMethod]
    public async Task RefreshRelayInfo_ReDownloadsMetadata()
    {
        await using var host = await StrfryRelayHost.StartAsync();

        var client = new NostrClient();
        client.Initialize(null!);

        var descriptor = new RelayDescriptor
        {
            Url = host.RelayWebSocketUrl
        };

        var session = client.ConnectRelay(descriptor, new NullAuthCallback());
        var initialMetadata = session.GetDescriptor().Metadata as string;
        Assert.IsFalse(string.IsNullOrEmpty(initialMetadata));

        client.RefreshRelayInfo(host.RelayWebSocketUrl);

        var refreshedMetadata = session.GetDescriptor().Metadata as string;
        Assert.IsFalse(string.IsNullOrEmpty(refreshedMetadata));
        Assert.AreEqual(initialMetadata, refreshedMetadata);

        client.DisconnectRelay(host.RelayWebSocketUrl);
    }
}
