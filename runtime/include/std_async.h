#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace wio::runtime
{
    class AsyncCancelled final : public std::runtime_error
    {
    public:
        AsyncCancelled() : std::runtime_error("asynchronous operation was cancelled") {}
    };

    class AsyncTimeout final : public std::runtime_error
    {
    public:
        AsyncTimeout() : std::runtime_error("asynchronous operation timed out") {}
    };

    class AsyncScheduler final
    {
    public:
        using Clock = std::chrono::steady_clock;

        explicit AsyncScheduler(std::size_t workerCount = 0)
        {
            if (workerCount == 0)
            {
                const auto hardware = static_cast<std::size_t>(std::thread::hardware_concurrency());
                workerCount = hardware == 0 ? 2 : hardware;
            }
            workerCount = workerCount < 2 ? 2 : workerCount;
            workers_.reserve(workerCount);
            for (std::size_t index = 0; index < workerCount; ++index)
                workers_.emplace_back([this] { RunWorker(); });
        }

        AsyncScheduler(const AsyncScheduler&) = delete;
        AsyncScheduler& operator=(const AsyncScheduler&) = delete;

        ~AsyncScheduler()
        {
            {
                std::lock_guard lock(mutex_);
                stopping_ = true;
            }
            changed_.notify_all();
            for (auto& worker : workers_)
            {
                if (worker.joinable())
                    worker.join();
            }
        }

        void Post(std::function<void()> action)
        {
            PostAt(Clock::now(), std::move(action));
        }

        void PostAfter(std::chrono::milliseconds delay, std::function<void()> action)
        {
            PostAt(Clock::now() + delay, std::move(action));
        }

        std::size_t WorkerCount() const noexcept
        {
            return workers_.size();
        }

    private:
        struct Work final
        {
            Clock::time_point readyAt;
            std::uint64_t sequence = 0;
            std::function<void()> action;
        };

        struct LaterWork final
        {
            bool operator()(const Work& left, const Work& right) const noexcept
            {
                if (left.readyAt != right.readyAt)
                    return left.readyAt > right.readyAt;
                return left.sequence > right.sequence;
            }
        };

        void PostAt(Clock::time_point readyAt, std::function<void()> action)
        {
            {
                std::lock_guard lock(mutex_);
                work_.push(Work{readyAt, nextSequence_++, std::move(action)});
            }
            changed_.notify_one();
        }

        void RunWorker()
        {
            for (;;)
            {
                std::function<void()> action;
                {
                    std::unique_lock lock(mutex_);
                    for (;;)
                    {
                        if (stopping_ && work_.empty())
                            return;
                        if (work_.empty())
                        {
                            changed_.wait(lock);
                            continue;
                        }

                        const auto now = Clock::now();
                        const auto readyAt = work_.top().readyAt;
                        if (readyAt > now)
                        {
                            changed_.wait_until(lock, readyAt);
                            continue;
                        }

                        action = work_.top().action;
                        work_.pop();
                        break;
                    }
                }

                try
                {
                    action();
                }
                catch (...)
                {
                    // Coroutine promises capture user exceptions. Scheduler
                    // actions are isolated so one bad native callback cannot
                    // terminate the worker pool.
                }
            }
        }

        std::mutex mutex_;
        std::condition_variable changed_;
        std::priority_queue<Work, std::vector<Work>, LaterWork> work_;
        std::vector<std::thread> workers_;
        std::uint64_t nextSequence_ = 0;
        bool stopping_ = false;
    };

    inline std::size_t ResolveDefaultAsyncWorkerCount() noexcept
    {
        const char* configured = std::getenv("WIO_ASYNC_WORKERS");
        if (!configured || *configured == '\0')
            return 0;

        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(configured, &end, 10);
        if (end == configured || *end != '\0' || parsed == 0 || parsed > 256)
            return 0;
        return static_cast<std::size_t>(parsed);
    }

    inline AsyncScheduler& DefaultAsyncScheduler()
    {
        // The default scheduler intentionally has process lifetime. Detached
        // tasks must not make process shutdown wait for distant timers, and a
        // static std::thread container cannot be safely abandoned during C++
        // static destruction. The operating system reclaims the scheduler at
        // process exit; structured tasks are still joined by their owners.
        static AsyncScheduler* scheduler = new AsyncScheduler(ResolveDefaultAsyncWorkerCount());
        return *scheduler;
    }

    inline std::uint64_t AsyncWorkerCount() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncScheduler().WorkerCount());
    }

    namespace detail
    {
        struct AsyncTaskStateBase : std::enable_shared_from_this<AsyncTaskStateBase>
        {
            virtual ~AsyncTaskStateBase()
            {
                if (handle)
                    handle.destroy();
            }

            void Start()
            {
                std::shared_ptr<AsyncTaskStateBase> owner;
                {
                    std::lock_guard lock(mutex);
                    if (started || completed)
                        return;
                    started = true;
                    owner = shared_from_this();
                    selfKeepAlive = owner;
                }

                DefaultAsyncScheduler().Post([owner]
                {
                    if (owner->cancelled.load(std::memory_order_acquire))
                    {
                        owner->Complete();
                        return;
                    }
                    owner->handle.resume();
                });
            }

            void Cancel()
            {
                cancelled.store(true, std::memory_order_release);
                bool completeWithoutStarting = false;
                {
                    std::lock_guard lock(mutex);
                    completeWithoutStarting = !started && !completed;
                }
                if (completeWithoutStarting)
                    Complete();
            }

            void Complete()
            {
                std::vector<std::coroutine_handle<>> pending;
                {
                    std::lock_guard lock(mutex);
                    if (completed)
                        return;
                    completed = true;
                    pending.swap(continuations);
                }
                changed.notify_all();
                for (const auto continuation : pending)
                    DefaultAsyncScheduler().Post([continuation] { continuation.resume(); });
                selfKeepAlive.reset();
            }

            bool Ready() const
            {
                std::lock_guard lock(mutex);
                return completed;
            }

            bool Cancelled() const noexcept
            {
                return cancelled.load(std::memory_order_acquire);
            }

            bool Faulted() const
            {
                std::lock_guard lock(mutex);
                return completed && failure != nullptr;
            }

            bool AddContinuation(std::coroutine_handle<> continuation)
            {
                std::lock_guard lock(mutex);
                if (completed)
                    return false;
                continuations.push_back(continuation);
                return true;
            }

            void Wait()
            {
                Start();
                std::unique_lock lock(mutex);
                changed.wait(lock, [this] { return completed; });
            }

            bool WaitFor(const std::chrono::milliseconds timeout)
            {
                Start();
                std::unique_lock lock(mutex);
                return changed.wait_for(lock, timeout, [this] { return completed; });
            }

            void RethrowFailure() const
            {
                if (cancelled.load(std::memory_order_acquire))
                    throw AsyncCancelled();
                if (failure)
                    std::rethrow_exception(failure);
            }

            mutable std::mutex mutex;
            std::condition_variable changed;
            std::coroutine_handle<> handle;
            std::vector<std::coroutine_handle<>> continuations;
            std::shared_ptr<AsyncTaskStateBase> selfKeepAlive;
            std::exception_ptr failure;
            std::atomic<bool> cancelled{false};
            bool started = false;
            bool completed = false;
        };

        template<typename T>
        struct AsyncTaskState final : AsyncTaskStateBase
        {
            std::optional<T> value;
        };

        template<>
        struct AsyncTaskState<void> final : AsyncTaskStateBase
        {
        };

    }

    template<typename T>
    class AsyncTask
    {
    public:
        struct promise_type
        {
            std::shared_ptr<detail::AsyncTaskState<T>> state;

            AsyncTask get_return_object()
            {
                auto owner = std::make_shared<detail::AsyncTaskState<T>>();
                owner->handle = std::coroutine_handle<promise_type>::from_promise(*this);
                owner->started = true;
                owner->selfKeepAlive = owner;
                state = owner;
                return AsyncTask(std::move(owner));
            }

            ~promise_type()
            {
                if (state)
                    state->handle = {};
            }

            std::suspend_never initial_suspend() const noexcept { return {}; }
            std::suspend_never final_suspend() const noexcept { return {}; }

            template<typename U>
            void return_value(U&& value)
            {
                auto owner = state;
                if (owner)
                {
                    std::lock_guard lock(owner->mutex);
                    owner->value.emplace(std::forward<U>(value));
                }
                if (owner)
                    owner->Complete();
            }

            void unhandled_exception() noexcept
            {
                auto owner = state;
                if (!owner)
                    return;
                {
                    std::lock_guard lock(owner->mutex);
                    owner->failure = std::current_exception();
                }
                owner->Complete();
            }
        };

        AsyncTask() = default;

        void Start() const
        {
            RequireState()->Start();
        }

        void Cancel() const
        {
            RequireState()->Cancel();
        }

        bool IsReady() const
        {
            return state_ && state_->Ready();
        }

        bool IsCancelled() const
        {
            return state_ && state_->Cancelled();
        }

        bool IsFaulted() const
        {
            return state_ && state_->Faulted();
        }

        bool WaitFor(const std::uint64_t milliseconds) const
        {
            return RequireState()->WaitFor(std::chrono::milliseconds(milliseconds));
        }

        T Get() const
        {
            auto state = RequireState();
            state->Wait();
            std::lock_guard lock(state->mutex);
            state->RethrowFailure();
            if (!state->value)
                throw std::runtime_error("async task completed without a value");
            return *state->value;
        }

        struct Awaiter final
        {
            std::shared_ptr<detail::AsyncTaskState<T>> state;

            bool await_ready() const { return state && state->Ready(); }

            bool await_suspend(std::coroutine_handle<> continuation)
            {
                if (!state)
                    throw std::runtime_error("cannot await an empty coroutine");
                if (!state->AddContinuation(continuation))
                    return false;
                state->Start();
                return true;
            }

            T await_resume() const
            {
                std::lock_guard lock(state->mutex);
                state->RethrowFailure();
                if (!state->value)
                    throw std::runtime_error("async task completed without a value");
                return *state->value;
            }
        };

        Awaiter operator co_await() const { return Awaiter{RequireState()}; }

    private:
        explicit AsyncTask(std::shared_ptr<detail::AsyncTaskState<T>> state)
            : state_(std::move(state))
        {
        }

        std::shared_ptr<detail::AsyncTaskState<T>> RequireState() const
        {
            if (!state_)
                throw std::runtime_error("empty coroutine has no operation");
            return state_;
        }

        std::shared_ptr<detail::AsyncTaskState<T>> state_;
    };

    template<>
    class AsyncTask<void>
    {
    public:
        struct promise_type
        {
            std::shared_ptr<detail::AsyncTaskState<void>> state;

            AsyncTask get_return_object()
            {
                auto owner = std::make_shared<detail::AsyncTaskState<void>>();
                owner->handle = std::coroutine_handle<promise_type>::from_promise(*this);
                owner->started = true;
                owner->selfKeepAlive = owner;
                state = owner;
                return AsyncTask(std::move(owner));
            }

            ~promise_type()
            {
                if (state)
                    state->handle = {};
            }

            std::suspend_never initial_suspend() const noexcept { return {}; }
            std::suspend_never final_suspend() const noexcept { return {}; }
            void return_void()
            {
                if (state)
                    state->Complete();
            }

            void unhandled_exception() noexcept
            {
                auto owner = state;
                if (!owner)
                    return;
                {
                    std::lock_guard lock(owner->mutex);
                    owner->failure = std::current_exception();
                }
                owner->Complete();
            }
        };

        AsyncTask() = default;

        void Start() const { RequireState()->Start(); }
        void Cancel() const { RequireState()->Cancel(); }
        bool IsReady() const { return state_ && state_->Ready(); }
        bool IsCancelled() const { return state_ && state_->Cancelled(); }
        bool IsFaulted() const { return state_ && state_->Faulted(); }
        bool WaitFor(const std::uint64_t milliseconds) const
        {
            return RequireState()->WaitFor(std::chrono::milliseconds(milliseconds));
        }

        void Get() const
        {
            auto state = RequireState();
            state->Wait();
            std::lock_guard lock(state->mutex);
            state->RethrowFailure();
        }

        struct Awaiter final
        {
            std::shared_ptr<detail::AsyncTaskState<void>> state;

            bool await_ready() const { return state && state->Ready(); }

            bool await_suspend(std::coroutine_handle<> continuation)
            {
                if (!state)
                    throw std::runtime_error("cannot await an empty coroutine");
                if (!state->AddContinuation(continuation))
                    return false;
                state->Start();
                return true;
            }

            void await_resume() const
            {
                std::lock_guard lock(state->mutex);
                state->RethrowFailure();
            }
        };

        Awaiter operator co_await() const { return Awaiter{RequireState()}; }

    private:
        explicit AsyncTask(std::shared_ptr<detail::AsyncTaskState<void>> state)
            : state_(std::move(state))
        {
        }

        std::shared_ptr<detail::AsyncTaskState<void>> RequireState() const
        {
            if (!state_)
                throw std::runtime_error("empty coroutine has no operation");
            return state_;
        }

        std::shared_ptr<detail::AsyncTaskState<void>> state_;
    };

    struct AsyncScheduleAwaiter final
    {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> continuation) const
        {
            DefaultAsyncScheduler().Post([continuation] { continuation.resume(); });
        }
        void await_resume() const noexcept {}
    };

    struct AsyncDelayAwaiter final
    {
        std::chrono::milliseconds delay;
        bool await_ready() const noexcept { return delay.count() == 0; }
        void await_suspend(std::coroutine_handle<> continuation) const
        {
            DefaultAsyncScheduler().PostAfter(delay, [continuation] { continuation.resume(); });
        }
        void await_resume() const noexcept {}
    };

    inline AsyncTask<void> AsyncYield()
    {
        co_await AsyncScheduleAwaiter{};
    }

    inline AsyncTask<void> AsyncSleep(const std::uint64_t milliseconds)
    {
        co_await AsyncDelayAwaiter{std::chrono::milliseconds(milliseconds)};
    }

    template<typename T>
    T BlockOn(const AsyncTask<T>& task)
    {
        return task.Get();
    }

    inline void BlockOn(const AsyncTask<void>& task)
    {
        task.Get();
    }

    template<typename T>
    AsyncTask<T> StartAsync(AsyncTask<T> task)
    {
        task.Start();
        return task;
    }

    template<typename T>
    bool AsyncReady(const AsyncTask<T>& task)
    {
        return task.IsReady();
    }

    template<typename T>
    bool AsyncCancelledStatus(const AsyncTask<T>& task)
    {
        return task.IsCancelled();
    }

    template<typename T>
    bool AsyncFaulted(const AsyncTask<T>& task)
    {
        return task.IsFaulted();
    }

    template<typename T>
    bool AsyncWaitFor(const AsyncTask<T>& task, const std::uint64_t milliseconds)
    {
        return task.WaitFor(milliseconds);
    }

    template<typename T>
    void CancelAsync(const AsyncTask<T>& task)
    {
        task.Cancel();
    }

    template<typename T>
    void DetachAsync(const AsyncTask<T>& task)
    {
        task.Start();
    }

    template<typename T>
    AsyncTask<T> RunAsync(std::function<T()> action)
    {
        co_await AsyncScheduleAwaiter{};
        co_return action();
    }

    inline AsyncTask<void> RunAsync(std::function<void()> action)
    {
        co_await AsyncScheduleAwaiter{};
        action();
    }

    template<typename T>
    AsyncTask<std::vector<T>> WhenAll(std::vector<AsyncTask<T>> tasks)
    {
        for (const auto& task : tasks)
            task.Start();

        std::vector<T> values;
        values.reserve(tasks.size());
        try
        {
            for (const auto& task : tasks)
                values.push_back(co_await task);
        }
        catch (...)
        {
            for (const auto& task : tasks)
                task.Cancel();
            throw;
        }
        co_return values;
    }

    inline AsyncTask<void> WhenAll(std::vector<AsyncTask<void>> tasks)
    {
        for (const auto& task : tasks)
            task.Start();
        try
        {
            for (const auto& task : tasks)
                co_await task;
        }
        catch (...)
        {
            for (const auto& task : tasks)
                task.Cancel();
            throw;
        }
    }

    template<typename T>
    AsyncTask<std::size_t> WhenAny(std::vector<AsyncTask<T>> tasks)
    {
        if (tasks.empty())
            throw std::invalid_argument("WhenAny requires at least one coroutine");
        for (const auto& task : tasks)
            task.Start();

        for (;;)
        {
            for (std::size_t index = 0; index < tasks.size(); ++index)
            {
                if (tasks[index].IsReady())
                    co_return index;
            }
            co_await AsyncDelayAwaiter{std::chrono::milliseconds(1)};
        }
    }

    template<typename T>
    AsyncTask<T> Race(std::vector<AsyncTask<T>> tasks)
    {
        const std::size_t index = co_await WhenAny(tasks);
        try
        {
            T value = co_await tasks[index];
            for (std::size_t loser = 0; loser < tasks.size(); ++loser)
            {
                if (loser != index)
                    tasks[loser].Cancel();
            }
            co_return value;
        }
        catch (...)
        {
            for (const auto& task : tasks)
                task.Cancel();
            throw;
        }
    }

    template<typename T>
    AsyncTask<void> CancelAfter(AsyncTask<T> task, const std::uint64_t milliseconds)
    {
        co_await AsyncDelayAwaiter{std::chrono::milliseconds(milliseconds)};
        if (!task.IsReady())
            task.Cancel();
    }

    template<typename T>
    AsyncTask<T> WithTimeout(AsyncTask<T> task, const std::uint64_t milliseconds)
    {
        task.Start();
        const auto deadline = AsyncScheduler::Clock::now() + std::chrono::milliseconds(milliseconds);
        while (!task.IsReady())
        {
            if (AsyncScheduler::Clock::now() >= deadline)
            {
                task.Cancel();
                throw AsyncTimeout();
            }
            co_await AsyncDelayAwaiter{std::chrono::milliseconds(1)};
        }
        co_return co_await task;
    }

    inline AsyncTask<void> WithTimeout(AsyncTask<void> task, const std::uint64_t milliseconds)
    {
        task.Start();
        const auto deadline = AsyncScheduler::Clock::now() + std::chrono::milliseconds(milliseconds);
        while (!task.IsReady())
        {
            if (AsyncScheduler::Clock::now() >= deadline)
            {
                task.Cancel();
                throw AsyncTimeout();
            }
            co_await AsyncDelayAwaiter{std::chrono::milliseconds(1)};
        }
        co_await task;
    }
}
