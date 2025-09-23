#pragma once

#include "runtime/ClientRuntimeOptions.h"

#include <vector>
#include <string>
#include <windows.h>
#include <winhttp.h>

namespace com::nostr::native
{
    struct NativeWebSocketMessage
    {
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
        bool endOfMessage = true;
        std::vector<uint8_t> payload;
    };

    class INativeWebSocket
    {
    public:
        virtual ~INativeWebSocket() = default;

        virtual HRESULT Connect(const std::wstring& url, const ClientRuntimeOptions& options) = 0;
        virtual HRESULT SendText(const std::vector<uint8_t>& payload, bool endOfMessage) = 0;
        virtual HRESULT SendBinary(const std::vector<uint8_t>& payload, bool endOfMessage) = 0;
        virtual HRESULT Receive(DWORD timeoutMilliseconds, NativeWebSocketMessage& message) = 0;
        virtual HRESULT Close(USHORT closeStatus, const std::wstring& reason) = 0;
        virtual HRESULT CloseOutput(USHORT closeStatus, const std::wstring& reason) = 0;
        virtual HRESULT Abort() = 0;
    };
}
