#include <wio/vm/machine.h>
#include <wio_module_contract.h>

#include <iostream>
#include <thread>

namespace
{
    namespace bc = wio::bytecode;
    using wio::vm::Value;

    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::shared_ptr<const bc::Module> fixture()
    {
        bc::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {bc::Constant{}};
        module.strings = {"void", "i32", "string", "coroutine<i32>", "coroutine<string>"};
        module.types = {bc::Type{.kind = 1}, bc::Type{.kind = 5}, bc::Type{.kind = 17},
                        bc::Type{.kind = 34, .arguments = {1}}, bc::Type{.kind = 34, .arguments = {2}}};
        for (std::uint32_t index = 0; index < module.types.size(); ++index)
            module.contract.reflection.push_back(bc::Reflection{
                .stableTypeId = WioStableSdkId(module.strings[index].c_str()), .logicalName = index, .type = index});
        return std::make_shared<const bc::Module>(std::move(module));
    }
} // namespace

int main()
{
    try
    {
        for (unsigned iteration = 0; iteration < 100; ++iteration)
        {
            auto module = fixture();
            std::weak_ptr<const bc::Module> weak = module;
            wio::vm::Machine machine{module};
            wio::vm::Machine foreign{module};
            auto task = machine.makeExternalTask();
            WioNativeAbiValue wire{};
            std::string error;
            expect(machine.exportTaskSource(task, 3, wire, error), error.c_str());
            Value imported;
            expect(machine.importTaskSource(wire, 3, imported, error), error.c_str());
            expect(imported.taskState() == wio::vm::AsyncTaskState::Pending, "task import implicitly waited");
            Value rejected;
            expect(!foreign.importTaskSource(wire, 3, rejected, error) && rejected.isEmpty(),
                   "task import crossed VM generations");
            expect(!machine.importTaskSource(wire, 4, rejected, error) && rejected.isEmpty(),
                   "task import accepted the wrong payload identity");
            auto borrowed = wire;
            borrowed.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_TASK_SOURCE;
            expect(!machine.importTaskSource(borrowed, 3, rejected, error), "borrowed task input was accepted");
            Value occupied = Value::signedInteger(99);
            expect(!machine.importTaskSource(wire, 3, occupied, error) && occupied.asSignedInteger() == 99,
                   "task import overwrote a nonempty output");
            expect(machine.complete(imported, Value::signedInteger(42)) &&
                       task.taskState() == wio::vm::AsyncTaskState::Ready && task.taskResult().asSignedInteger() == 42,
                   "task input was cloned into a separate task state");
            expect(!machine.complete(task, Value::signedInteger(7)), "completion was not exactly once");
            auto answer = machine.wait(imported);
            expect(answer.succeeded() && answer.value().asSignedInteger() == 42, "imported result was lost");
            machine.shutdown();
            expect(!machine.importTaskSource(wire, 3, rejected, error), "closed VM accepted a task input");
            foreign.shutdown();
            module.reset();
            expect(!weak.expired(), "escaped source lost immutable metadata");
            std::thread release{[&] { WioNativeAbiReleaseValue(&wire); }};
            release.join();
            expect(weak.expired(), "task import leaked a machine or source-code lease");
            expect(answer.value().asSignedInteger() == 42, "closed machine invalidated a decoded result");
        }
        std::cout << "task-source-import-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
