#include "pch.h"

#include "runtime/ComCallbackDispatcher.h"

#include "NostrHResults.h"

#include <utility>

namespace com::nostr::native
{
    namespace
    {
        constexpr UINT kDispatchMessageId = WM_APP + 1;
        constexpr DWORD kDefaultStartTimeoutMs = 10000;
    }

    ComCallbackDispatcher::ComCallbackDispatcher()
    {
        startEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    ComCallbackDispatcher::~ComCallbackDispatcher()
    {
        Shutdown();
        if (startEvent_)
        {
            CloseHandle(startEvent_);
            startEvent_ = nullptr;
        }
    }

    HRESULT ComCallbackDispatcher::Start()
    {
        if (!startEvent_)
        {
            return hresults::FromWin32(GetLastError());
        }

        if (running_.load(std::memory_order_acquire))
        {
            return S_FALSE;
        }

        ResetEvent(startEvent_);
        threadInitResult_.store(E_FAIL, std::memory_order_release);
        running_.store(true, std::memory_order_release);
        threadId_ = 0;

        threadHandle_ = CreateThread(nullptr, 0, &ComCallbackDispatcher::ThreadProc, this, 0, nullptr);
        if (!threadHandle_)
        {
            running_.store(false, std::memory_order_release);
            return hresults::FromWin32(GetLastError());
        }

        const DWORD waitResult = WaitForSingleObject(startEvent_, kDefaultStartTimeoutMs);
        if (waitResult != WAIT_OBJECT_0)
        {
            PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
            WaitForSingleObject(threadHandle_, 1000);
            CloseHandle(threadHandle_);
            threadHandle_ = nullptr;
            running_.store(false, std::memory_order_release);
            return waitResult == WAIT_TIMEOUT ? hresults::Timeout() : hresults::FromWin32(GetLastError());
        }

        const HRESULT init = threadInitResult_.load(std::memory_order_acquire);
        if (FAILED(init))
        {
            PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
            WaitForSingleObject(threadHandle_, 1000);
            CloseHandle(threadHandle_);
            threadHandle_ = nullptr;
            running_.store(false, std::memory_order_release);
            return init;
        }

        return S_OK;
    }

    HRESULT ComCallbackDispatcher::Shutdown(DWORD waitMilliseconds)
    {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
        {
            return S_OK;
        }

        if (threadId_ != 0)
        {
            PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        }

        if (threadHandle_)
        {
            const DWORD waitResult = WaitForSingleObject(threadHandle_, waitMilliseconds);
            CloseHandle(threadHandle_);
            threadHandle_ = nullptr;

            if (waitResult == WAIT_TIMEOUT)
            {
                return hresults::Timeout();
            }
        }

        return S_OK;
    }

    bool ComCallbackDispatcher::IsRunning() const noexcept
    {
        return running_.load(std::memory_order_acquire);
    }

    DWORD ComCallbackDispatcher::ThreadId() const noexcept
    {
        return threadId_;
    }

    HRESULT ComCallbackDispatcher::Post(std::function<void()> callback)
    {
        if (!callback)
        {
            return hresults::InvalidArgument();
        }

        if (!running_.load(std::memory_order_acquire))
        {
            return hresults::E_NOSTR_OBJECT_DISPOSED;
        }

        {
            std::lock_guard<std::mutex> guard(queueMutex_);
            queue_.push(std::move(callback));
        }

        if (!PostThreadMessageW(threadId_, kDispatchMessageId, 0, 0))
        {
            return hresults::FromWin32(GetLastError());
        }

        return S_OK;
    }

    DWORD WINAPI ComCallbackDispatcher::ThreadProc(LPVOID param)
    {
        auto* self = static_cast<ComCallbackDispatcher*>(param);
        if (!self)
        {
            return 0;
        }

        self->threadId_ = GetCurrentThreadId();
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        self->threadInitResult_.store(hr, std::memory_order_release);
        SetEvent(self->startEvent_);

        if (FAILED(hr))
        {
            return 0;
        }

        MSG msg;
        while (self->running_.load(std::memory_order_acquire))
        {
            const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
            if (result <= 0)
            {
                break;
            }

            if (msg.message == kDispatchMessageId)
            {
                self->DrainQueue();
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        self->DrainQueue();
        CoUninitialize();
        return 0;
    }

    void ComCallbackDispatcher::DrainQueue()
    {
        for (;;)
        {
            std::function<void()> task;
            {
                std::lock_guard<std::mutex> guard(queueMutex_);
                if (queue_.empty())
                {
                    break;
                }

                task = std::move(queue_.front());
                queue_.pop();
            }

            if (!task)
            {
                continue;
            }

            try
            {
                task();
            }
            catch (...)
            {
            }
        }
    }
}
