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
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    void check(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    struct Library
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
            const auto address = GetProcAddress(handle, name);
            if (!address)
                throw std::runtime_error(std::string{"missing producer symbol: "} + name);
            return reinterpret_cast<T>(address);
        }
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
            const auto address = dlsym(handle, name);
            if (!address)
                throw std::runtime_error(std::string{"missing producer symbol: "} + name);
            return reinterpret_cast<T>(address);
        }
#endif
        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;
    };

    WioNativeAbiThunk original;
    unsigned corruption;

    WioNativeAbiStatus corrupt(const WioNativeAbiValue* arguments, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure* failure) noexcept
    {
        const auto status = original(arguments, count, result, failure);
        if (status != WIO_NATIVE_ABI_OK)
            return status;
        switch (corruption)
        {
        case 0:
            result->payload.mutableSlice.data = reinterpret_cast<void*>(std::uintptr_t{1});
            break;
        case 1:
            result->typeId ^= 0x100;
            break;
        case 2:
            result->flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            break;
        case 3:
            result->payload.mutableSlice.size -= 1;
            break;
        case 4:
            result->owner.generation = 1;
            break;
        case 5:
            result->flags |= WIO_NATIVE_ABI_VALUE_MUTABLE;
            break;
        default:
            result->flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
            break;
        }
        return status;
    }

    void qualify(const wio::bytecode::Module& module, const char* path)
    {
        Library library{path};
        check(library.handle != nullptr, "borrow producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto dictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        const auto defaults = library.symbol<WioGetNativePodDefaultApiFn>("WioGetNativePodDefaultApi")();
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), "borrow host entry missing");
            return found->id;
        };
        const auto install = [&](wio::vm::Machine& machine, const WioNativeAbiRegistry& input)
        {
            std::string error;
            if (!machine.registerNativeRegistry(input, *objects, &error, any, dictionaries, sequences, pods, defaults))
                throw std::runtime_error(error);
        };

        wio::vm::Machine good{module};
        install(good, *registry);
        const auto entry = good.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "borrow lifecycle differential failed");
        const auto foreign = good.invoke(function("GlobalProbe"));
        check(!foreign.succeeded() && foreign.error().code == "WVM1222", "unrelated producer/global storage escaped");
        const auto projected = good.invoke(function("ProjectProbe"));
        check(!projected.succeeded() && projected.error().code == "WVM1222",
              "projected result escaped its caller contract");

        auto expiredModule = module;
        auto& probe = expiredModule.functions[function("BorrowProbe")];
        std::uint32_t root = wio::bytecode::InvalidIndex;
        bool patched = false;
        for (auto& block : probe.blocks)
            for (std::size_t index = 0; index < block.instructions.size(); ++index)
            {
                const auto& instruction = block.instructions[index];
                if (instruction.opcode == wio::bytecode::Opcode::PlaceInit && root == wio::bytecode::InvalidIndex)
                    root = instruction.operands.front();
                else if (instruction.opcode == wio::bytecode::Opcode::NativeInvoke)
                {
                    check(root != wio::bytecode::InvalidIndex, "borrow fixture root missing");
                    wio::bytecode::Instruction drop;
                    drop.opcode = wio::bytecode::Opcode::DropPlace;
                    drop.constant = instruction.constant;
                    drop.operands = {root};
                    block.instructions.insert(block.instructions.begin() + index + 1, std::move(drop));
                    patched = true;
                    break;
                }
            }
        check(patched, "borrow fixture native call missing");
        wio::vm::Machine expired{expiredModule};
        install(expired, *registry);
        const auto expiredBorrow = expired.invoke(function("BorrowProbe"));
        check(!expiredBorrow.succeeded() && expiredBorrow.error().code == "WVM1043",
              "borrowed native result remained readable after caller root cleanup");

        for (corruption = 0; corruption < 7; ++corruption)
        {
            const auto name = corruption == 5 ? "Observe" : "Borrow";
            const auto& binding = module.functions[function(name)].nativeBinding;
            std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                    registry->functions + registry->functionCount);
            const auto selected =
                std::ranges::find_if(descriptors, [&](const auto& descriptor)
                                     { return descriptor.stableKey == module.string(binding.stableKey); });
            check(selected != descriptors.end(), "borrow thunk descriptor missing");
            original = selected->thunk;
            selected->thunk = &corrupt;
            auto forged = *registry;
            forged.functions = descriptors.data();
            wio::vm::Machine rejected{module};
            install(rejected, forged);
            const auto result = rejected.invoke(function(corruption == 5 ? "ObserveProbe" : "BorrowProbe"));
            check(!result.succeeded() && result.error().code == "WVM1224",
                  "forged borrowed result contract was accepted");
            check(rejected.invoke(function("SafePoint")).succeeded(), "borrow failure left the machine unusable");
        }
        const auto balanced = good.invoke(function("Balanced"));
        check(balanced.succeeded() && balanced.value().asSignedInteger() == 0,
              "borrow return acquired an object ownership claim");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "borrow gate requires bytecode and producer paths");
        std::ifstream input(std::filesystem::u8path(argv[1]), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "borrow bytecode decode failed");
        qualify(decoded.module, argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
