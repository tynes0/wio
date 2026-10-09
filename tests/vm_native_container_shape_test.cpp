#include "native_container_copy_back_internal.h"
#include "native_dictionary_internal.h"
#include "native_sequence_internal.h"
#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_any.h>
#include <wio_native_dictionary.h>
#include <wio_native_object.h>
#include <wio_native_pod.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    using Value = wio::vm::Value;
    namespace bridge = wio::vm::detail;

    void check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    void contracts()
    {
        auto old = Value::dictionary(
            7, {Value::string("keep"), Value::signedInteger(3), Value::string("gone"), Value::signedInteger(7)}, false);
        auto next = Value::dictionary(
            7, {Value::string("new"), Value::signedInteger(11), Value::string("keep"), Value::signedInteger(41)},
            false);
        std::vector<std::size_t> mapping;
        constexpr auto Missing = (std::numeric_limits<std::size_t>::max)();
        check(bridge::nativeDictionaryCopyBackMapping(old, next, mapping) &&
                  mapping == std::vector<std::size_t>{1, Missing},
              "surviving dictionary keys were not mapped");
        mapping = {91};
        check(!bridge::nativeDictionaryCopyBackOrder(old, next, mapping) && mapping.front() == 91,
              "same-shape predicate accepted a replaced key or published a partial mapping");
        std::vector<bridge::NativeCopyBackAction> actions;
        std::string error;
        check(bridge::planNativeCopyBack(old, next, actions, error) && actions.size() == 2 &&
                  actions.back().kind == bridge::NativeCopyBackAction::Kind::RebuildDictionary &&
                  old.dictionaryValue(0)->asSignedInteger() == 3 && next.dictionaryValue(1)->asSignedInteger() == 41,
              "dictionary planning mutated the graph or failed child-before-parent ordering");
        {
            const auto snapshot = old;
            actions.clear();
            check(!bridge::planNativeCopyBack(old, next, actions, error) &&
                      error.find("exclusive storage") != std::string::npos && old.dictionaryCount() == 2,
                  "shared dictionary storage was structurally replaced");
        }
        auto numbers = Value::array({Value::signedInteger(7)});
        auto grown = Value::array({Value::signedInteger(7), Value::signedInteger(19)});
        actions.clear();
        check(bridge::planNativeCopyBack(numbers, grown, actions, error) && actions.size() == 1 &&
                  numbers.elementCount() == 1,
              "array planning mutated caller storage");
        {
            const auto snapshot = numbers;
            actions.clear();
            check(!bridge::planNativeCopyBack(numbers, grown, actions, error),
                  "structural array update accepted a storage snapshot");
        }
        auto record = Value::component(9, {Value::array({Value::signedInteger(3)})});
        check(bridge::containsNativeContainerCell(record, record.field(0)->element(0)),
              "physical container cell traversal lost a component descendant");
        check(!bridge::containsNativeContainerCell(record, numbers.element(0)),
              "cell traversal confused unrelated equal values");
        actions.clear();
        check(!bridge::planNativeCopyBack(numbers, grown, actions, error, 65) && actions.empty(),
              "copy-back depth guard published an action");
        auto wrong = Value::dictionary(8, {Value::string("keep"), Value::signedInteger(41)}, false);
        actions.clear();
        check(!bridge::planNativeCopyBack(old, wrong, actions, error) && actions.empty(),
              "dictionary type mismatch reached commit planning");
    }

    struct Library final
    {
#if defined(_WIN32)
        HMODULE handle;
        explicit Library(const char* path) : handle(LoadLibraryW(std::filesystem::u8path(path).c_str()))
        {
        }
        ~Library()
        {
            if (handle)
                FreeLibrary(handle);
        }
        template <class T> T symbol(const char* name)
        {
            const auto result = GetProcAddress(handle, name);
#else
        void* handle;
        explicit Library(const char* path) : handle(dlopen(path, RTLD_NOW | RTLD_LOCAL))
        {
        }
        ~Library()
        {
            if (handle)
                dlclose(handle);
        }
        template <class T> T symbol(const char* name)
        {
            const auto result = dlsym(handle, name);
#endif
            check(result != nullptr, name);
            return reinterpret_cast<T>(result);
        }
        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;
    };

    WioNativeAbiThunk original;
    WioNativeAbiStatus corrupt(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure* failure) noexcept
    {
        const auto status = original(args, count, result, failure);
        if (status == WIO_NATIVE_ABI_OK && count == 2)
        {
            auto* output = static_cast<WioNativeAbiValue*>(args[1].payload.mutableSlice.data);
            WioNativeAbiReleaseValue(output);
            *output = {};
            output->kind = WIO_NATIVE_ABI_I64;
        }
        return status;
    }

    void host(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "container shape bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "container producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto dicts = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        const auto defaults = library.symbol<WioGetNativePodDefaultApiFn>("WioGetNativePodDefaultApi")();
        const auto install = [&](wio::vm::Machine& machine, const WioNativeAbiRegistry& selected)
        {
            std::string error;
            check(machine.registerNativeRegistry(selected, *objects, &error, any, dicts, sequences, pods, defaults),
                  error.c_str());
        };
        wio::vm::Machine machine{module};
        install(machine, *registry);
        const auto result = machine.invoke(function("Entry"));
        if (!result.succeeded())
            throw std::runtime_error(result.error().code + ": " + result.error().message);
        check(result.value().asSignedInteger() == 0, "container shape live-host entry failed");
        for (const auto* name : {"ExpiredArray", "ExpiredPod", "ExpiredKey", "ExpiredNested", "ExpiredHeapArray"})
        {
            const auto expired = machine.invoke(function(name));
            check(!expired.succeeded() && expired.error().code == "WVM1043", name);
            check(machine.invoke(function("SafePoint")).succeeded(), "VM failed to recover after invalid borrow");
        }
        const auto overlap = machine.invoke(function("OverlapProbe"));
        check(!overlap.succeeded() && overlap.error().code == "WVM1225", "overlapping container projections escaped");
        const auto thrown = machine.invoke(function("FailureProbe"));
        check(!thrown.succeeded() && thrown.error().code == "WVM1222" &&
                  thrown.error().message.find("shape-failure-oracle") != std::string::npos,
              "native exception boundary lost structural failure");
        const auto cleanup = machine.invoke(function("CleanupProbe"));
        check(!cleanup.succeeded() && cleanup.error().code == "WVM1211" && !cleanup.error().cleanupFailures.empty(),
              "structural copy-back swallowed a failing object destructor");
        const auto balanced = machine.invoke(function("Balanced"));
        check(balanced.succeeded() && balanced.value().asBoolean(),
              "failed copy-back cleanup leaked or repeated object destruction");
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        const auto& binding = module.functions[function("Pair")].nativeBinding;
        const auto selected = std::ranges::find_if(descriptors, [&](const auto& value)
                                                   { return value.stableKey == module.string(binding.stableKey); });
        check(selected != descriptors.end(), "pair native descriptor missing");
        original = selected->thunk;
        selected->thunk = &corrupt;
        auto invalidRegistry = *registry;
        invalidRegistry.functions = descriptors.data();
        // Promote only the two probe roots to globals so exceptional frame
        // cleanup cannot hide an accidental partial copy-back.
        auto observable = module;
        check(observable.globals.empty(), "atomicity fixture unexpectedly has globals");
        auto& probe = observable.functions[function("PairProbe")];
        std::vector<std::uint32_t> roots;
        std::vector<std::uint32_t> referenceTypes;
        for (auto& block : probe.blocks)
            for (auto& instruction : block.instructions)
                if (instruction.opcode == wio::bytecode::Opcode::LocalPlace)
                {
                    const auto& reference = observable.types[instruction.resultType];
                    check(reference.kind == 29 && reference.arguments.size() == 1, "probe root is not a reference");
                    const auto kind = observable.types[reference.arguments.front()].kind;
                    if (kind != 31 && kind != 32)
                        continue;
                    instruction.opcode = wio::bytecode::Opcode::GlobalPlace;
                    instruction.global = static_cast<std::uint32_t>(observable.globals.size());
                    observable.globals.push_back({.id = instruction.global,
                                                  .name = probe.name,
                                                  .type = reference.arguments.front(),
                                                  .flags = 1});
                    roots.push_back(instruction.result);
                    referenceTypes.push_back(instruction.resultType);
                }
        check(roots.size() == 2, "atomicity probe roots missing");
        for (auto& block : probe.blocks)
        {
            for (auto& instruction : block.instructions)
                if (instruction.opcode == wio::bytecode::Opcode::PlaceInit &&
                    std::ranges::find(roots, instruction.operands.front()) != roots.end())
                    instruction.opcode = wio::bytecode::Opcode::Store;
            std::erase_if(block.instructions,
                          [&](const auto& instruction)
                          {
                              return instruction.opcode == wio::bytecode::Opcode::DropPlace &&
                                     std::ranges::find(roots, instruction.operands.front()) != roots.end();
                          });
        }
        const auto readerStart = static_cast<std::uint32_t>(observable.functions.size());
        for (std::uint32_t index = 0; index < 2; ++index)
        {
            wio::bytecode::Function reader;
            reader.id = static_cast<std::uint32_t>(observable.functions.size());
            reader.name = observable.functions[function("SafePoint")].name;
            reader.returnType = observable.globals[index].type;
            wio::bytecode::Instruction place;
            place.opcode = wio::bytecode::Opcode::GlobalPlace;
            place.result = 0;
            place.resultType = referenceTypes[index];
            place.global = index;
            place.constant = 0;
            wio::bytecode::Instruction load;
            load.opcode = wio::bytecode::Opcode::Load;
            load.result = 1;
            load.resultType = reader.returnType;
            load.operands = {0};
            load.constant = 0;
            wio::bytecode::Instruction returned;
            returned.opcode = wio::bytecode::Opcode::Return;
            returned.operands = {1};
            returned.constant = 0;
            reader.blocks = {{.id = 0, .instructions = {place, load, returned}}};
            observable.functions.push_back(std::move(reader));
        }
        wio::vm::Machine invalid{observable};
        install(invalid, invalidRegistry);
        const auto rejected = invalid.invoke(function("PairProbe"));
        check(!rejected.succeeded(), "invalid second output reached copy-back");
        if (rejected.error().code != "WVM1224")
            throw std::runtime_error(rejected.error().code + ": " + rejected.error().message);
        const auto first = invalid.invoke(readerStart);
        const auto second = invalid.invoke(readerStart + 1);
        check(first.succeeded() && first.value().elementCount() == 1 &&
                  first.value().element(0)->asSignedInteger() == 7 && second.succeeded() &&
                  second.value().dictionaryCount() == 1 && second.value().dictionaryValue(0)->asSignedInteger() == 11,
              "malformed second output partially committed caller storage");
        check(invalid.invoke(function("SafePoint")).succeeded(), "malformed-output VM did not recover");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "container shape qualification requires bytecode and producer");
        contracts();
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
