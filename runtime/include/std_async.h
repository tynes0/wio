#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
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

    class AsyncRuntimeStopped final : public std::runtime_error
    {
    public:
        AsyncRuntimeStopped() : std::runtime_error("asynchronous runtime is shutting down") {}
    };

    class AsyncQueueFull final : public std::runtime_error
    {
    public:
        AsyncQueueFull() : std::runtime_error("asynchronous blocking queue is full") {}
    };

    class AsyncIoQueueFull final : public std::runtime_error
    {
    public:
        AsyncIoQueueFull() : std::runtime_error("asynchronous I/O queue is full") {}
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

        ~AsyncScheduler() { Shutdown(); }

        bool Post(std::function<void()> action)
        {
            return PostAt(Clock::now(), std::move(action), nullptr);
        }

        std::shared_ptr<std::atomic<bool>> PostAfter(
            std::chrono::milliseconds delay,
            std::function<void()> action)
        {
            auto cancelled = std::make_shared<std::atomic<bool>>(false);
            if (!PostAt(Clock::now() + delay, std::move(action), cancelled))
                return nullptr;
            return cancelled;
        }

        std::size_t WorkerCount() const noexcept
        {
            return workers_.size();
        }

        std::size_t PendingCount() const noexcept
        {
            std::lock_guard lock(mutex_);
            return work_.size();
        }

        bool IsRunning() const noexcept
        {
            std::lock_guard lock(mutex_);
            return !stopping_;
        }

        void Shutdown() noexcept
        {
            std::unique_lock shutdownLock(shutdownMutex_);
            {
                std::lock_guard lock(mutex_);
                stopping_ = true;
            }
            changed_.notify_all();
            for (auto& worker : workers_)
            {
                if (!worker.joinable())
                    continue;
                if (worker.get_id() == std::this_thread::get_id())
                    continue;
                else
                    worker.join();
            }
        }

    private:
        struct Work final
        {
            Clock::time_point readyAt;
            std::uint64_t sequence = 0;
            std::function<void()> action;
            std::shared_ptr<std::atomic<bool>> cancelled;
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

        bool PostAt(
            Clock::time_point readyAt,
            std::function<void()> action,
            std::shared_ptr<std::atomic<bool>> cancelled)
        {
            {
                std::lock_guard lock(mutex_);
                if (stopping_)
                    return false;
                work_.push(Work{readyAt, nextSequence_++, std::move(action), std::move(cancelled)});
            }
            changed_.notify_one();
            return true;
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

                        if (work_.top().cancelled &&
                            work_.top().cancelled->load(std::memory_order_acquire))
                        {
                            work_.pop();
                            continue;
                        }

                        const auto now = Clock::now();
                        const auto readyAt = work_.top().readyAt;
                        if (readyAt > now && !stopping_)
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

        mutable std::mutex mutex_;
        std::mutex shutdownMutex_;
        std::condition_variable changed_;
        std::priority_queue<Work, std::vector<Work>, LaterWork> work_;
        std::vector<std::thread> workers_;
        std::uint64_t nextSequence_ = 0;
        bool stopping_ = false;
    };

    class AsyncBlockingScheduler final
    {
    public:
        explicit AsyncBlockingScheduler(std::size_t workerCount, std::size_t queueCapacity)
            : queueCapacity_(queueCapacity == 0 ? 1 : queueCapacity)
        {
            workerCount = workerCount == 0 ? 1 : workerCount;
            workers_.reserve(workerCount);
            for (std::size_t index = 0; index < workerCount; ++index)
                workers_.emplace_back([this] { RunWorker(); });
        }

        AsyncBlockingScheduler(const AsyncBlockingScheduler&) = delete;
        AsyncBlockingScheduler& operator=(const AsyncBlockingScheduler&) = delete;
        ~AsyncBlockingScheduler() { Shutdown(); }

        bool Submit(std::function<void()> action)
        {
            {
                std::lock_guard lock(mutex_);
                if (stopping_ || work_.size() >= queueCapacity_)
                    return false;
                work_.push_back(std::move(action));
            }
            changed_.notify_one();
            return true;
        }

        void Shutdown() noexcept
        {
            std::unique_lock shutdownLock(shutdownMutex_);
            {
                std::lock_guard lock(mutex_);
                stopping_ = true;
            }
            changed_.notify_all();
            for (auto& worker : workers_)
            {
                if (!worker.joinable())
                    continue;
                if (worker.get_id() == std::this_thread::get_id())
                    continue;
                else
                    worker.join();
            }
        }

        std::size_t WorkerCount() const noexcept { return workers_.size(); }
        std::size_t QueueCapacity() const noexcept { return queueCapacity_; }
        std::size_t PendingCount() const noexcept
        {
            std::lock_guard lock(mutex_);
            return work_.size();
        }
        bool IsRunning() const noexcept
        {
            std::lock_guard lock(mutex_);
            return !stopping_;
        }

    private:
        void RunWorker()
        {
            for (;;)
            {
                std::function<void()> action;
                {
                    std::unique_lock lock(mutex_);
                    changed_.wait(lock, [this] { return stopping_ || !work_.empty(); });
                    if (work_.empty())
                        return;
                    action = std::move(work_.front());
                    work_.pop_front();
                }

                try { action(); }
                catch (...) { }
            }
        }

        mutable std::mutex mutex_;
        std::mutex shutdownMutex_;
        std::condition_variable changed_;
        std::deque<std::function<void()>> work_;
        std::vector<std::thread> workers_;
        std::size_t queueCapacity_ = 1;
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

    inline std::size_t ResolveAsyncLimit(
        const char* name,
        const std::size_t fallback,
        const std::size_t maximum) noexcept
    {
        const char* configured = std::getenv(name);
        if (!configured || *configured == '\0')
            return fallback;

        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(configured, &end, 10);
        if (end == configured || *end != '\0' || parsed == 0 || parsed > maximum)
            return fallback;
        return static_cast<std::size_t>(parsed);
    }

    inline std::size_t ResolveDefaultBlockingWorkerCount() noexcept
    {
        const auto hardware = static_cast<std::size_t>(std::thread::hardware_concurrency());
        const std::size_t fallback = hardware == 0 ? 2 : std::max<std::size_t>(1, std::min<std::size_t>(hardware / 2, 8));
        return ResolveAsyncLimit("WIO_ASYNC_BLOCKING_WORKERS", fallback, 64);
    }

    inline std::size_t ResolveDefaultBlockingQueueCapacity() noexcept
    {
        return ResolveAsyncLimit("WIO_ASYNC_BLOCKING_QUEUE", 1024, 1u << 20u);
    }

    inline std::size_t ResolveDefaultIoWorkerCount() noexcept
    {
        const auto hardware = static_cast<std::size_t>(std::thread::hardware_concurrency());
        const std::size_t fallback = hardware == 0
            ? 2
            : std::max<std::size_t>(1, std::min<std::size_t>(hardware / 2, 8));
        return ResolveAsyncLimit("WIO_ASYNC_IO_WORKERS", fallback, 64);
    }

    inline std::size_t ResolveDefaultIoQueueCapacity() noexcept
    {
        return ResolveAsyncLimit("WIO_ASYNC_IO_QUEUE", 2048, 1u << 20u);
    }

    class AsyncMainExecutor final
    {
    public:
        void BindCurrentThread()
        {
            std::lock_guard lock(mutex_);
            const auto current = std::this_thread::get_id();
            if (bound_ && owner_ != current)
                throw std::runtime_error("main executor is already bound to another thread");
            owner_ = current;
            bound_ = true;
        }

        bool IsCurrentThread() const
        {
            std::lock_guard lock(mutex_);
            return bound_ && owner_ == std::this_thread::get_id();
        }

        bool Post(std::function<void()> action)
        {
            if (!action)
                return false;
            std::lock_guard lock(mutex_);
            work_.push_back(std::move(action));
            return true;
        }

        std::size_t Drain()
        {
            std::deque<std::function<void()>> pending;
            {
                std::lock_guard lock(mutex_);
                const auto current = std::this_thread::get_id();
                if (!bound_)
                {
                    owner_ = current;
                    bound_ = true;
                }
                else if (owner_ != current)
                {
                    throw std::runtime_error("main executor can only be drained by its owner thread");
                }
                pending.swap(work_);
            }

            for (auto& action : pending)
                action();
            return pending.size();
        }

        std::size_t PendingCount() const
        {
            std::lock_guard lock(mutex_);
            return work_.size();
        }

    private:
        mutable std::mutex mutex_;
        std::deque<std::function<void()>> work_;
        std::thread::id owner_{};
        bool bound_ = false;
    };

    inline AsyncScheduler& DefaultAsyncScheduler()
    {
        // Shutdown drains queued timers immediately instead of honoring their
        // remaining wall-clock delay. This lets coroutine frames and captured
        // values clean up without making detached work hold process exit.
        static AsyncScheduler scheduler(ResolveDefaultAsyncWorkerCount());
        return scheduler;
    }

    inline AsyncBlockingScheduler& DefaultAsyncBlockingScheduler()
    {
        static AsyncBlockingScheduler scheduler(
            ResolveDefaultBlockingWorkerCount(),
            ResolveDefaultBlockingQueueCapacity());
        return scheduler;
    }

    inline AsyncBlockingScheduler& DefaultAsyncIoScheduler()
    {
        static AsyncBlockingScheduler scheduler(
            ResolveDefaultIoWorkerCount(),
            ResolveDefaultIoQueueCapacity());
        return scheduler;
    }

    inline AsyncMainExecutor& DefaultAsyncMainExecutor()
    {
        static AsyncMainExecutor executor;
        return executor;
    }

    inline void BindAsyncMainExecutor()
    {
        DefaultAsyncMainExecutor().BindCurrentThread();
    }

    inline std::uint64_t DrainAsyncMainExecutor()
    {
        return static_cast<std::uint64_t>(DefaultAsyncMainExecutor().Drain());
    }

    inline std::uint64_t AsyncMainPendingCount()
    {
        return static_cast<std::uint64_t>(DefaultAsyncMainExecutor().PendingCount());
    }

    inline bool IsAsyncMainThread()
    {
        return DefaultAsyncMainExecutor().IsCurrentThread();
    }

    inline std::uint64_t AsyncWorkerCount() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncScheduler().WorkerCount());
    }

    inline std::uint64_t AsyncBlockingWorkerCount() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncBlockingScheduler().WorkerCount());
    }

    inline std::uint64_t AsyncBlockingQueueCapacity() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncBlockingScheduler().QueueCapacity());
    }

    inline std::uint64_t AsyncBlockingPendingCount() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncBlockingScheduler().PendingCount());
    }

    inline std::uint64_t AsyncIoWorkerCount() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncIoScheduler().WorkerCount());
    }

    inline std::uint64_t AsyncIoQueueCapacity() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncIoScheduler().QueueCapacity());
    }

    inline std::uint64_t AsyncIoPendingCount() noexcept
    {
        return static_cast<std::uint64_t>(DefaultAsyncIoScheduler().PendingCount());
    }

    inline bool AsyncRuntimeRunning() noexcept
    {
        return DefaultAsyncScheduler().IsRunning() &&
            DefaultAsyncBlockingScheduler().IsRunning() &&
            DefaultAsyncIoScheduler().IsRunning();
    }

    inline void ShutdownAsyncRuntime() noexcept
    {
        // I/O and blocking jobs may publish continuations back to the async
        // pool, so both bounded work queues drain before continuation workers
        // stop.
        DefaultAsyncIoScheduler().Shutdown();
        DefaultAsyncBlockingScheduler().Shutdown();
        DefaultAsyncScheduler().Shutdown();
    }

    namespace detail
    {
        struct AsyncContinuationRegistration final
        {
            explicit AsyncContinuationRegistration(std::coroutine_handle<> value)
                : continuation(value)
            {
            }

            void ResumeOnce() noexcept
            {
                if (phase.exchange(2, std::memory_order_acq_rel) != 1)
                    return;
                if (!DefaultAsyncScheduler().Post([value = continuation] { value.resume(); }))
                    continuation.resume();
            }

            bool Arm() noexcept
            {
                int expected = 0;
                return phase.compare_exchange_strong(
                    expected, 1, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            // 0 = registration is still being assembled, 1 = suspension is
            // armed, 2 = completion/cancellation already claimed it.
            std::atomic<int> phase{0};
            std::coroutine_handle<> continuation;
        };

        struct AsyncInlineContinuationRegistration final
        {
            explicit AsyncInlineContinuationRegistration(std::coroutine_handle<> value)
                : continuation(value)
            {
            }

            void ResumeOnce() noexcept
            {
                if (phase.exchange(2, std::memory_order_acq_rel) != 1)
                    return;
                continuation.resume();
            }

            bool Arm() noexcept
            {
                int expected = 0;
                return phase.compare_exchange_strong(
                    expected, 1, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            std::atomic<int> phase{0};
            std::coroutine_handle<> continuation;
        };

        struct AsyncCancellationRegistration final
        {
            explicit AsyncCancellationRegistration(std::function<void()> value)
                : action(std::move(value))
            {
            }

            void InvokeOnce() noexcept
            {
                if (!active.exchange(false, std::memory_order_acq_rel))
                    return;
                try { action(); }
                catch (...) { }
            }

            void Deactivate() noexcept
            {
                active.store(false, std::memory_order_release);
            }

            std::atomic<bool> active{true};
            std::function<void()> action;
        };

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

                if (!DefaultAsyncScheduler().Post([owner]
                {
                    if (owner->cancelled.load(std::memory_order_acquire))
                    {
                        owner->Complete();
                        return;
                    }
                    owner->handle.resume();
                }))
                {
                    {
                        std::lock_guard lock(mutex);
                        failure = std::make_exception_ptr(AsyncRuntimeStopped());
                    }
                    Complete();
                }
            }

            void Cancel()
            {
                bool completeWithoutStarting = false;
                std::vector<std::shared_ptr<AsyncCancellationRegistration>> pendingCancellation;
                {
                    std::lock_guard lock(mutex);
                    // Completion wins over a late cancellation request.  A
                    // value that was already published must stay observable.
                    if (completed || cancelled.load(std::memory_order_acquire))
                        return;
                    cancelled.store(true, std::memory_order_release);
                    completeWithoutStarting = !started && !completed;
                    pendingCancellation = cancellationCallbacks;
                }
                for (const auto& registration : pendingCancellation)
                    registration->InvokeOnce();
                if (completeWithoutStarting)
                    Complete();
            }

            void Complete()
            {
                std::vector<std::shared_ptr<AsyncContinuationRegistration>> pending;
                std::vector<std::shared_ptr<AsyncCancellationRegistration>> pendingCancellation;
                {
                    std::lock_guard lock(mutex);
                    if (completed)
                        return;
                    completed = true;
                    pending.swap(continuations);
                    pendingCancellation.swap(cancellationCallbacks);
                }
                for (const auto& registration : pendingCancellation)
                    registration->Deactivate();
                changed.notify_all();
                for (const auto& continuation : pending)
                    continuation->ResumeOnce();
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

            std::string FailureMessage() const
            {
                std::exception_ptr captured;
                {
                    std::lock_guard lock(mutex);
                    captured = failure;
                }
                if (!captured)
                    return {};
                try
                {
                    std::rethrow_exception(captured);
                }
                catch (const std::exception& error)
                {
                    return error.what();
                }
                catch (...)
                {
                    return "unknown asynchronous failure";
                }
            }

            std::shared_ptr<AsyncContinuationRegistration> AddContinuation(
                std::coroutine_handle<> continuation)
            {
                auto registration = std::make_shared<AsyncContinuationRegistration>(continuation);
                std::lock_guard lock(mutex);
                if (completed)
                    return nullptr;
                continuations.push_back(registration);
                return registration;
            }

            std::shared_ptr<AsyncCancellationRegistration> AddCancellationCallback(
                std::function<void()> callback)
            {
                auto registration = std::make_shared<AsyncCancellationRegistration>(std::move(callback));
                bool invokeImmediately = false;
                {
                    std::lock_guard lock(mutex);
                    if (completed)
                    {
                        registration->Deactivate();
                        return registration;
                    }
                    cancellationCallbacks.push_back(registration);
                    invokeImmediately = cancelled.load(std::memory_order_acquire);
                }
                if (invokeImmediately)
                    registration->InvokeOnce();
                return registration;
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
            std::vector<std::shared_ptr<AsyncContinuationRegistration>> continuations;
            std::vector<std::shared_ptr<AsyncCancellationRegistration>> cancellationCallbacks;
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
            std::weak_ptr<detail::AsyncTaskStateBase> awaitingState;

            bool await_ready() const { return state && state->Ready(); }

            template<typename Promise>
            bool await_suspend(std::coroutine_handle<Promise> continuation)
            {
                if (!state)
                    throw std::runtime_error("cannot await an empty coroutine");
                if constexpr (requires(Promise& promise) { promise.state; })
                    awaitingState = continuation.promise().state;

                auto registration = state->AddContinuation(continuation);
                if (!registration)
                    return false;

                if constexpr (requires(Promise& promise) { promise.state; })
                {
                    auto parent = awaitingState.lock();
                    if (parent)
                    {
                        parent->AddCancellationCallback([registration]
                        {
                            registration->ResumeOnce();
                        });
                    }
                }
                state->Start();
                return registration->Arm();
            }

            T await_resume() const
            {
                if (auto parent = awaitingState.lock(); parent && parent->Cancelled())
                    throw AsyncCancelled();
                std::lock_guard lock(state->mutex);
                state->RethrowFailure();
                if (!state->value)
                    throw std::runtime_error("async task completed without a value");
                return *state->value;
            }
        };

        Awaiter operator co_await() const { return Awaiter{RequireState()}; }

        std::shared_ptr<detail::AsyncTaskStateBase> SharedState() const
        {
            return RequireState();
        }

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
            std::weak_ptr<detail::AsyncTaskStateBase> awaitingState;

            bool await_ready() const { return state && state->Ready(); }

            template<typename Promise>
            bool await_suspend(std::coroutine_handle<Promise> continuation)
            {
                if (!state)
                    throw std::runtime_error("cannot await an empty coroutine");
                if constexpr (requires(Promise& promise) { promise.state; })
                    awaitingState = continuation.promise().state;

                auto registration = state->AddContinuation(continuation);
                if (!registration)
                    return false;
                if constexpr (requires(Promise& promise) { promise.state; })
                {
                    auto parent = awaitingState.lock();
                    if (parent)
                    {
                        parent->AddCancellationCallback([registration]
                        {
                            registration->ResumeOnce();
                        });
                    }
                }
                state->Start();
                return registration->Arm();
            }

            void await_resume() const
            {
                if (auto parent = awaitingState.lock(); parent && parent->Cancelled())
                    throw AsyncCancelled();
                std::lock_guard lock(state->mutex);
                state->RethrowFailure();
            }
        };

        Awaiter operator co_await() const { return Awaiter{RequireState()}; }

        std::shared_ptr<detail::AsyncTaskStateBase> SharedState() const
        {
            return RequireState();
        }

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

    struct AsyncStateAwaiter final
    {
        std::shared_ptr<detail::AsyncTaskStateBase> state;

        bool await_ready() const { return state && state->Ready(); }

        bool await_suspend(std::coroutine_handle<> continuation)
        {
            if (!state)
                throw std::runtime_error("cannot await an empty scoped task");
            auto registration = state->AddContinuation(continuation);
            if (!registration)
                return false;
            state->Start();
            return registration->Arm();
        }

        void await_resume() const
        {
            std::lock_guard lock(state->mutex);
            state->RethrowFailure();
        }
    };

    class AsyncScopeState final : public std::enable_shared_from_this<AsyncScopeState>
    {
    public:
        bool Add(const std::shared_ptr<detail::AsyncTaskStateBase>& child)
        {
            std::lock_guard lock(mutex_);
            if (closed_)
                return false;
            children_.push_back(child);
            return true;
        }

        std::vector<std::shared_ptr<detail::AsyncTaskStateBase>> CloseAndSnapshot()
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
            return children_;
        }

        void Clear()
        {
            std::lock_guard lock(mutex_);
            children_.clear();
        }

        void Cancel()
        {
            auto children = CloseAndSnapshot();
            for (const auto& child : children)
                child->Cancel();
        }

        void CancelAndWait() noexcept
        {
            try
            {
                auto children = CloseAndSnapshot();
                for (const auto& child : children)
                    child->Cancel();
                for (const auto& child : children)
                    child->Wait();
                Clear();
            }
            catch (...)
            {
            }
        }

        std::size_t Count() const
        {
            std::lock_guard lock(mutex_);
            return children_.size();
        }

        bool IsClosed() const
        {
            std::lock_guard lock(mutex_);
            return closed_;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<std::shared_ptr<detail::AsyncTaskStateBase>> children_;
        bool closed_ = false;
    };

    using AsyncScopeHandle = std::shared_ptr<AsyncScopeState>*;

    inline void* AsyncScopeCreate()
    {
        return new std::shared_ptr<AsyncScopeState>(std::make_shared<AsyncScopeState>());
    }

    inline std::shared_ptr<AsyncScopeState> RequireAsyncScope(void* handle)
    {
        if (!handle)
            throw std::runtime_error("async scope is disposed");
        auto* holder = static_cast<AsyncScopeHandle>(handle);
        if (!*holder)
            throw std::runtime_error("async scope is disposed");
        return *holder;
    }

    inline void AsyncScopeDestroy(void* handle)
    {
        if (!handle)
            return;
        auto* holder = static_cast<AsyncScopeHandle>(handle);
        if (*holder)
            (*holder)->CancelAndWait();
        delete holder;
    }

    template<typename T>
    AsyncTask<T> AsyncScopeSpawn(void* handle, AsyncTask<T> task)
    {
        auto scope = RequireAsyncScope(handle);
        if (!scope->Add(task.SharedState()))
            throw std::runtime_error("cannot spawn into a closed async scope");
        task.Start();
        return task;
    }

    inline void AsyncScopeCancel(void* handle)
    {
        RequireAsyncScope(handle)->Cancel();
    }

    inline std::uint64_t AsyncScopeCount(void* handle)
    {
        return static_cast<std::uint64_t>(RequireAsyncScope(handle)->Count());
    }

    inline bool AsyncScopeClosed(void* handle)
    {
        return RequireAsyncScope(handle)->IsClosed();
    }

    inline bool AsyncScopeDeadline(void* handle, const std::uint64_t milliseconds)
    {
        std::weak_ptr<AsyncScopeState> weakScope = RequireAsyncScope(handle);
        return static_cast<bool>(DefaultAsyncScheduler().PostAfter(
            std::chrono::milliseconds(milliseconds),
            [weakScope]
            {
                if (auto scope = weakScope.lock())
                    scope->Cancel();
            }));
    }

    inline AsyncTask<void> AsyncScopeJoin(void* handle)
    {
        auto scope = RequireAsyncScope(handle);
        auto children = scope->CloseAndSnapshot();
        std::exception_ptr firstFailure;
        for (const auto& child : children)
        {
            try
            {
                co_await AsyncStateAwaiter{child};
            }
            catch (...)
            {
                if (!firstFailure)
                {
                    firstFailure = std::current_exception();
                    for (const auto& sibling : children)
                        sibling->Cancel();
                }
            }
        }
        scope->Clear();
        if (firstFailure)
            std::rethrow_exception(firstFailure);
    }

    struct AsyncMainAwaiter final
    {
        std::weak_ptr<detail::AsyncTaskStateBase> taskState;

        bool await_ready() const
        {
            return DefaultAsyncMainExecutor().IsCurrentThread();
        }

        template<typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> continuation)
        {
            if constexpr (requires(Promise& promise) { promise.state; })
                taskState = continuation.promise().state;

            auto registration = std::make_shared<detail::AsyncInlineContinuationRegistration>(continuation);
            if constexpr (requires(Promise& promise) { promise.state; })
            {
                if (auto state = taskState.lock())
                {
                    state->AddCancellationCallback([registration]
                    {
                        registration->ResumeOnce();
                    });
                }
            }

            // Arm before publishing to the owner queue. Unlike an ordinary
            // completion awaiter, observing the callback before Arm cannot
            // mean "continue here": the entire purpose of this awaiter is to
            // move the caller to the bound owner thread.
            if (!registration->Arm())
                return false;

            DefaultAsyncMainExecutor().Post([registration]
            {
                registration->ResumeOnce();
            });
            return true;
        }

        void await_resume() const
        {
            if (auto state = taskState.lock(); state && state->Cancelled())
                throw AsyncCancelled();
        }
    };

    struct AsyncScheduleAwaiter final
    {
        std::weak_ptr<detail::AsyncTaskStateBase> taskState;
        bool await_ready() const noexcept { return false; }
        template<typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> continuation)
        {
            if constexpr (requires(Promise& promise) { promise.state; })
                taskState = continuation.promise().state;

            auto registration = std::make_shared<detail::AsyncContinuationRegistration>(continuation);
            if constexpr (requires(Promise& promise) { promise.state; })
            {
                if (auto state = taskState.lock())
                {
                    state->AddCancellationCallback([registration]
                    {
                        registration->ResumeOnce();
                    });
                }
            }

            // Run/Yield promise a real scheduler boundary. If the worker can
            // claim the registration before it is armed, returning false from
            // await_suspend would continue the coroutine on its caller (often
            // the UI thread) and execute scheduled work in the wrong place.
            if (!registration->Arm())
                return false;

            if (!DefaultAsyncScheduler().Post([registration] { registration->ResumeOnce(); }))
                throw AsyncRuntimeStopped();
            return true;
        }
        void await_resume() const
        {
            if (auto state = taskState.lock(); state && state->Cancelled())
                throw AsyncCancelled();
        }
    };

    struct AsyncDelayAwaiter final
    {
        std::chrono::milliseconds delay;
        std::weak_ptr<detail::AsyncTaskStateBase> taskState;
        bool await_ready() const noexcept { return delay.count() == 0; }
        template<typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> continuation)
        {
            if constexpr (requires(Promise& promise) { promise.state; })
                taskState = continuation.promise().state;

            auto registration = std::make_shared<detail::AsyncContinuationRegistration>(continuation);
            auto timer = DefaultAsyncScheduler().PostAfter(
                delay, [registration] { registration->ResumeOnce(); });
            if (!timer)
                throw AsyncRuntimeStopped();

            if constexpr (requires(Promise& promise) { promise.state; })
            {
                if (auto state = taskState.lock())
                {
                    state->AddCancellationCallback([registration, timer]
                    {
                        timer->store(true, std::memory_order_release);
                        registration->ResumeOnce();
                    });
                }
            }
            return registration->Arm();
        }
        void await_resume() const
        {
            if (auto state = taskState.lock(); state && state->Cancelled())
                throw AsyncCancelled();
        }
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
    std::string AsyncFailureMessage(const AsyncTask<T>& task)
    {
        return task.SharedState()->FailureMessage();
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
    struct AsyncBlockingAwaiter final
    {
        std::function<T()> action;
        std::optional<T> value;
        std::exception_ptr failure;

        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> continuation)
        {
            auto& scheduler = DefaultAsyncBlockingScheduler();
            auto registration = std::make_shared<detail::AsyncContinuationRegistration>(continuation);
            if (!scheduler.Submit([this, registration]
                {
                    try { value.emplace(action()); }
                    catch (...) { failure = std::current_exception(); }
                    registration->ResumeOnce();
                }))
            {
                if (!scheduler.IsRunning())
                    throw AsyncRuntimeStopped();
                throw AsyncQueueFull();
            }
            return registration->Arm();
        }

        T await_resume()
        {
            if (failure)
                std::rethrow_exception(failure);
            return std::move(*value);
        }
    };

    template<>
    struct AsyncBlockingAwaiter<void> final
    {
        std::function<void()> action;
        std::exception_ptr failure;

        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> continuation)
        {
            auto& scheduler = DefaultAsyncBlockingScheduler();
            auto registration = std::make_shared<detail::AsyncContinuationRegistration>(continuation);
            if (!scheduler.Submit([this, registration]
                {
                    try { action(); }
                    catch (...) { failure = std::current_exception(); }
                    registration->ResumeOnce();
                }))
            {
                if (!scheduler.IsRunning())
                    throw AsyncRuntimeStopped();
                throw AsyncQueueFull();
            }
            return registration->Arm();
        }

        void await_resume()
        {
            if (failure)
                std::rethrow_exception(failure);
        }
    };

    template<typename T>
    AsyncTask<T> RunBlockingAsync(std::function<T()> action)
    {
        co_return co_await AsyncBlockingAwaiter<T>{std::move(action)};
    }

    inline AsyncTask<void> RunBlockingAsync(std::function<void()> action)
    {
        co_await AsyncBlockingAwaiter<void>{std::move(action)};
    }

    template<typename T>
    struct AsyncIoAwaiter final
    {
        std::function<T()> action;
        std::optional<T> value;
        std::exception_ptr failure;

        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> continuation)
        {
            auto& scheduler = DefaultAsyncIoScheduler();
            auto registration = std::make_shared<detail::AsyncContinuationRegistration>(continuation);
            if (!scheduler.Submit([this, registration]
                {
                    try { value.emplace(action()); }
                    catch (...) { failure = std::current_exception(); }
                    registration->ResumeOnce();
                }))
            {
                if (!scheduler.IsRunning())
                    throw AsyncRuntimeStopped();
                throw AsyncIoQueueFull();
            }
            return registration->Arm();
        }

        T await_resume()
        {
            if (failure)
                std::rethrow_exception(failure);
            return std::move(*value);
        }
    };

    template<>
    struct AsyncIoAwaiter<void> final
    {
        std::function<void()> action;
        std::exception_ptr failure;

        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> continuation)
        {
            auto& scheduler = DefaultAsyncIoScheduler();
            auto registration = std::make_shared<detail::AsyncContinuationRegistration>(continuation);
            if (!scheduler.Submit([this, registration]
                {
                    try { action(); }
                    catch (...) { failure = std::current_exception(); }
                    registration->ResumeOnce();
                }))
            {
                if (!scheduler.IsRunning())
                    throw AsyncRuntimeStopped();
                throw AsyncIoQueueFull();
            }
            return registration->Arm();
        }

        void await_resume()
        {
            if (failure)
                std::rethrow_exception(failure);
        }
    };

    template<typename T>
    AsyncTask<T> RunIoAsync(std::function<T()> action)
    {
        co_return co_await AsyncIoAwaiter<T>{std::move(action)};
    }

    inline AsyncTask<void> RunIoAsync(std::function<void()> action)
    {
        co_await AsyncIoAwaiter<void>{std::move(action)};
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

namespace wio::intrinsics
{
    template<typename T>
    inline runtime::AsyncTask<T> TaskStart(const runtime::AsyncTask<T>& task)
    {
        task.Start();
        return task;
    }

    template<typename T>
    inline void TaskCancel(const runtime::AsyncTask<T>& task)
    {
        task.Cancel();
    }

    template<typename T>
    inline bool TaskIsReady(const runtime::AsyncTask<T>& task)
    {
        return task.IsReady();
    }

    template<typename T>
    inline bool TaskIsCancelled(const runtime::AsyncTask<T>& task)
    {
        return task.IsCancelled();
    }

    template<typename T>
    inline bool TaskIsFaulted(const runtime::AsyncTask<T>& task)
    {
        return task.IsFaulted();
    }

    template<typename T>
    inline bool TaskWaitFor(const runtime::AsyncTask<T>& task, const std::uint64_t milliseconds)
    {
        return task.WaitFor(milliseconds);
    }

    template<typename T>
    inline T TaskBlock(const runtime::AsyncTask<T>& task)
    {
        return task.Get();
    }

    inline void TaskBlock(const runtime::AsyncTask<void>& task)
    {
        task.Get();
    }

    template<typename T>
    inline runtime::AsyncTask<void> TaskCancelAfter(
        const runtime::AsyncTask<T>& task,
        const std::uint64_t milliseconds)
    {
        return runtime::CancelAfter(task, milliseconds);
    }

    template<typename T>
    inline void TaskDetach(const runtime::AsyncTask<T>& task)
    {
        runtime::DetachAsync(task);
    }
}
