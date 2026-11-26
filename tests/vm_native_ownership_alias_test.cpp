#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"
#include "wio/vm/machine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    void check(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void qualify(const char* bytecodePath, const char* libraryPath)
    {
        std::ifstream input(std::filesystem::u8path(bytecodePath), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "ownership bytecode decode failed");
        auto& module = decoded.module;
        for (const std::uint8_t kind : {29, 30, 20, 33})
        {
            auto forged = module;
            auto consumed = std::ranges::find_if(forged.functions,
                                                 [](const auto& candidate)
                                                 {
                                                     return candidate.hasNativeBinding &&
                                                            !candidate.nativeBinding.parameters.empty() &&
                                                            candidate.nativeBinding.parameters.front().passing == 3;
                                                 });
            check(consumed != forged.functions.end(), "consumed binding metadata missing");
            const auto original = consumed->nativeBinding.parameters.front().type;
            wio::bytecode::Type type;
            type.kind = kind;
            type.arguments = {original};
            if (kind == 30)
            {
                auto reference = type;
                reference.kind = 29;
                type.arguments = {static_cast<std::uint32_t>(forged.types.size())};
                forged.types.push_back(std::move(reference));
            }
            consumed->nativeBinding.parameters.front().type = static_cast<std::uint32_t>(forged.types.size());
            consumed->nativeBinding.parameters.front().marshalling = 1;
            forged.types.push_back(std::move(type));
            const auto rejected = wio::bytecode::Verifier{}.verify(forged);
            check(std::ranges::any_of(rejected.diagnostics(),
                                      [](const auto& diagnostic) { return diagnostic.code == "WBC1074"; }),
                  "forged consumed bytecode bypassed its ownership contract");
        }
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        wio::vm::Value escaped;
        {
            wio::vm::Machine machine{module};
            std::string error;
            check(machine.loadNativeLibrary(std::filesystem::u8path(libraryPath), &error), error);
            const auto balanced = [&]
            {
                const auto result = machine.invoke(function("HostBalanced"));
                check(result.succeeded() && result.value().asBoolean(), "foreign resource claims leaked");
            };
            for (const char* name : {"HostFailure", "HostContainerFailure"})
            {
                const auto result = machine.invoke(function(name));
                check(!result.succeeded() && result.error().code == "WVM1222" &&
                          result.error().message.find("ownership native failure") != std::string::npos,
                      "consumed native failure was lost");
                balanced();
            }
            const auto aliases = machine.invoke(function("HostAliasFailure"));
            check(!aliases.succeeded() && aliases.error().code == "WVM1222" &&
                      aliases.error().message.find("alias native failure") != std::string::npos,
                  "alias failure boundary was lost: " + (aliases.succeeded()
                                                             ? std::string{"unexpected success"}
                                                             : aliases.error().code + ": " + aliases.error().message));
            const auto read = machine.invoke(function("HostReadAlias"));
            check(read.succeeded() && read.value().asSignedInteger() == 37,
                  "overlapping projection exception copy-back lost the canonical cell");
            for (int round = 0; round < 40; ++round)
            {
                const auto pending = machine.invoke(function("HostPending"));
                check(pending.succeeded(), "owned resource coroutine construction failed");
                check(machine.cancel(pending.value()), "owned resource coroutine cancellation failed");
                const auto cancelled = machine.wait(pending.value());
                check(!cancelled.succeeded() && cancelled.error().code == "WVM1165", "cancelled task resumed");
                balanced();
                const auto ready = machine.invoke(function("HostReady"));
                check(ready.succeeded() && ready.value().asSignedInteger() == 59, "consumed resource retention failed");
                balanced();
            }
            const auto native = machine.invoke(function("HostMakeNative"));
            check(native.succeeded() && native.value().kind() == wio::vm::Value::Kind::ForeignObject,
                  "producer-owned object was not preserved");
            escaped = native.value();
        }
        // No Machine or loader remains here. The escaped value must pin the
        // producer until its final intrusive claim (and destructor) retires.
        const auto handle = escaped.foreignHandle();
        check(handle.state && handle.ops && handle.ops->typeId(handle.state) == escaped.foreignTypeId(),
              "foreign result lost its producer after loader shutdown");
        handle.ops->retain(handle.state);
        handle.ops->release(handle.state);
        wio::vm::Machine probe{module};
        std::string error;
        check(probe.loadNativeLibrary(std::filesystem::u8path(libraryPath), &error), error);
        const auto live = probe.invoke(function("HostOutstanding"));
        check(live.succeeded() && live.value().asSignedInteger() == 1, "escaped native resource was destroyed early");
        escaped = {};
        const auto closed = probe.invoke(function("HostBalanced"));
        check(closed.succeeded() && closed.value().asBoolean(), "escaped native resource did not release exactly once");
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "expected bytecode and producer paths");
        qualify(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
