#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include "place_storage_internal.h"
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
    void check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
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

    void mutationUnit()
    {
        using namespace wio::vm;
        PlaceProjectionDomain domain;
        PlaceStorage root;
        root.value = Value::dictionary(
            0, {Value::signedInteger(20), Value::signedInteger(7), Value::signedInteger(40), Value::signedInteger(9)},
            true);
        root.initialized = true;
        PlaceStorage key, value, repeated, ownerBacked;
        key.alias = const_cast<Value*>(root.value.dictionaryKey(0));
        value.alias = const_cast<Value*>(root.value.dictionaryValue(std::size_t{0}));
        repeated.alias = value.alias;
        ownerBacked.alias = value.alias;
        ownerBacked.owner = root.value;
        for (auto* place : {&key, &value, &repeated})
        {
            place->initialized = true;
            place->bindProjection(&root, &domain);
        }
        ownerBacked.initialized = true;
        ownerBacked.bindProjection(nullptr, &domain);
        {
            DictionaryPlaceMutation mutation{domain, root.value};
            root.value.dictionarySet(Value::signedInteger(10), Value::signedInteger(3));
        }
        check(key.storedValue().asSignedInteger() == 20 && value.storedValue().asSignedInteger() == 7,
              "ordered key/value remap failed");
        check(value.alias == repeated.alias && value.alias == ownerBacked.alias, "repeated alias identity lost");
        value.storedValue() = Value::signedInteger(11);
        {
            DictionaryPlaceMutation mutation{domain, root.value};
            root.value.dictionaryRemove(Value::signedInteger(10));
        }
        check(value.storedValue().asSignedInteger() == 11 && key.storedValue().asSignedInteger() == 20,
              "surviving entry shifted to the wrong key");
        {
            DictionaryPlaceMutation mutation{domain, root.value};
            root.value.dictionaryRemove(Value::signedInteger(20));
        }
        check(!key.initialized && !value.initialized && !repeated.initialized && !ownerBacked.initialized,
              "removed direct entry alias survived");
        check(root.initialized && root.value.dictionaryCount() == 1, "dictionary root expired with its entries");
    }

    void host(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "dictionary intrinsic bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& candidate)
                                                    { return module.string(candidate.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        Library library{producer};
        check(library.handle != nullptr, "dictionary intrinsic producer load failed");
        const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
        const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
        const auto sequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi")();
        const auto dictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi")();
        const auto pods = library.symbol<WioGetNativePodLayoutApiFn>("WioGetNativePodLayoutApi")();
        const auto defaults = library.symbol<WioGetNativePodDefaultApiFn>("WioGetNativePodDefaultApi")();
        wio::vm::Machine machine{module};
        std::string error;
        check(machine.registerNativeRegistry(*registry, *objects, &error, nullptr, dictionaries, sequences, pods,
                                             defaults),
              error.c_str());
        const auto entry = machine.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "dictionary intrinsic host entry failed");
        for (const auto* name : {"RemovedProbe", "ClearedProbe", "ReplacedChildProbe"})
        {
            const auto failed = machine.invoke(function(name));
            check(!failed.succeeded() && failed.error().code == "WVM1043", name);
            check(machine.invoke(function("SafePoint")).succeeded(), "dictionary expiry recovery failed");
        }
        for (std::int64_t action = 0; action < 3; ++action)
        {
            const std::array arguments{wio::vm::Value::signedInteger(action)};
            const auto failed = machine.invoke(function("CleanupProbe"), arguments);
            check(!failed.succeeded() && failed.error().code == "WVM1211" &&
                      failed.error().message.find("Dictionary mutation cleanup") != std::string::npos &&
                      !failed.error().cleanupFailures.empty(),
                  "dictionary destructor failure was lost or delayed");
            check(machine.invoke(function("SafePoint")).succeeded(), "dictionary cleanup recovery failed");
            const auto balanced = machine.invoke(function("LifecycleBalanced"));
            check(balanced.succeeded() && balanced.value().asBoolean(), "dictionary cleanup leaked an object claim");
        }
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "dictionary intrinsic host requires bytecode and producer paths");
        mutationUnit();
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
