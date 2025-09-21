using System;
using System.Runtime.InteropServices;

namespace COM_Nostr.Contracts;

[ComVisible(true)]
[Guid("7d3091fe-ca18-49ba-835c-012991076660")]
[ClassInterface(ClassInterfaceType.None)]
[ProgId("COM_Nostr.NostrClient")]
public sealed class NostrClient : INostrClient
{
    public void Initialize(ClientOptions options)
    {
        throw new NotImplementedException();
    }

    public void SetSigner(INostrSigner signer)
    {
        throw new NotImplementedException();
    }

    public INostrRelaySession ConnectRelay(RelayDescriptor descriptor, INostrAuthCallback authCallback)
    {
        throw new NotImplementedException();
    }

    public void DisconnectRelay(string relayUrl)
    {
        throw new NotImplementedException();
    }

    public bool HasRelay(string relayUrl)
    {
        throw new NotImplementedException();
    }

    public INostrSubscription OpenSubscription(string relayUrl, NostrFilter[] filters, INostrEventCallback callback, SubscriptionOptions options)
    {
        throw new NotImplementedException();
    }

    public void PublishEvent(string relayUrl, NostrEvent eventPayload)
    {
        throw new NotImplementedException();
    }

    public void RespondAuth(string relayUrl, NostrEvent authEvent)
    {
        throw new NotImplementedException();
    }

    public void RefreshRelayInfo(string relayUrl)
    {
        throw new NotImplementedException();
    }

    public string[] ListRelays()
    {
        throw new NotImplementedException();
    }
}

[ComVisible(true)]
[Guid("e53e9b56-da8d-4064-8df6-5563708f65a5")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class NostrRelaySession : INostrRelaySession
{
    public string Url => throw new NotImplementedException();

    public RelaySessionState State => throw new NotImplementedException();

    public NostrOkResult LastOkResult => throw new NotImplementedException();

    public int[] SupportedNips => throw new NotImplementedException();

    public bool WriteEnabled => throw new NotImplementedException();

    public bool ReadEnabled => throw new NotImplementedException();

    public void Reconnect()
    {
        throw new NotImplementedException();
    }

    public void Close()
    {
        throw new NotImplementedException();
    }

    public RelayDescriptor GetDescriptor()
    {
        throw new NotImplementedException();
    }

    public void UpdatePolicy(RelayDescriptor descriptor)
    {
        throw new NotImplementedException();
    }
}

[ComVisible(true)]
[Guid("175bd625-18d9-42bd-b75a-0642abf029b4")]
[ClassInterface(ClassInterfaceType.None)]
public sealed class NostrSubscription : INostrSubscription
{
    public string Id => throw new NotImplementedException();

    public SubscriptionStatus Status => throw new NotImplementedException();

    public NostrFilter[] Filters => throw new NotImplementedException();

    public void UpdateFilters(NostrFilter[] filters)
    {
        throw new NotImplementedException();
    }

    public void Close()
    {
        throw new NotImplementedException();
    }
}
