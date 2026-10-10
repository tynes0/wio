#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wir_abi.h>
#include <wio_native_any.h>
#include <wio_native_dictionary.h>
#include <wio_native_object.h>
#include <wio_native_pod.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    namespace abi = wio::wir_backend::abi;
    void check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    struct Wire final
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };

    void contracts()
    {
        using Sequence = std::vector<std::int32_t>;
        Wire root;
        root.value = abi::Codec<Sequence, 11>::write(Sequence{7, 19});
        WioNativeAbiValue reference;
        reference.kind = WIO_NATIVE_ABI_REFERENCE;
        reference.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
        reference.payload.mutableSlice = {&root.value, sizeof(WioNativeAbiValue)};
        abi::Frame frame;
        auto& sequence = frame.borrow<Sequence, 11>(reference, true);
        const std::uint64_t index = 1;
        WioNativeAbiSequenceProjection projection{&root.value, 11, 12, &index, 1};
        auto projected = reference;
        projected.typeId = 12;
        projected.flags |= WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED;
        projected.payload.mutableSlice = {&projection, sizeof(projection)};
        auto& item = frame.borrow<std::int32_t, 12>(projected, true);
        check(&item == &sequence[1], "producer projection split element address identity");
        item = 43;
        frame.commit();
        check(abi::Codec<Sequence, 11>::read(root.value)[1] == 43, "projection did not dirty its root slot");

        const auto reject = [&](WioNativeAbiValue input)
        {
            bool rejected = false;
            try
            {
                (void)frame.borrow<std::int32_t, 12>(input, true);
            }
            catch (const abi::Error&)
            {
                rejected = true;
            }
            check(rejected, "malformed producer projection was accepted");
        };
        auto bad = projected;
        bad.flags |= WIO_NATIVE_ABI_VALUE_PROJECTED;
        reject(bad);
        bad = projected;
        bad.flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
        reject(bad);
        bad = projected;
        bad.typeId = 99;
        reject(bad);
        auto saved = projection;
        projection.count = 65;
        reject(projected);
        projection = saved;
        projection.root = reinterpret_cast<WioNativeAbiValue*>(std::uintptr_t{1});
        reject(projected);
        projection = saved;
        projection.indices = nullptr;
        reject(projected);
        projection = saved;
        std::uint64_t outside = 999;
        projection.indices = &outside;
        reject(projected);
        projection = saved;
        projection.rootTypeId = 99;
        reject(projected);
        projection = saved;
        projection.typeId = 99;
        reject(projected);
        std::vector<bool> bits{false, true};
        check(abi::SequenceProjectionAccess<std::vector<bool>>::resolve(&bits, &index, 1, typeid(bool)) == nullptr,
              "bool proxy was addressable");
        check(abi::SequenceProjectionAccess<Sequence>::resolve(&sequence, &index, 1, typeid(double)) == nullptr,
              "terminal C++ type mismatch escaped index validation");
        std::vector<Sequence> nested{{3, 7}};
        const std::array<std::uint64_t, 2> path{0, 1};
        check(abi::SequenceProjectionAccess<decltype(nested)>::resolve(&nested, path.data(), 2, typeid(std::int32_t)) ==
                      &nested[0][1] &&
                  abi::SequenceProjectionAccess<decltype(nested)>::resolve(&nested, path.data(), 1, typeid(Sequence)) ==
                      &nested[0],
              "nested projection split row/element addresses");
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
            const auto address = GetProcAddress(handle, name);
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
#endif
            check(address != nullptr, name);
            return reinterpret_cast<T>(address);
        }
        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;
    };

    WioNativeAbiThunk original;
    unsigned corruption;
    WioNativeAbiStatus corrupt(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure* failure) noexcept
    {
        if (count != 2 || !(args[1].flags & WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED))
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        std::array<WioNativeAbiValue, 2> arguments{args[0], args[1]};
        auto projection = *static_cast<const WioNativeAbiSequenceProjection*>(args[1].payload.mutableSlice.data);
        std::uint64_t index = 9999;
        switch (corruption)
        {
        case 0:
            projection.count = 65;
            break;
        case 1:
            projection.indices = &index;
            break;
        case 2:
            projection.typeId ^= 1;
            break;
        case 3:
            projection.rootTypeId ^= 1;
            break;
        case 4:
            projection.root = reinterpret_cast<WioNativeAbiValue*>(std::uintptr_t{1});
            break;
        case 5:
            projection.indices = nullptr;
            break;
        case 6:
            arguments[1].flags |= WIO_NATIVE_ABI_VALUE_PROJECTED;
            break;
        case 7:
            arguments[1].typeId ^= 1;
            break;
        case 8:
            projection.count = 0;
            break;
        default:
            arguments[1].payload.mutableSlice.size -= 1;
            break;
        }
        arguments[1].payload.mutableSlice.data = &projection;
        return original(arguments.data(), count, result, failure);
    }

    void host(const char* bytecode, const char* path)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "projection bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{path};
        check(library.handle != nullptr, "projection producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto any = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        const auto defaults = library.symbol<WioGetNativePodDefaultApiFn>("WioGetNativePodDefaultApi")();
        const auto install = [&](wio::vm::Machine& machine, const WioNativeAbiRegistry& selected)
        {
            std::string error;
            check(machine.registerNativeRegistry(selected, *objects, &error, any, nullptr, sequences, pods, defaults),
                  error.c_str());
        };
        wio::vm::Machine machine{module};
        install(machine, *registry);
        const auto entry = machine.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "live projection host entry failed");
        const auto borrowed = machine.invoke(function("BorrowProbe"));
        check(borrowed.succeeded() && borrowed.value().asSignedInteger() == 7,
              "qualified sequence borrowed result failed");
        for (const auto& [name, code] : std::array<std::pair<const char*, const char*>, 2>{
                 {{"FailureProbe", "WVM1222"}, {"ExpiredProbe", "WVM1043"}}})
        {
            const auto failed = machine.invoke(function(name));
            check(!failed.succeeded() && failed.error().code == code, name);
            check(machine.invoke(function("SafePoint")).succeeded(), "projection VM failed to recover");
        }
        check(machine.invoke(function("PodFieldProbe")).succeeded(), "qualified POD field projection failed");
        const auto cleanup = machine.invoke(function("CleanupProbe"));
        check(!cleanup.succeeded() && cleanup.error().code == "WVM1211" &&
                  cleanup.error().message.find("Native copy-back cleanup") != std::string::npos &&
                  !cleanup.error().cleanupFailures.empty(),
              "projected object token retained an old handle or swallowed copy-back destruction failure");
        const auto balanced = machine.invoke(function("LifecycleBalanced"));
        check(balanced.succeeded() && balanced.value().asBoolean(), "projection cleanup leaked an object claim");
        const auto& binding = module.functions[function("Mutate")].nativeBinding;
        std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                registry->functions + registry->functionCount);
        const auto selected = std::ranges::find_if(descriptors, [&](const auto& value)
                                                   { return value.stableKey == module.string(binding.stableKey); });
        check(selected != descriptors.end(), "projection thunk missing");
        original = selected->thunk;
        auto changed = *registry;
        changed.functions = descriptors.data();
        selected->flags &= ~WIO_NATIVE_ABI_FUNCTION_SEQUENCE_PROJECTIONS;
        {
            wio::vm::Machine old{module};
            install(old, changed);
            const auto failed = old.invoke(function("Probe"));
            check(!failed.succeeded() && failed.error().code == "WVM1225", "old producer capability was ignored");
        }
        selected->flags |= WIO_NATIVE_ABI_FUNCTION_SEQUENCE_PROJECTIONS;
        selected->thunk = &corrupt;
        for (corruption = 0; corruption < 10; ++corruption)
        {
            wio::vm::Machine invalid{module};
            install(invalid, changed);
            const auto before = invalid.invoke(function("ReadCalls"));
            const auto failed = invalid.invoke(function("Probe"));
            if (failed.succeeded() || failed.error().code != "WVM1222")
                throw std::runtime_error(failed.succeeded() ? "forged projection was accepted"
                                                            : failed.error().code + ": " + failed.error().message);
            const auto after = invalid.invoke(function("ReadCalls"));
            check(before.succeeded() && after.succeeded() && before.value() == after.value(),
                  "invalid producer projection reached the native function");
            check(invalid.invoke(function("SafePoint")).succeeded(), "forged projection VM failed to recover");
        }
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "projection qualification requires bytecode and producer paths");
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
