#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_abi.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace
{
    void check(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void qualify(const char* bytecodePath, const char* libraryPath)
    {
        std::ifstream input{std::filesystem::u8path(bytecodePath), std::ios::binary};
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "task input bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        std::string error;
        // A producer built against older ABI-v2 headers cannot release the new
        // source-slice union shape. Reject it before entering its thunk.
        const auto& echoFunction = module.functions[function("HostEcho")];
        std::uint32_t start = wio::bytecode::InvalidIndex;
        for (const auto& block : echoFunction.blocks)
            for (const auto& instruction : block.instructions)
                if (instruction.opcode == wio::bytecode::Opcode::NativeInvoke)
                    start = instruction.callee;
        check(start < module.functions.size(), "Start native binding was not serialized");
        const std::string key{module.string(module.functions[start].nativeBinding.stableKey)};
        bool legacyCalled = false;
        // No closures in ABI function pointers: the status uniquely proves
        // whether invocation occurred; an executed legacy thunk returns panic.
        const WioNativeAbiFunctionDescriptor legacyDescriptor{
            WIO_NATIVE_ABI_VERSION,
            0,
            0,
            key.c_str(),
            "legacy",
            "legacy",
            +[](const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*, WioNativeAbiFailure*) noexcept
            { return WIO_NATIVE_ABI_PANIC; }};
        const WioNativeAbiRegistry legacyRegistry{WIO_NATIVE_ABI_VERSION, 1, &legacyDescriptor};
        {
            wio::vm::Machine legacy{module};
            check(legacy.registerNativeRegistry(legacyRegistry, &error), error);
            auto ready = legacy.makeExternalTask();
            check(legacy.complete(ready, wio::vm::Value::signedInteger(7)), "legacy task fixture failed");
            const auto rejected = legacy.invoke(function("HostEcho"), std::span{&ready, 1});
            legacyCalled = rejected.succeeded() || rejected.error().code != "WVM1223" ||
                           rejected.error().message.find("task-source input support") == std::string::npos;
            check(!legacyCalled, "legacy task-source capability check failed: " +
                                     (rejected.succeeded() ? std::string{"unexpected success"}
                                                           : rejected.error().code + ": " + rejected.error().message));
        }
        check(!legacyCalled, "legacy producer received a task-source input without advertising support");
        wio::vm::Machine probe{module};
        check(probe.loadNativeLibrary(std::filesystem::u8path(libraryPath), &error), error);
        probe.bindMainExecutor();
        wio::vm::Value escaped;
        {
            wio::vm::Machine original{module};
            check(original.loadNativeLibrary(std::filesystem::u8path(libraryPath), &error), error);
            original.bindMainExecutor();
            const auto pending = original.invoke(function("HostKeepPending"));
            check(pending.succeeded() && pending.value().kind() == wio::vm::Value::Kind::AsyncTask,
                  "native code could not retain a pending VM task");
            escaped = pending.value();
            wio::vm::Machine moved{std::move(original)};
            const auto echo = moved.invoke(function("HostEcho"), std::span{&escaped, 1});
            check(echo.succeeded() && echo.value() == escaped,
                  "native Start did not preserve VM task identity after Machine move");
            const auto recovered = moved.invoke(function("HostReturnKept"));
            check(recovered.succeeded() && recovered.value() == escaped, "retained task origin was lost");
            const auto foreign = probe.invoke(function("HostReturnKept"));
            check(!foreign.succeeded() &&
                      foreign.error().message.find("different machine/generation") != std::string::npos,
                  "a task source crossed Machine/generation ownership without rejection");
        }
        check(escaped.taskState() == wio::vm::AsyncTaskState::Cancelled,
              "Machine shutdown did not cancel the retained source task");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        bool cancelled = false;
        do
        {
            const auto result = probe.invoke(function("HostKeptCancelled"));
            check(result.succeeded(), "retained producer task query failed after Machine shutdown");
            cancelled = result.value().asBoolean();
            if (!cancelled)
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
        } while (!cancelled && std::chrono::steady_clock::now() < deadline);
        check(cancelled, "native task wrapper outlived shutdown without a terminal cancellation");
        check(probe.invoke(function("HostDropKept")).succeeded(), "retained task could not be released safely");
        escaped = {};
        wio::vm::Machine::collectNativeObjectRetirements();
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "expected bytecode and producer paths");
        qualify(argv[1], argv[2]);
        std::cout << "native-task-input-host-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
