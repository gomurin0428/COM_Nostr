using System.Runtime.InteropServices;

namespace COM_Nostr.Contracts;

[ComVisible(true)]
[Guid("21a01469-e744-4999-b0fe-2952231da74a")]
public enum RelaySessionState
{
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Faulted = 3
}

[ComVisible(true)]
[Guid("85af2e31-4669-433e-9104-38b7603f7656")]
public enum SubscriptionStatus
{
    Pending = 0,
    Active = 1,
    Draining = 2,
    Closed = 3
}
