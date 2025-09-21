using System;
using System.Runtime.InteropServices;

namespace COM_Nostr.Internal;

internal static class HResults
{
    public const int E_FAIL = unchecked((int)0x80004005);
    public const int E_INVALIDARG = unchecked((int)0x80070057);
    public const int E_POINTER = unchecked((int)0x80004003);
    public const int E_TIMEOUT = unchecked((int)0x800705B4);
    public const int E_CLASSNOTREGISTERED = unchecked((int)0x80040154);

    public const int E_NOSTR_SIGNER_MISSING = unchecked((int)0x88990001);
    public const int E_NOSTR_RELAY_NOT_CONNECTED = unchecked((int)0x88990002);
    public const int E_NOSTR_NOT_INITIALIZED = unchecked((int)0x88990003);
    public const int E_NOSTR_WEBSOCKET_ERROR = unchecked((int)0x80200010);
    public const int E_NOSTR_OBJECT_DISPOSED = unchecked((int)0x88990004);
    public const int E_NOSTR_ALREADY_INITIALIZED = unchecked((int)0x88990005);

    public static COMException Exception(string message, int errorCode)
    {
        return new COMException(message, errorCode);
    }

    public static COMException Exception(string message, Exception inner)
    {
        return new ComExceptionWithErrorCode(message, inner, E_FAIL);
    }

    public static COMException Exception(string message, int errorCode, Exception inner)
    {
        return new ComExceptionWithErrorCode(message, inner, errorCode);
    }

    public static COMException InvalidArgument(string message) => Exception(message, E_INVALIDARG);

    public static COMException InvalidArgument(string message, Exception inner) => Exception(message, E_INVALIDARG, inner);

    public static COMException PointerRequired(string message) => Exception(message, E_POINTER);

    public static COMException Timeout(string message) => Exception(message, E_TIMEOUT);

    public static COMException Timeout(string message, Exception inner) => Exception(message, E_TIMEOUT, inner);

    public static COMException ObjectDisposed(string message) => Exception(message, E_NOSTR_OBJECT_DISPOSED);

    public static COMException AlreadyInitialized(string message) => Exception(message, E_NOSTR_ALREADY_INITIALIZED);

    public static COMException SignerMissing(string message) => Exception(message, E_NOSTR_SIGNER_MISSING);

    public static COMException RelayNotConnected(string message) => Exception(message, E_NOSTR_RELAY_NOT_CONNECTED);

    public static COMException NotInitialized(string message) => Exception(message, E_NOSTR_NOT_INITIALIZED);

    public static COMException WebSocketFailure(string message) => Exception(message, E_NOSTR_WEBSOCKET_ERROR);

    public static COMException WebSocketFailure(string message, Exception inner) => Exception(message, E_NOSTR_WEBSOCKET_ERROR, inner);

    private sealed class ComExceptionWithErrorCode : COMException
    {
        public ComExceptionWithErrorCode(string message, Exception inner, int errorCode)
            : base(message, inner)
        {
            HResult = errorCode;
        }
    }
}
