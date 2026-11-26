#include "native_library.h"
#include "wio/bytecode/codec.h"
#include "wio/codegen/wir_native_container_emitter.h"
#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/vm/machine.h"
#include <wio_native_any.h>
#include <wio_native_dictionary.h>
#include <wio_native_object.h>
#include <wio_native_sequence.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    void check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    void selection()
    {
        using namespace wio::wir;
        lowered::Module module;
        const auto integer = module.types.i32Type();
        const auto string = module.types.intern(Type{.kind = TypeKind::String});
        const auto any = module.types.intern(Type{.kind = TypeKind::Any});
        const auto open = module.types.intern(Type{.kind = TypeKind::GenericParameter, .name = "T"});
        const auto object = [&](const char* name, std::vector<TypeId> arguments, FieldVisibility visibility)
        {
            const auto id = module.types.internNominal(
                Type{.kind = TypeKind::Named,
                     .name = name,
                     .arguments = std::move(arguments),
                     .nominalKind = NominalKind::Object,
                     .fields = {{.name = "value", .type = integer, .visibility = visibility}}});
            module.contract.reflection.push_back({.stableTypeId = 1000 + id.value(), .type = id});
            return id;
        };
        const auto good = object("Good", {}, FieldVisibility::Private);
        const auto generic = object("Generic", {integer}, FieldVisibility::Private);
        const auto unresolved = object("Open", {open}, FieldVisibility::Private);
        const auto publicField = object("Public", {}, FieldVisibility::Public);
        const auto array = [&](TypeId value)
        { return module.types.intern(Type{.kind = TypeKind::Array, .arguments = {value}}); };
        const auto anyArray = array(any);
        const auto goodArray = array(good);
        const auto genericArray = array(generic);
        const auto publicArray = array(publicField);
        const auto openArray = array(unresolved);
        const auto goodMap = module.types.intern(Type{.kind = TypeKind::Dictionary, .arguments = {string, generic}});
        const auto badMap = module.types.intern(Type{.kind = TypeKind::Dictionary, .arguments = {any, integer}});
        lowered::Function boundary;
        // No direct Any or object parameter: discovery must recurse into Any[].
        boundary.nativeBinding = NativeBinding{
            .parameters = {{.type = anyArray}, {.type = publicArray}, {.type = openArray}, {.type = badMap}}};
        lowered::BasicBlock block;
        for (const auto id : {good, generic, publicField, unresolved, goodArray, genericArray, goodMap})
            block.instructions.push_back({.opcode = lowered::Opcode::AnyBox, .targetType = id});
        boundary.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(boundary));
        check(wio::codegen::WirNativeObjectEmitter::eligibleTypes(module) == std::set{good, generic},
              "Any-container-only object discovery widened or lost eligibility");
        const auto emitted = wio::codegen::WirNativeContainerEmitter::emit(
            module, [](TypeId id) { return "test_type_" + std::to_string(id.value()); });
        for (const auto id : {anyArray, goodArray, genericArray})
            check(emitted.find("using _native_sequence_" + std::to_string(id.value()) + " = ") != std::string::npos,
                  "qualified Any/object/generic container adapter missing");
        check(emitted.find("using _native_dictionary_" + std::to_string(goodMap.value()) + " = ") != std::string::npos,
              "boxed generic object dictionary adapter missing");
        for (const auto id : {publicArray, openArray})
            check(emitted.find("using _native_sequence_" + std::to_string(id.value()) + " = ") == std::string::npos,
                  "unsupported object container received an adapter");
        check(emitted.find("using _native_dictionary_" + std::to_string(badMap.value()) + " = ") == std::string::npos,
              "Any dictionary keys escaped the native key contract");
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
            return reinterpret_cast<T>(GetProcAddress(handle, name));
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
            return reinterpret_cast<T>(dlsym(handle, name));
        }
