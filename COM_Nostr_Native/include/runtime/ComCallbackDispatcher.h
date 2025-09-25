#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>

#include <windows.h>

namespace com::nostr::native
{
    class ComCallbackDispatcher
    {
    public:
        ComCallbackDispatcher();
        ~ComCallbackDispatcher();

        ComCallbackDispatcher(const ComCallbackDispatcher&) = delete;
        ComCallbackDispatcher& operator=(const ComCallbackDispatcher&) = delete;
        ComCallbackDispatcher(ComCallbackDispatcher&&) = delete;
        ComCallbackDispatcher& operator=(ComCallbackDispatcher&&) = delete;

        HRESULT Start();
        HRESULT Shutdown(DWORD waitMilliseconds = 10000);

        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] DWORD ThreadId() const noexcept;

        HRESULT Post(std::function<void()> callback);

    private:
        static DWORD WINAPI ThreadProc(LPVOID param);
        void Run();
        void DrainQueue();

        std::atomic<bool> running_{ false };
        std::atomic<HRESULT> threadInitResult_{ E_FAIL };
        HANDLE threadHandle_ = nullptr;
        HANDLE startEvent_ = nullptr;
        DWORD threadId_ = 0;

        std::mutex queueMutex_;
        std::queue<std::function<void()>> queue_;
    };
}
