#include "native_library.h"
#include "wio/bytecode/codec.h"
#include "wio/codegen/wir_native_container_emitter.h"
#include "wio/codegen/wir_native_pod_emitter.h"
#include "wio/vm/machine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
    void check(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void selection()
    {
        using namespace wio::wir;
        lowered::Module module;
        const auto integer = module.types.intern(Type{.kind = TypeKind::I32});
        const auto size = module.types.intern(Type{.kind = TypeKind::USize});
        const auto string = module.types.intern(Type{.kind = TypeKind::String});
        const auto text = module.types.intern(Type{.kind = TypeKind::Text});
        const auto any = module.types.intern(Type{.kind = TypeKind::Any});
        const auto extent = module.types.intern(Type{.kind = TypeKind::ConstValue, .name = "3", .arguments = {size}});
        const auto open = module.types.intern(Type{.kind = TypeKind::GenericParameter, .name = "T"});
        const auto unresolved =
            module.types.intern(Type{.kind = TypeKind::Array, .arguments = {integer}, .extentParameter = open});
        const auto pack = module.types.intern(Type{.kind = TypeKind::PackStorage, .arguments = {integer}});
        const auto pod = [&](const char* name, std::vector<TypeId> arguments, TypeId field)
        {
            const auto id = module.types.internNominal(Type{.kind = TypeKind::Named,
                                                            .name = name,
                                                            .arguments = std::move(arguments),
                                                            .nominalKind = NominalKind::Component,
                                                            .nominalRepresentation = NominalRepresentation::NativePod,
                                                            .fields = {{.name = "value", .type = field}},
                                                            .nativeBinding = NativeTypeBinding{.cppName = name}});
            module.contract.reflection.push_back({.stableTypeId = 1000 + id.value(), .type = id});
            return id;
        };
        const auto constant = pod("Constant", {integer, extent}, integer);
        const auto unused = pod("Unused", {string, text}, integer);
        const auto emptyPack = pod("EmptyPack", {}, integer);
        const auto nested = pod("Nested", {integer, extent}, constant);
        const auto openPod = pod("Open", {open}, integer);
        const auto badField = pod("StringField", {integer}, string);
        const auto packField = pod("PackField", {integer}, pack);
        const auto openExtent = pod("OpenExtent", {integer}, unresolved);
        const auto array = module.types.intern(Type{.kind = TypeKind::Array, .arguments = {constant}});
        const auto dictionary = module.types.intern(Type{.kind = TypeKind::Dictionary, .arguments = {string, nested}});
        const auto badArray = module.types.intern(Type{.kind = TypeKind::Array, .arguments = {openPod}});
        const auto badDictionary =
            module.types.intern(Type{.kind = TypeKind::Dictionary, .arguments = {string, badField}});
        lowered::Function boundary;
        boundary.nativeBinding = NativeBinding{.parameters = {{.type = any},
                                                              {.type = array},
                                                              {.type = dictionary},
                                                              {.type = badArray},
                                                              {.type = badDictionary}},
                                               .result = {.type = any}};
        lowered::BasicBlock block;
        for (const auto target : {constant, unused, emptyPack, nested, openPod, badField, packField, openExtent})
            block.instructions.push_back({.opcode = lowered::Opcode::AnyBox, .targetType = target});
        boundary.blocks.push_back(std::move(block));
        module.functions.push_back(std::move(boundary));
        const auto eligible = wio::codegen::WirNativePodEmitter::eligibleTypes(module);
        const auto emitted = wio::codegen::WirNativeContainerEmitter::emit(
            module, [](TypeId id) { return "test_type_" + std::to_string(id.value()); });
        for (const auto id : {constant, unused, emptyPack, nested})
        {
            check(std::ranges::find(eligible, id) != eligible.end(), "concrete POD eligibility diverged");
            check(emitted.find("using _native_any_payload_" + std::to_string(id.value()) + " = ") != std::string::npos,
                  "qualified generic POD Any adapter missing");
        }
        for (const auto id : {openPod, badField, packField, openExtent})
        {
            check(std::ranges::find(eligible, id) == eligible.end(), "open/non-POD type gained a layout adapter");
            check(emitted.find("using _native_any_payload_" + std::to_string(id.value()) + " = ") == std::string::npos,
                  "open/non-POD type gained an Any adapter");
        }
        check(emitted.find("using _native_sequence_" + std::to_string(array.value()) + " = ") != std::string::npos,
              "generic POD sequence adapter missing");
        check(emitted.find("using _native_dictionary_" + std::to_string(dictionary.value()) + " = ") !=
                  std::string::npos,
              "generic POD dictionary adapter missing");
        check(emitted.find("using _native_sequence_" + std::to_string(badArray.value()) + " = ") == std::string::npos,
              "open generic POD array received an adapter");
        check(emitted.find("using _native_dictionary_" + std::to_string(badDictionary.value()) + " = ") ==
                  std::string::npos,
              "non-POD dictionary received an adapter");
    }

    void lifetime(const char* bytecode, const char* library)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "generic Any bytecode decode failed");
        const auto entry = [&](const char* name)
        {
            const auto found = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                    { return decoded.module.string(function.name) == name; });
            check(found != decoded.module.functions.end(), "generic Any entry missing");
            return found->id;
        };
        const auto escape = entry("Escape");
        wio::vm::Value escaped;
        std::weak_ptr<wio::vm::detail::NativeLibrary> lease;
        {
            std::string error;
            auto producer = wio::vm::detail::NativeLibrary::open(std::filesystem::u8path(library), error);
            check(bool(producer), error.c_str());
            lease = producer;
            wio::vm::Machine machine{decoded.module};
            check(machine.loadNativeLibrary(std::filesystem::u8path(library), &error), error.c_str());
            producer.reset();
            const auto result = machine.invoke(escape);
            if (!result.succeeded())
                throw std::runtime_error(result.error().code + ": " + result.error().message);
            check(result.value().kind() == wio::vm::Value::Kind::Any && result.value().anyPayload() &&
                      result.value().anyPayload()->kind() == wio::vm::Value::Kind::Component,
                  "generic native POD Any remained an opaque foreign value");
            escaped = result.value();
        }
        check(!lease.expired(), "escaped generic Any failed to pin producer code");
        auto child = *escaped.anyPayload();
        escaped = {};
        check(!lease.expired(), "generic POD payload lost its producer lease");
        const auto* field = child.field(1);
        check(field && field->kind() == wio::vm::Value::Kind::Opaque && field->opaquePointer(),
              "generic POD opaque field lost its producer identity");
        auto pointer = *field;
        auto backing = child.nativePodBacking();
        child = {};
        check(!lease.expired(), "escaped generic POD children lost producer code");
        pointer = {};
        check(!lease.expired(), "generic POD immutable backing failed to pin hidden state");
        backing = {};
        check(lease.expired(), "generic POD Any producer lease leaked after final release");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "generic Any qualification requires bytecode and producer paths");
        selection();
        lifetime(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
