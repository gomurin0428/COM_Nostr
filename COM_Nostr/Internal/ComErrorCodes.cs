namespace COM_Nostr.Internal;

internal static class ComErrorCodes
{
    public const int E_NOSTR_SIGNER_MISSING = unchecked((int)0x88990001);
    public const int E_NOSTR_RELAY_NOT_CONNECTED = unchecked((int)0x88990002);
    public const int E_NOSTR_NOT_INITIALIZED = unchecked((int)0x88990003);
    public const int E_NOSTR_WEBSOCKET_ERROR = unchecked((int)0x80200010);
    public const int E_NOSTR_OBJECT_DISPOSED = unchecked((int)0x88990004);
}
