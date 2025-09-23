#pragma once

#include <windows.h>

namespace com::nostr::native
{
    namespace hresults
    {
        constexpr HRESULT E_FAIL_GENERAL = static_cast<HRESULT>(0x80004005);
        constexpr HRESULT E_INVALIDARG_GENERAL = static_cast<HRESULT>(0x80070057);
        constexpr HRESULT E_POINTER_GENERAL = static_cast<HRESULT>(0x80004003);
        constexpr HRESULT E_TIMEOUT_GENERAL = static_cast<HRESULT>(0x800705B4);
        constexpr HRESULT E_CLASS_NOT_REGISTERED = static_cast<HRESULT>(0x80040154);

        constexpr HRESULT E_NOSTR_SIGNER_MISSING = static_cast<HRESULT>(0x88990001);
        constexpr HRESULT E_NOSTR_RELAY_NOT_CONNECTED = static_cast<HRESULT>(0x88990002);
        constexpr HRESULT E_NOSTR_NOT_INITIALIZED = static_cast<HRESULT>(0x88990003);
        constexpr HRESULT E_NOSTR_OBJECT_DISPOSED = static_cast<HRESULT>(0x88990004);
        constexpr HRESULT E_NOSTR_ALREADY_INITIALIZED = static_cast<HRESULT>(0x88990005);
        constexpr HRESULT E_NOSTR_WEBSOCKET_ERROR = static_cast<HRESULT>(0x80200010);

        inline HRESULT WebSocketFailure() noexcept
        {
            return E_NOSTR_WEBSOCKET_ERROR;
        }

        inline HRESULT InvalidArgument() noexcept
        {
            return E_INVALIDARG_GENERAL;
        }

        inline HRESULT PointerRequired() noexcept
        {
            return E_POINTER_GENERAL;
        }

        inline HRESULT Timeout() noexcept
        {
            return E_TIMEOUT_GENERAL;
        }

        inline HRESULT FromWin32(DWORD error) noexcept
        {
            return HRESULT_FROM_WIN32(error);
        }
    }
}