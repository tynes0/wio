#include "wio/vm/machine.h"

#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;
    using namespace std::chrono_literals;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        return module;
    }
} // namespace

int main()
{
    bool ok = true;
    bytecode::Module module = makeModule();
    vm::Machine machine{module};

    const vm::Value external = machine.makeExternalTask();
    std::thread producer{[&]
                         {
                             std::this_thread::sleep_for(5ms);
                             if (!machine.complete(external, vm::Value::signedInteger(42)))
                                 std::cerr << "External completion unexpectedly lost its state race\n";
                         }};
    const vm::ExecutionResult externalResult = machine.wait(external);
    producer.join();
    ok &= expect(externalResult.succeeded() && externalResult.value().asSignedInteger() == 42,
                 "A foreign thread must be able to complete a pending VM task");
    ok &= expect(external.taskState() == vm::AsyncTaskState::Ready, "External completion must publish the ready state");
    ok &= expect(!machine.complete(external, vm::Value::signedInteger(7)), "Task completion must be exactly-once");

    const vm::Value failed = machine.makeExternalTask();
    ok &= expect(machine.fail(failed, "HOST0001", "host operation failed"),
                 "A pending external task must accept a host failure");
    const vm::ExecutionResult failedResult = machine.wait(failed);
    ok &= expect(!failedResult.succeeded() && failedResult.error().code == "HOST0001" &&
                     failedResult.error().message == "host operation failed",
                 "External failures must cross the wait boundary unchanged");

    const vm::Value cancelled = machine.makeExternalTask();
    ok &= expect(machine.cancel(cancelled), "A pending external task must accept cancellation");
    ok &= expect(!machine.fail(cancelled, "HOST0002", "too late"),
                 "A cancelled task must reject later failure publication");
    const vm::ExecutionResult cancelledResult = machine.wait(cancelled);
    ok &= expect(!cancelledResult.succeeded() && cancelledResult.error().code == "WVM1165",
                 "Cancellation must wake a task waiter with the stable VM diagnostic");

    const vm::Value immediate = machine.sleepFor(0ns);
    ok &= expect(immediate.taskState() == vm::AsyncTaskState::Ready && machine.wait(immediate).succeeded(),
                 "A non-positive timer must complete without entering the scheduler queue");

    std::vector<vm::Value> timers;
    timers.reserve(32);
    for (std::size_t index = 0; index < 32; ++index)
        timers.push_back(machine.sleepFor(2ms));
    for (const vm::Value& timer : timers)
        ok &= expect(machine.wait(timer).succeeded(), "Shared timer-queue work must complete successfully");

    const vm::Value distant = machine.sleepFor(1h);
    ok &= expect(machine.cancel(distant) && distant.taskState() == vm::AsyncTaskState::Cancelled,
                 "Timer cancellation must immediately remove distant work from the shared queue");

    vm::Value abandoned;
    {
        vm::Machine scoped{module};
        abandoned = scoped.sleepFor(1h);
    }
    ok &= expect(abandoned.taskState() == vm::AsyncTaskState::Cancelled,
                 "Machine shutdown must cancel timers instead of waiting for distant deadlines");
    return ok ? 0 : 1;
}
