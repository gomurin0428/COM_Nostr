using System;
using System.Runtime.InteropServices;

namespace COM_Nostr.Contracts;

[ComVisible(true)]
[Guid("56e23422-5e99-4be6-a29d-fb60eccb6559")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class NostrEvent
{
    [DispId(1)]
    public string Id { get; set; } = string.Empty;

    [DispId(2)]
    public string PublicKey { get; set; } = string.Empty;

    [DispId(3)]
    public double CreatedAt { get; set; }

    [DispId(4)]
    public int Kind { get; set; }

    [DispId(5)]
    public object[] Tags { get; set; } = Array.Empty<object>();

    [DispId(6)]
    public string Content { get; set; } = string.Empty;

    [DispId(7)]
    public string Signature { get; set; } = string.Empty;
}

[ComVisible(true)]
[Guid("ed57aa18-b9fe-4861-a6e9-66d68b9a0c49")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class NostrFilter
{
    [DispId(1)]
    public string[] Ids { get; set; } = Array.Empty<string>();

    [DispId(2)]
    public string[] Authors { get; set; } = Array.Empty<string>();

    [DispId(3)]
    public int[] Kinds { get; set; } = Array.Empty<int>();

    [DispId(4)]
    public NostrTagQuery[] Tags { get; set; } = Array.Empty<NostrTagQuery>();

    [DispId(5)]
    public object? Since { get; set; }

    [DispId(6)]
    public object? Until { get; set; }

    [DispId(7)]
    public object? Limit { get; set; }
}

[ComVisible(true)]
[Guid("4ed88857-740f-451e-8d1f-5959c89f31a2")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class NostrTagQuery
{
    [DispId(1)]
    public string Label { get; set; } = string.Empty;

    [DispId(2)]
    public string[] Values { get; set; } = Array.Empty<string>();
}

[ComVisible(true)]
[Guid("43910586-5980-4cbc-8936-ea8d1d2cb584")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class RelayDescriptor
{
    [DispId(1)]
    public string Url { get; set; } = string.Empty;

    [DispId(2)]
    public bool ReadEnabled { get; set; } = true;

    [DispId(3)]
    public bool WriteEnabled { get; set; } = true;

    [DispId(4)]
    public bool Preferred { get; set; }

    [DispId(5)]
    public object? Metadata { get; set; }
}

[ComVisible(true)]
[Guid("b8522064-312e-4ce0-b4c5-e7d59a76d073")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class SubscriptionOptions
{
    [DispId(1)]
    public bool KeepAlive { get; set; } = true;

    [DispId(2)]
    public object? AutoRequeryWindowSeconds { get; set; }

    [DispId(3)]
    public object? MaxQueueLength { get; set; }

    [DispId(4)]
    public object? QueueOverflowStrategy { get; set; }
}

[ComVisible(true)]
[Guid("83822390-3a93-4edb-ba46-2f2dc960d08d")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class AuthChallenge
{
    [DispId(1)]
    public string RelayUrl { get; set; } = string.Empty;

    [DispId(2)]
    public string Challenge { get; set; } = string.Empty;

    [DispId(3)]
    public object? ExpiresAt { get; set; }
}

[ComVisible(true)]
[Guid("38c8b451-af80-41d6-ae98-36ab672412e7")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class NostrEventDraft
{
    [DispId(1)]
    public string PublicKey { get; set; } = string.Empty;

    [DispId(2)]
    public double CreatedAt { get; set; }

    [DispId(3)]
    public int Kind { get; set; }

    [DispId(4)]
    public object[] Tags { get; set; } = Array.Empty<object>();

    [DispId(5)]
    public string Content { get; set; } = string.Empty;
}

[ComVisible(true)]
[Guid("21831b74-c106-4809-b47d-1bec57addc7c")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class ClientOptions
{
    [DispId(1)]
    public string? WebSocketFactoryProgId { get; set; }

    [DispId(2)]
    public string? UserAgent { get; set; }

    [DispId(3)]
    public object? ConnectTimeoutSeconds { get; set; }

    [DispId(4)]
    public object? SendTimeoutSeconds { get; set; }

    [DispId(5)]
    public object? ReceiveTimeoutSeconds { get; set; }
}

[ComVisible(true)]
[Guid("9b7fce0f-14c5-43ec-97f0-b968f479f3a1")]
[ClassInterface(ClassInterfaceType.AutoDual)]
public class NostrOkResult
{
    [DispId(1)]
    public bool Success { get; set; }

    [DispId(2)]
    public string EventId { get; set; } = string.Empty;

    [DispId(3)]
    public string Message { get; set; } = string.Empty;
}
