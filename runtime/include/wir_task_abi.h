#pragma once

#include "wir_abi.h"
#include <unordered_map>

namespace wio::wir_backend::abi
{
    class TaskSourceOwner final
    {
    public:
        explicit TaskSourceOwner(const WioNativeAbiValue& wire) : wire_(wire)
        {
            WioNativeAbiRetainValue(&wire_);
        }
        TaskSourceOwner(const TaskSourceOwner& other) : TaskSourceOwner(other.wire_)
        {
        }
        TaskSourceOwner(TaskSourceOwner&& other) noexcept : wire_(std::exchange(other.wire_, {}))
        {
        }
        ~TaskSourceOwner()
        {
            WioNativeAbiReleaseValue(&wire_);
        }
        const WioNativeTaskSource& source() const
        {
            return *static_cast<const WioNativeTaskSource*>(wire_.payload.slice.data);
        }
        WioNativeAbiValue copyWire() const
        {
            auto value = wire_;
            WioNativeAbiRetainValue(&value);
            return value;
        }

    private:
        WioNativeAbiValue wire_{};
    };

    struct TaskSourceAwaiter
    {
        struct Completion
        {
            std::shared_ptr<runtime::detail::AsyncContinuationRegistration> continuation;
            std::atomic<WioNativeAbiStatus> status{WIO_NATIVE_ABI_NOT_READY};
        };
        TaskSourceOwner owner;
        std::shared_ptr<Completion> completion = std::make_shared<Completion>();
        std::weak_ptr<runtime::detail::AsyncTaskStateBase> parent;

        bool await_ready() const
        {
            bool ready = false;
            const auto status = owner.source().ready(owner.source().state, &ready);
            require(status == WIO_NATIVE_ABI_OK, status);
            // Subscribe even to terminal sources so cancellation is reflected
            // in the materialized task's state, not merely as an exception.
            return false;
        }

        template <class Promise> bool await_suspend(std::coroutine_handle<Promise> handle)
        {
            parent = handle.promise().state;
            completion->continuation = std::make_shared<runtime::detail::AsyncContinuationRegistration>(
                handle, runtime::detail::CoroutineOwner(handle));
            auto* subscription = new std::shared_ptr<Completion>(completion);
            const auto callback = +[](void* state, WioNativeAbiStatus status) noexcept
            {
                std::unique_ptr<std::shared_ptr<Completion>> subscription{
                    static_cast<std::shared_ptr<Completion>*>(state)};
                (*subscription)->status.store(status, std::memory_order_release);
                (*subscription)->continuation->ResumeOnce();
            };
            const auto status = owner.source().onComplete(owner.source().state, callback, subscription);
            if (status != WIO_NATIVE_ABI_OK)
            {
                delete subscription;
                require(false, status);
            }
            handle.promise().state->AddCancellationCallback(
                [source = owner, value = completion]
                {
                    (void)source.source().cancel(source.source().state);
                    value->status.store(WIO_NATIVE_ABI_CANCELLED, std::memory_order_release);
                    value->continuation->ResumeOnce();
                });
            return completion->continuation->Arm();
        }

        void await_resume() const
        {
            if (auto state = parent.lock(); state && state->Cancelled())
                throw runtime::AsyncCancelled();
            if (completion->status.load(std::memory_order_acquire) == WIO_NATIVE_ABI_CANCELLED)
            {
                if (auto state = parent.lock())
                    state->Cancel();
                throw runtime::AsyncCancelled();
            }
        }
    };

    template <class T, std::uint64_t TaskId, std::uint64_t ResultId>
    runtime::AsyncTask<T> materializeTaskSource(std::shared_ptr<TaskSourceOwner> source)
    {
        co_await TaskSourceAwaiter{*source};
        WioNativeAbiValue result{};
        struct Guard
        {
            WioNativeAbiValue& value;
            ~Guard()
            {
                WioNativeAbiReleaseValue(&value);
            }
        } guard{result};
        WioNativeAbiFailure failure{};
        const auto status = source->source().read(source->source().state, &result, &failure);
        if (status == WIO_NATIVE_ABI_CANCELLED)
            throw runtime::AsyncCancelled();
        if (status != WIO_NATIVE_ABI_OK)
            throw std::runtime_error(
                failure.message.data ? std::string(static_cast<const char*>(failure.message.data), failure.message.size)
                                     : "native task source read failed");
        if constexpr (std::is_void_v<T>)
        {
            require(result.kind == WIO_NATIVE_ABI_VOID);
            co_return;
        }
        else
            co_return Codec<T, ResultId>::read(result);
    }

