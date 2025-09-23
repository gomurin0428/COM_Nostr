#pragma once

#include "runtime/INativeWebSocket.h"

#include <atomic>
#include <deque>
#include <mutex>

namespace com::nostr::native
{
    class WinHttpWebSocket final : public INativeWebSocket
    {
    public:
        WinHttpWebSocket();
        ~WinHttpWebSocket() override;

        HRESULT Connect(const std::wstring& url, const ClientRuntimeOptions& options) override;
        HRESULT SendText(const std::vector<uint8_t>& payload, bool endOfMessage) override;
        HRESULT SendBinary(const std::vector<uint8_t>& payload, bool endOfMessage) override;
        HRESULT Receive(DWORD timeoutMilliseconds, NativeWebSocketMessage& message) override;
        HRESULT Close(USHORT closeStatus, const std::wstring& reason) override;
        HRESULT CloseOutput(USHORT closeStatus, const std::wstring& reason) override;
        HRESULT Abort() override;

    private:
        class InternetHandle
        {
        public:
            InternetHandle() noexcept = default;
            explicit InternetHandle(HINTERNET handle) noexcept;
            InternetHandle(const InternetHandle&) = delete;
            InternetHandle& operator=(const InternetHandle&) = delete;
            InternetHandle(InternetHandle&& other) noexcept;
            InternetHandle& operator=(InternetHandle&& other) noexcept;
            ~InternetHandle();

            void Reset(HINTERNET handle = nullptr) noexcept;
            [[nodiscard]] HINTERNET Get() const noexcept;
            explicit operator bool() const noexcept;

        private:
            HINTERNET handle_ = nullptr;
        };

        static void CALLBACK ReceiveWorkCallback(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WORK work);

        HRESULT InitializeHandshake(const std::wstring& url, const ClientRuntimeOptions& options);
        HRESULT ParseUrl(const std::wstring& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& useTls) const;
        void StartReceiveLoop();
        void ReceiveLoop();
        void EnqueueMessage(NativeWebSocketMessage&& message);
        void ShutdownReceiveLoop(bool waitForCallbacks);
        HRESULT SendInternal(WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType, const std::vector<uint8_t>& payload, bool endOfMessage) const;
        HRESULT EnsureWebSocketHandle() const;

        InternetHandle session_;
        InternetHandle connection_;
        InternetHandle request_;
        InternetHandle webSocket_;

        HANDLE messageEvent_ = nullptr;
        PTP_WORK receiveWork_ = nullptr;
        std::atomic<bool> stopRequested_{false};

        mutable std::mutex queueMutex_;
        std::deque<NativeWebSocketMessage> messageQueue_;
    };
}
