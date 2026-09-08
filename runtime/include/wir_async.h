#pragma once

#include "std_async.h"

namespace wio::wir_backend {
// A cancellation checkpoint reads the current promise, without scheduling or
// blocking. Checking only the awaited child misses already-ready task paths.
struct CancellationCheckpoint final {
    std::weak_ptr<runtime::detail::AsyncTaskStateBase> state;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    bool await_suspend(std::coroutine_handle<Promise> continuation) noexcept {
        state = continuation.promise().state;
        return false;
    }
    void await_resume() const {
        if (auto owner = state.lock(); owner && owner->Cancelled())
            throw runtime::AsyncCancelled();
    }
};

enum class Executor { Worker, Blocking, Io };

struct SwitchExecutor final {
    Executor executor;
    std::weak_ptr<runtime::detail::AsyncTaskStateBase> state;
    bool await_ready() const noexcept { return false; }
    template<class Promise>
    bool await_suspend(std::coroutine_handle<Promise> continuation) {
        state = continuation.promise().state;
        // Inline delivery matters here: an ordinary task continuation would
        // bounce a blocking/IO handoff back onto the worker executor.
        auto registration = std::make_shared<runtime::detail::AsyncInlineContinuationRegistration>(continuation);
        if (auto owner = state.lock()) {
            owner->AddCancellationCallback([registration] { registration->ResumeOnce(); });
        }
        const Executor destination = executor;
        if (!registration->Arm()) return false;
        auto resume = [registration] { registration->ResumeOnce(); };
        const bool posted = destination == Executor::Worker
            ? runtime::DefaultAsyncScheduler().Post(std::move(resume))
            : destination == Executor::Blocking
                ? runtime::DefaultAsyncBlockingScheduler().Submit(std::move(resume))
                : runtime::DefaultAsyncIoScheduler().Submit(std::move(resume));
        if (!posted) {
            if (destination == Executor::Worker ||
                (destination == Executor::Blocking && !runtime::DefaultAsyncBlockingScheduler().IsRunning()) ||
                (destination == Executor::Io && !runtime::DefaultAsyncIoScheduler().IsRunning()))
                throw runtime::AsyncRuntimeStopped();
            if (destination == Executor::Blocking) throw runtime::AsyncQueueFull();
            throw runtime::AsyncIoQueueFull();
        }
        return true;
    }
    void await_resume() const {
        if (auto owner = state.lock(); owner && owner->Cancelled())
            throw runtime::AsyncCancelled();
    }
};

// Entry is the synchronous host boundary, not the implementation of await.
// Pump the bound main executor so an `await main` cannot deadlock entry.
template<class T>
T RunEntry(const runtime::AsyncTask<T>& task) {
    while (!task.IsReady()) {
        runtime::DrainAsyncMainExecutor();
        task.WaitFor(1);
    }
    runtime::DrainAsyncMainExecutor();
    return task.Get();
}
}
