#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <cstdint>
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
                if (stopping_)
                    throw std::runtime_error("async scheduler is stopping");
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

                        action = std::move(const_cast<Work&>(work_.top()).action);
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

    inline AsyncScheduler& DefaultAsyncScheduler()
    {
        static AsyncScheduler scheduler;
        return scheduler;
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

        struct AsyncFinalAwaiter final
        {
            bool await_ready() const noexcept { return false; }

            template<typename Promise>
            void await_suspend(std::coroutine_handle<Promise> handle) const noexcept
            {
                if (auto state = handle.promise().state.lock())
                    state->Complete();
            }

            void await_resume() const noexcept {}
        };
    }

    template<typename T>
    class AsyncTask
    {
    public:
        struct promise_type
        {
            std::weak_ptr<detail::AsyncTaskState<T>> state;

            AsyncTask get_return_object()
            {
                auto owner = std::make_shared<detail::AsyncTaskState<T>>();
                owner->handle = std::coroutine_handle<promise_type>::from_promise(*this);
                owner->started = true;
                owner->selfKeepAlive = owner;
                state = owner;
                return AsyncTask(std::move(owner));
            }

            std::suspend_never initial_suspend() const noexcept { return {}; }
            detail::AsyncFinalAwaiter final_suspend() const noexcept { return {}; }

            template<typename U>
            void return_value(U&& value)
            {
                if (auto owner = state.lock())
                {
                    std::lock_guard lock(owner->mutex);
                    owner->value.emplace(std::forward<U>(value));
                }
            }

            void unhandled_exception() noexcept
            {
                if (auto owner = state.lock())
                    owner->failure = std::current_exception();
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
            std::weak_ptr<detail::AsyncTaskState<void>> state;

            AsyncTask get_return_object()
            {
                auto owner = std::make_shared<detail::AsyncTaskState<void>>();
                owner->handle = std::coroutine_handle<promise_type>::from_promise(*this);
                owner->started = true;
                owner->selfKeepAlive = owner;
                state = owner;
                return AsyncTask(std::move(owner));
            }

            std::suspend_never initial_suspend() const noexcept { return {}; }
            detail::AsyncFinalAwaiter final_suspend() const noexcept { return {}; }
            void return_void() const noexcept {}

            void unhandled_exception() noexcept
            {
                if (auto owner = state.lock())
                    owner->failure = std::current_exception();
            }
        };

        AsyncTask() = default;

        void Start() const { RequireState()->Start(); }
        void Cancel() const { RequireState()->Cancel(); }
        bool IsReady() const { return state_ && state_->Ready(); }

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
    void CancelAsync(const AsyncTask<T>& task)
    {
        task.Cancel();
    }

    template<typename T>
    AsyncTask<std::vector<T>> WhenAll(std::vector<AsyncTask<T>> tasks)
    {
        for (const auto& task : tasks)
            task.Start();

        std::vector<T> values;
        values.reserve(tasks.size());
        for (const auto& task : tasks)
            values.push_back(co_await task);
        co_return values;
    }

    inline AsyncTask<void> WhenAll(std::vector<AsyncTask<void>> tasks)
    {
        for (const auto& task : tasks)
            task.Start();
        for (const auto& task : tasks)
            co_await task;
    }
}
