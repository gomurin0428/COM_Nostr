using System;

namespace COM_Nostr.Internal;

internal sealed class NostrRelayInformation
{
    public NostrRelayInformation(string metadataJson, int[] supportedNips)
    {
        MetadataJson = metadataJson ?? throw new ArgumentNullException(nameof(metadataJson));
        SupportedNips = supportedNips ?? Array.Empty<int>();
    }

    public string MetadataJson { get; }

    public int[] SupportedNips { get; }
}
