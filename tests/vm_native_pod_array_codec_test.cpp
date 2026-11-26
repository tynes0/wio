#include "native_pod_array_codec.h"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>

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

    int arrayReleases = 0;
    int readMode = 0;
    struct ArrayOwner
    {
        std::array<std::uint8_t, 2> bytes{1, 255};
    };
    const WioNativeAbiHandleOps ArrayOperations{WIO_NATIVE_ABI_VERSION, 0, [](void*) noexcept {},
                                                [](void* storage) noexcept
                                                {
                                                    ++arrayReleases;
                                                    delete static_cast<ArrayOwner*>(storage);
                                                },
                                                [](const void*) noexcept { return std::uint64_t{901}; }};

    WioNativeAbiStatus readArray(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                 WioNativeAbiFailure*) noexcept
    {
        auto* owner = new ArrayOwner;
        *result = {};
        result->kind = WIO_NATIVE_ABI_POD;
        result->typeId = readMode == 1 ? 902 : 901;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->payload.slice = {owner->bytes.data(), readMode == 2 ? 1u : 2u};
        result->owner = {owner, &ArrayOperations, 1};
        return readMode == 3 ? WIO_NATIVE_ABI_EXCEPTION : WIO_NATIVE_ABI_OK;
    }
} // namespace

int main()
{
    wio::bytecode::Module module;
    for (std::uint8_t kind = 0; kind <= 20; ++kind)
        module.types.push_back(wio::bytecode::Type{.kind = kind});
    const auto array = [&](std::uint32_t element, std::uint64_t count)
    {
        module.types.push_back(wio::bytecode::Type{.kind = 31, .arguments = {element}, .staticExtent = count});
        return static_cast<std::uint32_t>(module.types.size() - 1);
    };
    const auto bytesType = array(8, 2);
    const auto signedType = array(3, 2);
    const auto booleans = array(2, 2);
    const auto characters = array(16, 2);
    const auto row = array(5, 2);
    const auto matrix = array(row, 2);
    const auto empty = array(5, 0);
    const auto dynamic = array(5, (std::numeric_limits<std::uint64_t>::max)());
    const auto overflow = array(14, (std::numeric_limits<std::uint64_t>::max)() - 1);
    bridge::NativePodLayouts layouts;
    std::string error;
    std::size_t size = 0;
    bool passed = true;
    std::vector<std::byte> storage(2);
    auto values = Value::array({Value::unsignedInteger(0), Value::unsignedInteger(255)});
    passed &= expect(bridge::NativePodArrayCodec::pack(module, values, bytesType, layouts, storage, error),
                     "u8 fixed-array pack failed");
    const auto decoded = bridge::NativePodArrayCodec::unpack(module, storage, bytesType, layouts);
    passed &= expect(decoded.succeeded() && decoded.value.elementCount() == 2 &&
                         decoded.value.element(1)->asUnsignedInteger() == 255,
                     "u8 array round trip failed");
    passed &= expect(bridge::NativePodArrayCodec::pack(module, values, characters, layouts, storage, error),
                     "high-byte char array was rejected");
    values = Value::array({Value::unsignedInteger(0), Value::unsignedInteger(256)});
    passed &= expect(!bridge::NativePodArrayCodec::pack(module, values, characters, layouts, storage, error),
                     "out-of-range char storage was accepted");
    passed &= expect(!bridge::NativePodArrayCodec::pack(module, values, bytesType, layouts, storage, error),
                     "out-of-range u8 was accepted");
    passed &= expect(!bridge::NativePodArrayCodec::pack(module, Value::array({}), bytesType, layouts, storage, error),
                     "wrong fixed element count was accepted");
    passed &= expect(
        !bridge::NativePodArrayCodec::unpack(module, std::span{storage}.first(1), bytesType, layouts).succeeded(),
        "truncated fixed-array bytes were accepted");
    auto signedValues = Value::array({Value::signedInteger(-128), Value::signedInteger(127)});
    passed &= expect(bridge::NativePodArrayCodec::pack(module, signedValues, signedType, layouts, storage, error),
                     "signed array limits failed");
    signedValues = Value::array({Value::signedInteger(-128), Value::signedInteger(128)});
    passed &= expect(!bridge::NativePodArrayCodec::pack(module, signedValues, signedType, layouts, storage, error),
                     "signed element narrowing overflow was accepted");
    passed &= expect(bridge::NativePodArrayCodec::byteSize(module, booleans, layouts, size, error),
                     "bool array layout failed");
    storage.resize(size);
    passed &=
        expect(bridge::NativePodArrayCodec::pack(module, Value::array({Value::boolean(false), Value::boolean(true)}),
                                                 booleans, layouts, storage, error),
               "bool array packing failed");
    passed &= expect(bridge::NativePodArrayCodec::unpack(module, storage, booleans, layouts).succeeded(),
                     "bool array decoding failed");
    storage[0] = std::byte{2};
    passed &= expect(!bridge::NativePodArrayCodec::unpack(module, storage, booleans, layouts).succeeded(),
                     "invalid bool object representation was accepted");
    passed &= expect(bridge::NativePodArrayCodec::byteSize(module, matrix, layouts, size, error) &&
                         size == sizeof(std::array<std::array<std::int32_t, 2>, 2>),
                     "nested array layout mismatch");
    storage.resize(size);
    auto nested = Value::array({Value::array({Value::signedInteger(1), Value::signedInteger(2)}),
                                Value::array({Value::signedInteger(3), Value::signedInteger(4)})});
    passed &= expect(bridge::NativePodArrayCodec::pack(module, nested, matrix, layouts, storage, error),
                     "nested array pack failed");
    const auto nestedResult = bridge::NativePodArrayCodec::unpack(module, storage, matrix, layouts);
    passed &= expect(nestedResult.succeeded() && nestedResult.value.element(1)->element(1)->asSignedInteger() == 4,
                     "nested array unpack failed");
    passed &= expect(bridge::NativePodArrayCodec::byteSize(module, empty, layouts, size, error) && size != 0 &&
                         bridge::NativePodArrayCodec::validateByteSize(module, empty, layouts,
                                                                       sizeof(std::array<std::int32_t, 0>), error),
                     "empty array layout mismatch");
    storage.resize(sizeof(std::array<std::int32_t, 0>));
    passed &= expect(bridge::NativePodArrayCodec::pack(module, Value::array({}), empty, layouts, storage, error),
                     "empty array pack failed");
    const auto emptyResult = bridge::NativePodArrayCodec::unpack(module, storage, empty, layouts);
    passed &= expect(emptyResult.succeeded() && emptyResult.value.elementCount() == 0, "empty array unpack failed");
    const auto emptyRows = array(empty, 2);
    auto emptyValues = Value::array({Value::array({}), Value::array({})});
    for (const auto stride : {std::size_t{1}, sizeof(std::array<std::int32_t, 0>)})
    {
        storage.resize(stride * 2);
        passed &= expect(bridge::NativePodArrayCodec::pack(module, emptyValues, emptyRows, layouts, storage, error),
                         "producer-specific nested empty array stride was rejected");
        const auto result = bridge::NativePodArrayCodec::unpack(module, storage, emptyRows, layouts);
        passed &= expect(result.succeeded() && result.value.elementCount() == 2 &&
                             result.value.element(1)->elementCount() == 0,
                         "nested empty array unpack failed");
    }
    storage.clear();
    passed &= expect(!bridge::NativePodArrayCodec::unpack(module, storage, empty, layouts).succeeded(),
                     "zero-byte empty array storage accepted");
    for (const auto rejected : {dynamic, overflow, wio::bytecode::InvalidIndex})
        passed &= expect(!bridge::NativePodArrayCodec::byteSize(module, rejected, layouts, size, error),
                         "unsupported/overflowing array layout accepted");
    passed &= expect(!bridge::NativePodArrayCodec::byteSize(module, matrix, layouts, size, error, 65),
                     "native array recursion budget ignored");

    // Descriptor registration is transactional and must reject incompatible
    // array layouts before any adapter code is invoked.
    module.strings = {"values", "ArrayPod"};
    const auto pod = static_cast<std::uint32_t>(module.types.size());
    module.types.push_back(wio::bytecode::Type{.kind = 28,
                                               .name = 1,
                                               .nominalKind = 1,
                                               .nominalRepresentation = 1,
                                               .fields = {{.name = 0, .type = bytesType}}});
    module.types.back().hasNativeBinding = true;
    module.contract.reflection.push_back({.stableTypeId = 900, .logicalName = 1, .type = pod, .nominalKind = 1});
    WioNativePodFieldDescriptor field{"values", 901, 0, 2, 31};
    WioNativePodLayoutDescriptor descriptor{900, 2, 1, 1, &field, &unusedPack, &unusedRead};
    WioNativePodLayoutApi api{WIO_NATIVE_POD_LAYOUT_VERSION, 1, &descriptor};
    passed &= expect(bridge::mergeNativePodLayouts(module, api, layouts, error), "valid array field layout rejected");
    const auto old = layouts.types;
    bridge::NativePodLayouts fresh;
    field.size = 1;
    passed &= expect(!bridge::mergeNativePodLayouts(module, api, fresh, error) && fresh.types.empty(),
                     "incorrect array byte extent published a layout");
    field.size = 2;
    const auto oldExtent = module.types[bytesType].staticExtent;
    module.types[bytesType].staticExtent = (std::numeric_limits<std::uint64_t>::max)();
    passed &= expect(!bridge::mergeNativePodLayouts(module, api, layouts, error) && layouts.types == old,
                     "dynamic array accepted or failed registration mutated published layouts");
    module.types[bytesType].staticExtent = oldExtent;

    descriptor.read = &readArray;
    bridge::NativePodLayouts readerLayouts;
    passed &= expect(bridge::mergeNativePodLayouts(module, api, readerLayouts, error), "array read adapter rejected");
    const std::array<std::uint8_t, 2> parentBytes{1, 255};
    WioNativeAbiValue parent;
    parent.kind = WIO_NATIVE_ABI_POD;
    parent.typeId = 900;
    parent.payload.slice = {parentBytes.data(), parentBytes.size()};
    for (readMode = 0; readMode < 4; ++readMode)
    {
        const auto before = arrayReleases;
        const auto result = bridge::decodeNativeValue(module, parent, pod, 4, &readerLayouts);
        passed &= expect(result.succeeded() == (readMode == 0), "malformed/failed array read was accepted");
        passed &= expect(arrayReleases == before + 1, "array read owner was not released exactly once");
        if (result.succeeded())
            passed &= expect(result.value.field(0)->element(1)->asUnsignedInteger() == 255,
                             "decoded array borrowed released producer storage");
    }
    return passed ? 0 : 1;
}