    template <class T, std::uint64_t TaskId, std::uint64_t ResultId> struct TaskCodec
    {
        using Task = runtime::AsyncTask<T>;
        struct Origin
        {
            std::weak_ptr<runtime::detail::AsyncTaskStateBase> task;
            std::weak_ptr<TaskSourceOwner> source;
        };
        inline static std::mutex originMutex;
        inline static std::unordered_map<const void*, Origin> origins;
        static Task read(const WioNativeAbiValue& wire)
        {
            require(wire.kind == WIO_NATIVE_ABI_RUNTIME_VALUE && wire.typeId == TaskId);
            if (!(wire.flags & WIO_NATIVE_ABI_VALUE_TASK_SOURCE))
            {
                require(wire.payload.handle.state && wire.payload.handle.ops == &Box<Task, TaskId>::ops);
                return static_cast<Box<Task, TaskId>*>(wire.payload.handle.state)->value;
            }
            require(wire.flags == (WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_TASK_SOURCE) && wire.owner.state &&
                    wire.owner.ops && wire.owner.ops->retain && wire.owner.ops->release &&
                    wire.owner.ops->abiVersion == WIO_NATIVE_ABI_VERSION && !wire.owner.ops->reserved &&
                    wire.owner.ops->typeId && wire.owner.ops->typeId(wire.owner.state) == TaskId &&
                    wire.payload.slice.data && wire.payload.slice.size == sizeof(WioNativeTaskSource) &&
                    reinterpret_cast<std::uintptr_t>(wire.payload.slice.data) % alignof(WioNativeTaskSource) == 0);
            const auto& source = *static_cast<const WioNativeTaskSource*>(wire.payload.slice.data);
            require(source.abiVersion == WIO_NATIVE_ABI_VERSION && !source.reserved && source.taskTypeId == TaskId &&
                    source.resultTypeId == ResultId && source.state && source.ready && source.cancel && source.read &&
                    source.onComplete && source.pumpMain);
            auto sourceOwner = std::make_shared<TaskSourceOwner>(wire);
            auto task = materializeTaskSource<T, TaskId, ResultId>(sourceOwner);
            auto pump = [sourceOwner]
            {
                std::uint64_t drained = 0;
                const auto& source = sourceOwner->source();
                require(source.pumpMain(source.state, &drained) == WIO_NATIVE_ABI_OK, WIO_NATIVE_ABI_STALE_HANDLE);
            };
            task.SharedState()->foreignWaitPump = pump;
            runtime::detail::AsyncForeignWaitScope::Add(std::move(pump));
            {
                std::scoped_lock lock{originMutex};
                std::erase_if(origins, [](const auto& entry) { return entry.second.task.expired(); });
                origins.insert_or_assign(task.SharedState().get(), Origin{task.SharedState(), sourceOwner});
            }
            Tasks::get().track(task);
            return task;
        }
        static WioNativeAbiValue write(Task task)
        {
            // Identity-preserving helpers (Start, echo, collection copies)
            // must return the original host task, not a second cancellable VM
            // adapter. Both lookup references are weak: no DLL/task cycle.
            {
                std::scoped_lock lock{originMutex};
                const auto found = origins.find(task.SharedState().get());
                if (found != origins.end())
                    if (auto source = found->second.source.lock())
                        return source->copyWire();
            }
            auto* box = new Box<Task, TaskId>(std::move(task));
            WioNativeAbiValue wire{};
            wire.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
            wire.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            wire.typeId = TaskId;
            wire.payload.handle = {box, &Box<Task, TaskId>::ops, 1};
            return wire;
        }
    };
} // namespace wio::wir_backend::abi
