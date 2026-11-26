#include "native_pod_internal.h"
#include "wio/codegen/wir_native_pod_emitter.h"

#include <iostream>

namespace
{
    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    WioNativeAbiStatus unusedPack(const WioNativeAbiValue*, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                  WioNativeAbiFailure*) noexcept
    {
        return WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    WioNativeAbiStatus unusedRead(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                  WioNativeAbiFailure*) noexcept
    {
        return WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    bool emitterContract()
    {
        using namespace wio::wir;
        lowered::Module module;
        const auto integer = module.types.intern(Type{.kind = TypeKind::I32});
        const auto size = module.types.intern(Type{.kind = TypeKind::USize});
        const auto string = module.types.intern(Type{.kind = TypeKind::String});
        const auto text = module.types.intern(Type{.kind = TypeKind::Text});
        const auto extent = module.types.intern(Type{.kind = TypeKind::ConstValue, .name = "3", .arguments = {size}});
        const auto open = module.types.intern(Type{.kind = TypeKind::GenericParameter, .name = "T"});
        const auto pack = module.types.intern(Type{.kind = TypeKind::PackStorage, .arguments = {integer}});
        const auto add = [&](const char* name, std::vector<TypeId> arguments, TypeId field)
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
        const auto constant = add("ConstPod", {integer, extent}, integer);
        const auto unusedTypes = add("UnusedTypes", {string, text}, integer);
        const auto empty = add("EmptyPack<>", {}, integer);
        const auto invalidField = add("ConstantField", {extent}, extent);
        const auto openArgument = add("OpenArgument", {open}, integer);
        const auto packField = add("PackField", {integer}, pack);
        const auto output = wio::codegen::WirNativePodEmitter::emit(
            module, [&](TypeId id) { return "test_type_" + std::to_string(id.value()); });
        const auto emitted = [&](TypeId id)
        { return output.find("using _native_pod_" + std::to_string(id.value()) + " = ") != std::string::npos; };
        bool passed = true;
        passed &= expect(emitted(constant), "const argument was mistaken for physical field storage");
        passed &= expect(emitted(unusedTypes), "unused template types were mistaken for physical fields");
        passed &= expect(emitted(empty), "empty variadic specialization omitted its field adapter");
        passed &= expect(!emitted(invalidField) && !emitted(openArgument) && !emitted(packField),
                         "open template or non-POD field emitted an adapter");
        return passed;
    }

    bool registryContract()
    {
        namespace bridge = wio::vm::detail;
        wio::bytecode::Module module;
        module.strings = {"value", "ConstPod", "3"};
        module.types = {wio::bytecode::Type{.kind = 5}, wio::bytecode::Type{.kind = 12},
                        wio::bytecode::Type{.kind = 23, .name = 2, .arguments = {1}},
                        wio::bytecode::Type{.kind = 28,
                                            .name = 1,
                                            .arguments = {0, 2},
                                            .nominalKind = 1,
                                            .nominalRepresentation = 1,
                                            .fields = {{.name = 0, .type = 0}}}};
        module.types[3].hasNativeBinding = true;
        module.contract.reflection = {{.stableTypeId = 700, .logicalName = 1, .type = 3, .nominalKind = 1}};
        WioNativePodFieldDescriptor field{"value", 701, 0, sizeof(std::int32_t), 5};
        WioNativePodLayoutDescriptor descriptor{700,         sizeof(std::int32_t), alignof(std::int32_t), 1, &field,
                                                &unusedPack, &unusedRead};
        WioNativePodLayoutApi api{WIO_NATIVE_POD_LAYOUT_VERSION, 1, &descriptor};
        bridge::NativePodLayouts layouts;
        std::string error;
        bool passed = true;
        passed &= expect(bridge::mergeNativePodLayouts(module, api, layouts, error),
                         "concrete const-generic POD descriptor rejected");
        const auto previous = layouts.types;
        for (const std::uint8_t kind : {21, 22, 24, 25, 26})
        {
            module.types[2].kind = kind;
            passed &= expect(!bridge::mergeNativePodLayouts(module, api, layouts, error) && layouts.types == previous,
                             "open generic argument published a layout or changed the existing snapshot");
        }
        module.types[2].kind = 23;
        module.types[2].arguments = {2};
        passed &= expect(!bridge::mergeNativePodLayouts(module, api, layouts, error),
                         "cyclic template argument ignored recursion budget");
        module.types[2].arguments = {wio::bytecode::InvalidIndex};
        passed &= expect(!bridge::mergeNativePodLayouts(module, api, layouts, error),
                         "invalid template argument type id accepted");
        return passed;
    }
} // namespace

int main()
{
    return emitterContract() && registryContract() ? 0 : 1;
}
