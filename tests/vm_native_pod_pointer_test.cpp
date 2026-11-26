#include "native_pod_array_codec.h"

#include <array>
#include <iostream>

namespace
{
    namespace bridge = wio::vm::detail;
    using wio::vm::Value;

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    int releases = 0;
    int readMode = 0;
    struct Owner
    {
        unsigned references = 1;
        int token = 17;
    };

    const WioNativeAbiHandleOps Operations{WIO_NATIVE_ABI_VERSION, 0,
                                           [](void* storage) noexcept { ++static_cast<Owner*>(storage)->references; },
                                           [](void* storage) noexcept
                                           {
                                               auto* owner = static_cast<Owner*>(storage);
                                               if (--owner->references == 0)
                                               {
                                                   ++releases;
                                                   delete owner;
                                               }
                                           },
                                           [](const void*) noexcept { return std::uint64_t{501}; }};

    WioNativeAbiStatus unusedPack(const WioNativeAbiValue*, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                  WioNativeAbiFailure*) noexcept
    {
        return WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    WioNativeAbiStatus readPointer(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                   WioNativeAbiFailure*) noexcept
    {
        auto* owner = new Owner;
        *result = {};
        result->kind = readMode == 2 ? WIO_NATIVE_ABI_U64 : WIO_NATIVE_ABI_OPAQUE;
        result->typeId = readMode == 1 ? 999 : 502;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->payload.opaque = readMode == 4 ? nullptr : &owner->token;
        result->owner = {owner, &Operations, 1};
        if (readMode == 5 || readMode == 6)
        {
            result->kind = WIO_NATIVE_ABI_VOID;
            if (readMode == 6)
                result->flags |= WIO_NATIVE_ABI_VALUE_NULLABLE;
        }
        return readMode == 3 ? WIO_NATIVE_ABI_EXCEPTION : WIO_NATIVE_ABI_OK;
    }
} // namespace

int main()
{
    wio::bytecode::Module module;
    for (std::uint8_t kind = 0; kind <= 20; ++kind)
        module.types.push_back(wio::bytecode::Type{.kind = kind});
    const auto nullable = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 30, .arguments = {20}});
    const auto nullableScalar = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 30, .arguments = {5}});
    const auto nestedNullable = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 30, .arguments = {nullable}});
    const auto pointerArray = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 31, .arguments = {20}, .staticExtent = 2});
    const auto nullableArray = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 31, .arguments = {nullable}, .staticExtent = 2});
    module.strings = {"pointer", "PointerPod", "opaque", "opaque?"};
    module.contract.reflection.push_back({.stableTypeId = 501, .logicalName = 2, .type = 20});
    module.contract.reflection.push_back({.stableTypeId = 502, .logicalName = 3, .type = nullable});

    bridge::NativePodLayouts layouts;
    std::string error;
    std::size_t size = 0;
    bool passed = true;
    int token = 29;
    const auto pointer = Value::opaque(20, 501, &token);
    std::array<std::byte, sizeof(void*) * 2> storage{};
    passed &= expect(bridge::NativePodArrayCodec::byteSize(module, nullableArray, layouts, size, error) &&
                         size == storage.size(),
                     "nullable pointer array has incorrect storage size");
    passed &= expect(bridge::NativePodArrayCodec::pack(module, Value::array({Value::null(), pointer}), nullableArray,
                                                       layouts, storage, error),
                     "nullable pointer array packing failed");
    auto decoded = bridge::NativePodArrayCodec::unpack(module, storage, nullableArray, layouts);
    passed &= expect(decoded.succeeded() && *decoded.value.element(0) == Value::null() &&
                         decoded.value.element(1)->opaquePointer() == &token,
                     "nullable pointer array round trip changed null/pointer values");
    passed &= expect(!bridge::NativePodArrayCodec::pack(module, Value::array({Value::null(), pointer}), pointerArray,
                                                        layouts, storage, error),
                     "null accepted in a non-nullable opaque array");
    passed &= expect(bridge::NativePodArrayCodec::pack(module, Value::array({pointer, pointer}), pointerArray, layouts,
                                                       storage, error),
                     "non-nullable pointer array packing failed");
    passed &= expect(!bridge::NativePodArrayCodec::pack(module, Value::array({Value::signedInteger(0), pointer}),
                                                        nullableArray, layouts, storage, error),
                     "integer accepted as an opaque pointer");
    passed &=
        expect(!bridge::NativePodArrayCodec::validateByteSize(module, nullable, layouts, sizeof(void*) - 1, error),
               "incorrect native pointer storage width accepted");
    for (const auto rejected : {nullableScalar, nestedNullable, wio::bytecode::InvalidIndex})
        passed &= expect(!bridge::NativePodArrayCodec::byteSize(module, rejected, layouts, size, error),
                         "non-POD nullable shape accepted");

    const auto pod = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 28,
                                               .name = 1,
                                               .nominalKind = 1,
                                               .nominalRepresentation = 1,
                                               .fields = {{.name = 0, .type = nullable}}});
    module.types.back().hasNativeBinding = true;
    module.contract.reflection.push_back({.stableTypeId = 500, .logicalName = 1, .type = pod, .nominalKind = 1});
    WioNativePodFieldDescriptor field{"pointer", 502, 0, sizeof(void*), 30};
    WioNativePodLayoutDescriptor descriptor{500, sizeof(void*), alignof(void*), 1, &field, &unusedPack, &readPointer};
    WioNativePodLayoutApi api{WIO_NATIVE_POD_LAYOUT_VERSION, 1, &descriptor};
    passed &= expect(bridge::mergeNativePodLayouts(module, api, layouts, error),
                     "valid nullable opaque field layout rejected");
    bridge::NativePodLayouts fresh;
    field.size -= 1;
    passed &= expect(!bridge::mergeNativePodLayouts(module, api, fresh, error) && fresh.types.empty(),
                     "invalid pointer width published a layout");
    field.size = sizeof(void*);
    module.types[pod].fields[0].type = nullableScalar;
    passed &= expect(!bridge::mergeNativePodLayouts(module, api, fresh, error) && fresh.types.empty(),
                     "nullable scalar field published a layout");
    module.types[pod].fields[0].type = nullable;

    void* parentPointer = &token;
    WioNativeAbiValue parent;
    parent.kind = WIO_NATIVE_ABI_POD;
    parent.typeId = 500;
    parent.payload.slice = {&parentPointer, sizeof(parentPointer)};
    for (readMode = 0; readMode <= 6; ++readMode)
    {
        const int before = releases;
        {
            const auto result = bridge::decodeNativeValue(module, parent, pod, 4, &layouts);
            passed &= expect(result.succeeded() == (readMode == 0 || readMode == 4 || readMode == 6),
                             "malformed/failed opaque field adapter was accepted");
            if (result.succeeded() && readMode == 0)
            {
                passed &= expect(releases == before && *static_cast<int*>(result.value.field(0)->opaquePointer()) == 17,
                                 "opaque field owner released before the decoded value died");
            }
            if (result.succeeded() && (readMode == 4 || readMode == 6))
                passed &= expect(*result.value.field(0) == Value::null(), "null native pointer was not nullable null");
        }
        passed &= expect(releases == before + 1, "opaque field owner was not released exactly once");
    }
    return passed ? 0 : 1;
}