#endif
    };

    void qualify(const wio::bytecode::Module& module, const char* path)
    {
        Library library{path};
        check(library.handle != nullptr, "container producer load failed");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getAny = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi");
        const auto getSequences = library.symbol<WioGetNativeSequenceApiFn>("WioGetNativeSequenceApi");
        const auto getDictionaries = library.symbol<WioGetNativeDictionaryApiFn>("WioGetNativeDictionaryApi");
        check(getRegistry && getObjects && getAny && getSequences && getDictionaries,
              "container-only boundaries lost required object/Any sidecars");
        const auto& registry = *getRegistry();
        const auto& objects = *getObjects();
        const auto& any = *getAny();
        const auto& sequences = *getSequences();
        const auto& dictionaries = *getDictionaries();
        check(objects.typeCount == 3 && sequences.typeCount >= 5 && dictionaries.typeCount >= 3,
              "object/Any/generic/nullable container inventory is incomplete");
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions, [&](const auto& value)
                                                    { return module.string(value.name) == name; });
            check(found != module.functions.end(), "container host entry missing");
            return found->id;
        };
        std::string error;
        wio::vm::Machine missingAny{module};
        check(!missingAny.registerNativeRegistry(registry, objects, &error, nullptr, &dictionaries, &sequences) &&
                  error.find("unsupported structural materialization") != std::string::npos,
              "Any containers registered without their required Any adapter");
        check(missingAny.registerNativeRegistry(registry, objects, &error, &any, &dictionaries, &sequences),
              error.c_str());
        const auto entry = missingAny.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "live-host container differential entry failed");

        wio::vm::Machine missingObjects{module};
        check(!missingObjects.registerNativeRegistry(registry, any, &error, &dictionaries, &sequences) &&
                  error.find("unsupported structural materialization") != std::string::npos,
              "object payload containers registered without object adapters");
        wio::vm::Machine missingSequences{module};
        check(!missingSequences.registerNativeRegistry(registry, objects, &error, &any, &dictionaries) &&
                  error.find("unsupported structural materialization") != std::string::npos,
              "boxed sequences registered without materialization adapters");

        const auto base = std::ranges::find_if(module.contract.reflection, [&](const auto& value)
                                               { return module.string(module.types[value.type].name) == "Base"; });
        check(base != module.contract.reflection.end(), "Base reflection missing");
        const auto array =
            std::ranges::find_if(module.types, [&](const auto& value)
                                 { return value.kind == 31 && value.arguments == std::vector{base->type}; });
        check(array != module.types.end(), "Base array type missing");
        const auto arrayType = static_cast<std::uint32_t>(array - module.types.begin());
        const auto reflectedArray =
            std::ranges::find(module.contract.reflection, arrayType, &wio::bytecode::Reflection::type);
        check(reflectedArray != module.contract.reflection.end(), "Base array identity missing");
        std::vector<WioNativeSequenceDescriptor> descriptors(sequences.types, sequences.types + sequences.typeCount);
        const auto descriptor =
            std::ranges::find(descriptors, reflectedArray->stableTypeId, &WioNativeSequenceDescriptor::typeId);
        check(descriptor != descriptors.end(), "Base array producer adapter missing");
        auto forgedApi = sequences;
        forgedApi.types = descriptors.data();
        const auto elementType = descriptor->elementTypeId;
        descriptor->elementTypeId ^= 0x100;
        wio::vm::Machine malformed{module};
        check(!malformed.registerNativeRegistry(registry, objects, &error, &any, &dictionaries, &forgedApi) &&
                  error.find("does not match its VM array declaration") != std::string::npos,
              "malformed container metadata escaped atomic registration");
        descriptor->elementTypeId = elementType;
        static WioNativeSequenceReadFn originalRead;
        originalRead = descriptor->read;
        descriptor->read = [](const WioNativeAbiValue* value, std::uint64_t index, WioNativeAbiValue* result,
                              WioNativeAbiFailure* failure) noexcept
        {
            const auto status = originalRead(value, index, result, failure);
            if (status == WIO_NATIVE_ABI_OK && index == 1)
                result->typeId ^= 0x100;
            return status;
        };
        check(malformed.registerNativeRegistry(registry, objects, &error, &any, &dictionaries, &forgedApi),
              error.c_str());
        const auto forged = malformed.invoke(function("EscapeObjects"));
        check(!forged.succeeded() && forged.error().code == "WVM1224", "forged nested object wire was accepted");
        check(malformed.invoke(function("SafePoint")).succeeded(), "partial container decode cleanup failed");

        auto expiredModule = module;
        auto& probe = expiredModule.functions[function("BorrowProbe")];
        bool patched = false;
        std::uint32_t root = wio::bytecode::InvalidIndex;
        for (auto& block : probe.blocks)
            for (std::size_t index = 0; index < block.instructions.size(); ++index)
                if (block.instructions[index].opcode == wio::bytecode::Opcode::PlaceInit &&
                    root == wio::bytecode::InvalidIndex)
                    root = block.instructions[index].operands.front();
                else if (block.instructions[index].opcode == wio::bytecode::Opcode::ArrayPlace)
                {
                    check(root != wio::bytecode::InvalidIndex, "borrow fixture owning root missing");
                    wio::bytecode::Instruction drop;
                    drop.opcode = wio::bytecode::Opcode::DropPlace;
                    // Even non-literal bytecode instructions carry the interned
                    // empty constant; preserve that verified module annotation.
                    drop.constant = block.instructions[index].constant;
                    drop.operands = {root};
                    block.instructions.insert(block.instructions.begin() + index + 1, std::move(drop));
                    patched = true;
                    break;
                }
        check(patched, "borrow expiration fixture has no array projection");
        wio::vm::Machine expired{expiredModule};
        check(expired.registerNativeRegistry(registry, objects, &error, &any, &dictionaries, &sequences),
              error.c_str());
        const auto invalidBorrow = expired.invoke(function("BorrowProbe"));
        if (!invalidBorrow.succeeded() && invalidBorrow.error().code != "WVM1043")
            throw std::runtime_error("expired borrow: " + invalidBorrow.error().code + ": " +
                                     invalidBorrow.error().message);
        check(!invalidBorrow.succeeded() && invalidBorrow.error().code == "WVM1043",
              "expired container projection remained readable after root cleanup");

        {
            const auto escaped = missingAny.invoke(function("Escape"));
            check(escaped.succeeded() && escaped.value().kind() == wio::vm::Value::Kind::Array &&
                      escaped.value().element(0)->kind() == wio::vm::Value::Kind::Any &&
                      escaped.value().element(0)->anyPayload()->kind() == wio::vm::Value::Kind::Object,
                  "escaped Any array lost its original VM object");
        }
        check(missingAny.invoke(function("ReleaseHeld")).succeeded(), "native-held container release failed");
        check(missingAny.invoke(function("SafePoint")).succeeded(), "escaped container cleanup failed");
        const auto balanced = missingAny.invoke(function("Balanced"));
        check(balanced.succeeded() && balanced.value().asSignedInteger() == 0,
              "partial/escaped container decode lost or repeated object cleanup");
    }

    void lifetime(const wio::bytecode::Module& module, const char* path)
    {
        const auto escape = std::ranges::find_if(module.functions, [&](const auto& value)
                                                 { return module.string(value.name) == "EscapeNative"; });
        check(escape != module.functions.end(), "native container escape entry missing");
        wio::vm::Value escaped;
        std::weak_ptr<wio::vm::detail::NativeLibrary> lease;
        {
            std::string error;
            auto producer = wio::vm::detail::NativeLibrary::open(std::filesystem::u8path(path), error);
            check(bool(producer), error.c_str());
            lease = producer;
            wio::vm::Machine machine{module};
            check(machine.loadNativeLibrary(std::filesystem::u8path(path), &error), error.c_str());
            producer.reset();
            const auto result = machine.invoke(escape->id);
            check(result.succeeded() && result.value().elementCount() == 3, "native Any array escape failed");
            escaped = result.value();
        }
        check(!lease.expired(), "escaped Any array failed to pin producer code");
        auto child = *escaped.element(0);
        escaped = {};
        check(!lease.expired(), "escaped Any child lost producer code");
        auto backing = child.nativeAnyBacking();
        child = {};
        check(!lease.expired(), "Any child backing failed to pin producer code");
        backing = {};
        check(lease.expired(), "Any container producer lease leaked after final release");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "container qualification requires bytecode and producer paths");
        std::ifstream input(std::filesystem::u8path(argv[1]), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "container bytecode decode failed");
        selection();
        qualify(decoded.module, argv[2]);
        lifetime(decoded.module, argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
