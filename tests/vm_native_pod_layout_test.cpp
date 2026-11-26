#include "native_pod_internal.h"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
    namespace bridge = wio::vm::detail;
    using wio::vm::Value;
    constexpr std::uint64_t PodId = 777;
    struct Pod
    {
        std::int8_t tag;
        double value;
    };
    struct Owner
    {
        Pod value;
    };
    int releases = 0;
    const WioNativeAbiHandleOps Operations{WIO_NATIVE_ABI_VERSION, 0, [](void*) noexcept {},
                                           [](void* storage) noexcept
                                           {
                                               ++releases;
                                               delete static_cast<Owner*>(storage);
                                           },
                                           [](const void*) noexcept { return PodId; }};

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    WioNativeAbiStatus pack(const WioNativeAbiValue*, const WioNativeAbiValue* fields, std::uint64_t count,
                            WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
    {
        if (!fields || count != 2 || !result || fields[0].kind != WIO_NATIVE_ABI_I64 ||
            fields[1].kind != WIO_NATIVE_ABI_F64)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* owner =
            new Owner{{static_cast<std::int8_t>(fields[0].payload.signedInteger), fields[1].payload.floatingPoint}};
        *result = {};
        result->kind = WIO_NATIVE_ABI_POD;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->typeId = PodId;
        result->payload.slice = {&owner->value, sizeof(Pod)};
        result->owner = {owner, &Operations, 1};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus read(const WioNativeAbiValue* wire, std::uint64_t index, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (!wire || !result || wire->typeId != PodId || wire->kind != WIO_NATIVE_ABI_POD ||
            wire->payload.slice.size != sizeof(Pod) || !wire->payload.slice.data || index >= 2)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        Pod value{};
        std::memcpy(&value, wire->payload.slice.data, sizeof(value));
        *result = {};
        if (index == 0)
        {
            result->kind = WIO_NATIVE_ABI_I64;
            result->payload.signedInteger = value.tag;
        }
        else
        {
            result->kind = WIO_NATIVE_ABI_F64;
            result->payload.floatingPoint = value.value;
        }
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus badPack(const WioNativeAbiValue* prototype, const WioNativeAbiValue* fields, std::uint64_t count,
                               WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept
    {
        const auto status = pack(prototype, fields, count, result, failure);
        if (status == WIO_NATIVE_ABI_OK)
            result->payload.slice.size -= 1;
        return status;
    }

    WioNativeAbiStatus failingPack(const WioNativeAbiValue* prototype, const WioNativeAbiValue* fields,
                                   std::uint64_t count, WioNativeAbiValue* result,
                                   WioNativeAbiFailure* failure) noexcept
    {
        (void)pack(prototype, fields, count, result, failure);
        return WIO_NATIVE_ABI_EXCEPTION;
    }
} // namespace

int main()
{
    wio::bytecode::Module module;
    for (std::uint8_t kind = 0; kind <= 18; ++kind)
        module.types.push_back(wio::bytecode::Type{.kind = kind});
    module.strings = {"tag", "value", "Pod"};
    wio::bytecode::Type type;
    type.kind = 28;
    type.name = 2;
    type.nominalKind = 1;
    type.nominalRepresentation = 1;
    type.hasNativeBinding = true;
    type.fields = {{.name = 0, .type = 3}, {.name = 1, .type = 14}};
    module.types.push_back(type);
    module.contract.reflection.push_back({.stableTypeId = PodId, .logicalName = 2, .type = 19, .nominalKind = 1});
    std::array fields{WioNativePodFieldDescriptor{"tag", 101, offsetof(Pod, tag), sizeof(Pod::tag), 3},
                      WioNativePodFieldDescriptor{"value", 102, offsetof(Pod, value), sizeof(Pod::value), 14}};
    WioNativePodLayoutDescriptor descriptor{PodId, sizeof(Pod), alignof(Pod), 2, fields.data(), &pack, &read};
    const WioNativePodLayoutApi api{WIO_NATIVE_POD_LAYOUT_VERSION, 1, &descriptor};
    bridge::NativePodLayouts layouts;
    std::string error;
    bool passed = expect(bridge::mergeNativePodLayouts(module, api, layouts, error), "valid padded layout rejected");
    passed &= expect(bridge::mergeNativePodLayouts(module, api, layouts, error), "idempotent layout merge rejected");
    const auto rejection = [&](const char* message)
    {
        const auto old = layouts.types;
        const bool rejected = !bridge::mergeNativePodLayouts(module, api, layouts, error);
        return expect(rejected && !error.empty() && layouts.types == old, message);
    };
    fields[0].name = "wrong";
    passed &= rejection("wrong field name accepted or changed published layout");
    fields[0].name = "tag";
    fields[0].typeKind = 8;
    passed &= rejection("wrong scalar type accepted");
    fields[0].typeKind = 3;
    fields[0].offset = 1;
    passed &= rejection("changed field offset silently replaced a published layout");
    fields[0].offset = offsetof(Pod, tag);
    fields[1].offset = sizeof(Pod);
    passed &= rejection("out-of-bounds field accepted");
    fields[1].offset = 0;
    passed &= rejection("overlapping fields accepted");
    fields[1].offset = offsetof(Pod, value);
    fields[1].size = (std::numeric_limits<std::uint64_t>::max)();
    passed &= rejection("overflowing field accepted");
    fields[1].size = sizeof(Pod::value);
    descriptor.valueAlignment = 3;
    passed &= rejection("non-power-of-two alignment accepted");
    descriptor.valueAlignment = alignof(Pod);
    descriptor.fieldCount = 1;
    passed &= rejection("missing field accepted");
    descriptor.fieldCount = 2;
    descriptor.read = nullptr;
    passed &= rejection("missing adapter accepted");
    descriptor.read = &read;
    descriptor.pack = &badPack;
    passed &= rejection("conflicting adapter silently replaced existing layout");
    descriptor.pack = &pack;
    auto incompatible = api;
    incompatible.version += 1;
    passed &=
        expect(!bridge::mergeNativePodLayouts(module, incompatible, layouts, error), "wrong sidecar version accepted");

    const auto local = Value::component(19, {Value::signedInteger(-128), Value::floating(1.25)});
    WioNativeAbiValue wire;
    passed &=
        expect(!bridge::encodeNativeValue(module, local, 19, 4, wire, error), "local POD accepted without adapter");
    passed &=
        expect(bridge::encodeNativeValue(module, local, 19, 4, wire, error, &layouts), "local POD packing failed");
    passed &= expect(wire.payload.slice.size == sizeof(Pod) && (wire.flags & WIO_NATIVE_ABI_VALUE_OWNED),
                     "pack did not transfer owned storage");
    const auto decoded = bridge::decodeNativeValue(module, wire, 19, 4, &layouts);
    passed &=
        expect(decoded.succeeded() && decoded.value.kind() == Value::Kind::Component &&
                   decoded.value.field(0)->asSignedInteger() == -128 && decoded.value.field(1)->asFloat64() == 1.25,
               "POD field decoding failed");
    passed &= expect(bridge::decodeNativeValue(module, wire, 19, 4).value.kind() == Value::Kind::NativePod,
                     "legacy raw POD roundtrip changed without a sidecar");
    wire.payload.slice.size -= 1;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 19, 4, &layouts).succeeded(), "truncated POD accepted");
    wire.payload.slice.size += 1;
    wire.typeId += 1;
    passed &=
        expect(!bridge::decodeNativeValue(module, wire, 19, 4, &layouts).succeeded(), "wrong POD identity accepted");
    wire.typeId -= 1;
    WioNativeAbiReleaseValue(&wire);
    passed &= expect(releases == 1 && decoded.value.field(1)->asFloat64() == 1.25,
                     "decoded POD borrowed released producer bytes");
    const auto invalid = Value::component(19, {Value::signedInteger(-129), Value::floating(1.25)});
    passed &=
        expect(!bridge::encodeNativeValue(module, invalid, 19, 4, wire, error, &layouts), "out-of-range field packed");
    for (const auto adapter : {&badPack, &failingPack})
    {
        bridge::NativePodLayouts faulty;
        descriptor.pack = adapter;
        passed &= expect(bridge::mergeNativePodLayouts(module, api, faulty, error), "test adapter registration failed");
        const auto before = releases;
        passed &= expect(!bridge::encodeNativeValue(module, local, 19, 4, wire, error, &faulty) &&
                             wire.kind == WIO_NATIVE_ABI_VOID && releases == before + 1,
                         "failed pack leaked partially produced owned POD");
    }
    passed &= expect(!bridge::decodeNativeValue(module, {}, 19, 4, &layouts, 65).succeeded(), "nesting limit ignored");
    return passed ? 0 : 1;
}
