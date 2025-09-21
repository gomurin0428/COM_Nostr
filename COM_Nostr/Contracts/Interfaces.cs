using System.Runtime.InteropServices;

namespace COM_Nostr.Contracts;

[ComVisible(true)]
[Guid("75f24149-8cbf-4f9c-9482-ec8374fdc7b5")]
[InterfaceType(ComInterfaceType.InterfaceIsDual)]
public interface INostrClient
{
    [DispId(1)]
    void Initialize(ClientOptions options);

    [DispId(2)]
    void SetSigner(INostrSigner signer);

    [DispId(3)]
    INostrRelaySession ConnectRelay(RelayDescriptor descriptor, INostrAuthCallback authCallback);

    [DispId(4)]
    void DisconnectRelay(string relayUrl);

    [DispId(5)]
    bool HasRelay(string relayUrl);

    [DispId(6)]
    INostrSubscription OpenSubscription(
        string relayUrl,
        [MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_DISPATCH)] NostrFilter[] filters,
        INostrEventCallback callback,
        SubscriptionOptions options);

    [DispId(7)]
    void PublishEvent(string relayUrl, NostrEvent eventPayload);

    [DispId(8)]
    void RespondAuth(string relayUrl, NostrEvent authEvent);

    [DispId(9)]
    void RefreshRelayInfo(string relayUrl);

    [DispId(10)]
    [return: MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_BSTR)]
    string[] ListRelays();
}

[ComVisible(true)]
[Guid("2d375c22-fda2-4286-bcbe-81fdf2f245b5")]
[InterfaceType(ComInterfaceType.InterfaceIsDual)]
public interface INostrRelaySession
{
    [DispId(1)]
    string Url { get; }

    [DispId(2)]
    RelaySessionState State { get; }

    [DispId(3)]
    NostrOkResult LastOkResult { get; }

    [DispId(4)]
    int[] SupportedNips { [return: MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_I4)] get; }

    [DispId(5)]
    bool WriteEnabled { get; }

    [DispId(6)]
    bool ReadEnabled { get; }

    [DispId(7)]
    void Reconnect();

    [DispId(8)]
    void Close();

    [DispId(9)]
    RelayDescriptor GetDescriptor();

    [DispId(10)]
    void UpdatePolicy(RelayDescriptor descriptor);
}

[ComVisible(true)]
[Guid("143205c5-f229-4e2e-a47c-25c34a7f040d")]
[InterfaceType(ComInterfaceType.InterfaceIsDual)]
public interface INostrSubscription
{
    [DispId(1)]
    string Id { get; }

    [DispId(2)]
    SubscriptionStatus Status { get; }

    [DispId(3)]
    NostrFilter[] Filters { [return: MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_DISPATCH)] get; }

    [DispId(4)]
    void UpdateFilters([MarshalAs(UnmanagedType.SafeArray, SafeArraySubType = VarEnum.VT_DISPATCH)] NostrFilter[] filters);

    [DispId(5)]
    void Close();
}

[ComVisible(true)]
[Guid("fa6b2b97-da84-47d7-9395-3be07d18bb8a")]
[InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface INostrEventCallback
{
    [DispId(1)]
    void OnEvent(string relayUrl, NostrEvent @event);

    [DispId(2)]
    void OnEndOfStoredEvents(string relayUrl, string subscriptionId);

    [DispId(3)]
    void OnNotice(string relayUrl, string message);

    [DispId(4)]
    void OnClosed(string relayUrl, string subscriptionId, string reason);
}

[ComVisible(true)]
[Guid("0168a3e9-0da9-4fbe-92c7-6fb10c12c536")]
[InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface INostrAuthCallback
{
    [DispId(1)]
    void OnAuthRequired(AuthChallenge challenge);

    [DispId(2)]
    void OnAuthSucceeded(string relayUrl);

    [DispId(3)]
    void OnAuthFailed(string relayUrl, string reason);
}

[ComVisible(true)]
[Guid("0c458d3c-2e65-4a90-9f64-7809e944adbc")]
[InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface INostrSigner
{
    [DispId(1)]
    string Sign(NostrEventDraft draft);

    [DispId(2)]
    string GetPublicKey();
}
