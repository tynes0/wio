#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace wio::runtime::std_concurrency
{
    struct MutexHandle final { std::recursive_mutex value; };
    inline void* MutexCreate() { return new MutexHandle(); }
    inline void MutexDestroy(void* handle) noexcept { delete static_cast<MutexHandle*>(handle); }
    inline void MutexLock(void* handle) { static_cast<MutexHandle*>(handle)->value.lock(); }
    inline bool MutexTryLock(void* handle) { return static_cast<MutexHandle*>(handle)->value.try_lock(); }
    inline void MutexUnlock(void* handle) { static_cast<MutexHandle*>(handle)->value.unlock(); }

    struct ConditionHandle final { std::condition_variable_any value; };
    inline void* ConditionCreate() { return new ConditionHandle(); }
    inline void ConditionDestroy(void* handle) noexcept { delete static_cast<ConditionHandle*>(handle); }
    inline void ConditionWait(void* handle, void* mutexHandle)
    {
        auto lock = std::unique_lock<std::recursive_mutex>(
            static_cast<MutexHandle*>(mutexHandle)->value, std::adopt_lock);
        static_cast<ConditionHandle*>(handle)->value.wait(lock);
        static_cast<void>(lock.release());
    }
    inline bool ConditionWaitFor(void* handle, void* mutexHandle, const std::uint64_t milliseconds)
    {
        auto lock = std::unique_lock<std::recursive_mutex>(
            static_cast<MutexHandle*>(mutexHandle)->value, std::adopt_lock);
        const bool signaled = static_cast<ConditionHandle*>(handle)->value.wait_for(
            lock, std::chrono::milliseconds(milliseconds)) != std::cv_status::timeout;
        static_cast<void>(lock.release());
        return signaled;
    }
    inline void ConditionNotifyOne(void* handle) noexcept
    {
        static_cast<ConditionHandle*>(handle)->value.notify_one();
    }
    inline void ConditionNotifyAll(void* handle) noexcept
    {
        static_cast<ConditionHandle*>(handle)->value.notify_all();
    }

    struct AtomicI64Handle final { explicit AtomicI64Handle(std::int64_t initial) : value(initial) {} std::atomic<std::int64_t> value; };
    inline void* AtomicI64Create(const std::int64_t initial) { return new AtomicI64Handle(initial); }
    inline void AtomicI64Destroy(void* handle) noexcept { delete static_cast<AtomicI64Handle*>(handle); }
    inline std::int64_t AtomicI64Load(void* handle) noexcept { return static_cast<AtomicI64Handle*>(handle)->value.load(); }
    inline void AtomicI64Store(void* handle, const std::int64_t value) noexcept { static_cast<AtomicI64Handle*>(handle)->value.store(value); }
    inline std::int64_t AtomicI64Exchange(void* handle, const std::int64_t value) noexcept { return static_cast<AtomicI64Handle*>(handle)->value.exchange(value); }
    inline std::int64_t AtomicI64Add(void* handle, const std::int64_t value) noexcept { return static_cast<AtomicI64Handle*>(handle)->value.fetch_add(value) + value; }
    inline bool AtomicI64CompareExchange(void* handle, std::int64_t& expected, const std::int64_t desired) noexcept
    {
        return static_cast<AtomicI64Handle*>(handle)->value.compare_exchange_strong(expected, desired);
    }

    struct ThreadHandle final
    {
        explicit ThreadHandle(std::function<void()> action) : value(std::move(action)) {}
        ~ThreadHandle() { if (value.joinable()) value.join(); }
        std::thread value;
    };
    inline void* ThreadStart(std::function<void()> action) { return new ThreadHandle(std::move(action)); }
    inline bool ThreadJoinable(void* handle) noexcept { return static_cast<ThreadHandle*>(handle)->value.joinable(); }
    inline void ThreadJoin(void* handle) { if (static_cast<ThreadHandle*>(handle)->value.joinable()) static_cast<ThreadHandle*>(handle)->value.join(); }
    inline void ThreadDetach(void* handle) { if (static_cast<ThreadHandle*>(handle)->value.joinable()) static_cast<ThreadHandle*>(handle)->value.detach(); }
    inline void ThreadDestroy(void* handle) noexcept { delete static_cast<ThreadHandle*>(handle); }
    inline void SleepMilliseconds(const std::uint64_t milliseconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
    inline void Yield() noexcept { std::this_thread::yield(); }
}
