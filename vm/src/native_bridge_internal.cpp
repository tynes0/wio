#include "native_bridge_internal.h"
#include "native_pod_internal.h"
#include "native_sequence_internal.h"
#include "native_dictionary_internal.h"
#include "native_any_internal.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t TypeVoid = 1;
        constexpr std::uint8_t TypeBool = 2;
        constexpr std::uint8_t TypeI8 = 3;
        constexpr std::uint8_t TypeISize = 7;
        constexpr std::uint8_t TypeU8 = 8;
        constexpr std::uint8_t TypeUSize = 12;
        constexpr std::uint8_t TypeF32 = 13;
        constexpr std::uint8_t TypeF64 = 14;
        constexpr std::uint8_t TypeByte = 15;
        constexpr std::uint8_t TypeChar = 16;
        constexpr std::uint8_t TypeString = 17;
        constexpr std::uint8_t TypeText = 18;
        constexpr std::uint8_t TypeAny = 19;
        constexpr std::uint8_t TypeOpaque = 20;
        constexpr std::uint8_t TypeNamed = 28;
        constexpr std::uint8_t TypeReference = 29;
        constexpr std::uint8_t TypeNullable = 30;
        constexpr std::uint8_t TypeArray = 31;
        constexpr std::uint8_t TypeDictionary = 32;
        constexpr std::uint8_t TypeFunction = 33;
        constexpr std::uint8_t TypeAsyncTask = 34;
        constexpr std::uint8_t NominalObject = 2;
        constexpr std::uint8_t NominalInterface = 3;
        constexpr std::uint8_t NominalEnum = 4;
        constexpr std::uint8_t NominalFlagset = 5;
        constexpr std::uint8_t RepresentationNativePod = 1;

        struct OwnedCallbackValue final
        {
            std::atomic<std::uint32_t> references{1};
            Value value;

            explicit OwnedCallbackValue(const Value& input) : value(input)
            {
            }

            static const WioNativeAbiHandleOps Operations;
        };

        const WioNativeAbiHandleOps OwnedCallbackValue::Operations{
            WIO_NATIVE_ABI_VERSION, 0, [](void* storage) noexcept
            { static_cast<OwnedCallbackValue*>(storage)->references.fetch_add(1, std::memory_order_relaxed); },
            [](void* storage) noexcept
            {
                auto* owner = static_cast<OwnedCallbackValue*>(storage);
                if (owner->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
                    delete owner;
            },
            [](const void* storage) noexcept
            { return static_cast<const OwnedCallbackValue*>(storage)->value.foreignTypeId(); }};

        [[nodiscard]] bool signedScalarFits(const std::uint8_t kind, const std::int64_t value) noexcept
        {
            const unsigned bits = kind == TypeISize ? sizeof(std::intptr_t) * 8u : 8u << (kind - TypeI8);
            if (bits == 64)
                return true;
            const auto maximum = (std::int64_t{1} << (bits - 1)) - 1;
            return value >= -maximum - 1 && value <= maximum;
        }

        [[nodiscard]] bool unsignedScalarFits(const std::uint8_t kind, const std::uint64_t value) noexcept
        {
            const unsigned bits = kind == TypeByte    ? 8u
                                  : kind == TypeChar  ? 32u
                                  : kind == TypeUSize ? sizeof(std::uintptr_t) * 8u
                                                      : 8u << (kind - TypeU8);
            return bits == 64 || value <= (std::uint64_t{1} << bits) - 1;
        }

        [[nodiscard]] std::uint32_t directType(const bytecode::Module& module, std::uint32_t type) noexcept
        {
            for (std::uint32_t depth = 0; type < module.types.size() && depth < 32; ++depth)
            {
                const bytecode::Type& candidate = module.types[type];
                if ((candidate.kind != TypeReference && candidate.kind != TypeNullable) || candidate.arguments.empty())
                    return type;
                type = candidate.arguments.front();
            }
            return bytecode::InvalidIndex;
        }

        [[nodiscard]] std::uint32_t scalarType(const bytecode::Module& module, std::uint32_t type) noexcept
        {
            type = directType(module, type);
            if (type >= module.types.size())
                return bytecode::InvalidIndex;
            const bytecode::Type& candidate = module.types[type];
            if (candidate.kind == TypeNamed &&
                (candidate.nominalKind == NominalEnum || candidate.nominalKind == NominalFlagset))
                return directType(module, candidate.enumUnderlyingType);
            return type;
        }

        [[nodiscard]] bool isNullable(const bytecode::Module& module, std::uint32_t type) noexcept
        {
            for (unsigned depth = 0; type < module.types.size() && depth < 32; ++depth)
            {
                const auto& candidate = module.types[type];
                if (candidate.kind != TypeReference || candidate.arguments.empty())
                    return candidate.kind == TypeNullable;
                type = candidate.arguments.front();
            }
            return false;
        }

        [[nodiscard]] bool isUnicodeScalar(const char32_t scalar) noexcept
        {
            return scalar <= 0x10ffff && (scalar < 0xd800 || scalar > 0xdfff);
        }

        [[nodiscard]] std::string sliceString(const WioNativeAbiSlice slice)
        {
            if (!slice.data || slice.size == 0)
                return {};
            const auto size = static_cast<std::size_t>(
                (std::min)(slice.size, static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
            return std::string{static_cast<const char*>(slice.data), size};
        }

        [[nodiscard]] std::uint64_t stableTypeId(const bytecode::Module& module,
                                                 const std::uint32_t declaredType) noexcept
        {
            const std::uint32_t type = directType(module, declaredType);
            for (const bytecode::Reflection& reflection : module.contract.reflection)
            {
                if (reflection.type == type)
                    return reflection.stableTypeId;
            }
            return 0;
        }

        [[nodiscard]] bool validHandle(const WioNativeAbiHandle handle) noexcept
        {
            return handle.state != nullptr && handle.ops != nullptr &&
                   handle.ops->abiVersion == WIO_NATIVE_ABI_VERSION && handle.ops->retain != nullptr &&
                   handle.ops->release != nullptr && handle.ops->typeId != nullptr;
        }

        [[nodiscard]] bool validateWireType(const WioNativeAbiValue& value, const std::uint64_t expected,
                                            std::string& error)
        {
            if (expected == 0)
            {
                error = "native value type has no stable ABI type identity";
                return false;
            }
            if (value.typeId != expected)
            {
                error = "native value stable type identity does not match its Wio declaration";
                return false;
            }
            return true;
        }
    } // namespace

    bool encodeNativeValue(const bytecode::Module& module, const Value& value, const std::uint32_t declaredType,
                           const std::uint8_t marshalling, WioNativeAbiValue& output, std::string& error,
                           const NativePodLayouts* layouts, const unsigned depth, const NativeObjectAccess* objects)
    {
        output = {};
        if (depth > 64)
        {
            error = "native POD nesting exceeds the bridge depth limit";
            return false;
        }
        if (value.kind() == Value::Kind::Null)
        {
            const auto type = scalarType(module, declaredType);
            if (marshalling == 8 && type < module.types.size() && module.types[type].kind == TypeAny && layouts)
                return packNativeAny(module, value, type, *layouts, output, error, depth, objects);
            if (!isNullable(module, declaredType))
            {
                error = "null VM value does not match a nullable native ABI declaration";
                return false;
            }
            output.kind = WIO_NATIVE_ABI_VOID;
            output.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
            return true;
        }

        const std::uint32_t type = scalarType(module, declaredType);
        if (type >= module.types.size())
        {
            error = "native value references an invalid bytecode type";
            return false;
        }
        const std::uint8_t kind = module.types[type].kind;
        switch (marshalling)
        {
        case 0:
            if (kind == TypeVoid && value.isEmpty())
                return true;
            error = "non-void VM value cannot use void native marshalling";
            return false;
        case 1:
            if (kind == TypeBool && value.kind() == Value::Kind::Boolean)
            {
                output.kind = WIO_NATIVE_ABI_BOOL;
                output.payload.boolean = value.asBoolean();
                return true;
            }
            if (kind >= TypeI8 && kind <= TypeISize && value.kind() == Value::Kind::SignedInteger)
            {
                if (!signedScalarFits(kind, value.asSignedInteger()))
                {
                    error = "VM signed scalar is outside the native ABI declaration's range";
                    return false;
                }
                output.kind = WIO_NATIVE_ABI_I64;
                output.payload.signedInteger = value.asSignedInteger();
                return true;
            }
            if (((kind >= TypeU8 && kind <= TypeUSize) || kind == TypeByte || kind == TypeChar) &&
                value.kind() == Value::Kind::UnsignedInteger)
            {
                if (!unsignedScalarFits(kind, value.asUnsignedInteger()))
                {
                    error = "VM unsigned scalar is outside the native ABI declaration's range";
                    return false;
                }
                output.kind = WIO_NATIVE_ABI_U64;
                output.payload.unsignedInteger = value.asUnsignedInteger();
                return true;
            }
            if ((kind == TypeF32 || kind == TypeF64) && value.kind() == Value::Kind::Float64)
            {
                output.kind = WIO_NATIVE_ABI_F64;
                output.payload.floatingPoint =
                    kind == TypeF32 ? static_cast<float>(value.asFloat64()) : value.asFloat64();
                return true;
            }
            error = "VM scalar kind does not match the native ABI declaration";
            return false;
        case 2:
            if (kind == TypeString && value.kind() == Value::Kind::String)
            {
                output.kind = WIO_NATIVE_ABI_BYTES;
                output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                output.payload.slice = {value.asString().data(), value.asString().size()};
                return true;
            }
            error = "VM value is not a UTF-8 string required by the native ABI";
            return false;
        case 3:
            if (kind == TypeText && value.kind() == Value::Kind::Text)
            {
                for (const char32_t scalar : value.asText())
                {
                    if (!isUnicodeScalar(scalar))
                    {
                        error = "VM Unicode text contains an invalid scalar";
                        return false;
                    }
                }
                output.kind = WIO_NATIVE_ABI_TEXT_UTF32;
                output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                output.payload.slice = {value.asText().data(), value.asText().size() * sizeof(char32_t)};
                return true;
            }
            error = "VM value is not Unicode text required by the native ABI";
            return false;
        case 4:
        {
            const std::uint64_t expected = stableTypeId(module, declaredType);
            if (value.kind() == Value::Kind::Component && layouts)
                return packNativePod(module, value, type, *layouts, output, error, depth);
            if (value.kind() != Value::Kind::NativePod || value.aggregateType() != type ||
                value.foreignTypeId() != expected || expected == 0)
            {
                error = "VM value is not the native POD required by the ABI declaration";
                return false;
            }
            const std::span bytes = value.nativePodBytes();
            if (layouts)
            {
                const auto layout = layouts->types.find(type);
                if (layout != layouts->types.end() && bytes.size() != layout->second.size)
                {
                    error = "VM native POD byte size does not match its producer layout";
                    return false;
                }
            }
            output.kind = WIO_NATIVE_ABI_POD;
            output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            output.typeId = expected;
            output.payload.slice = {bytes.data(), bytes.size()};
            return true;
        }
        case 5:
        {
            const std::uint64_t expected = stableTypeId(module, declaredType);
            if (kind != TypeOpaque || value.kind() != Value::Kind::Opaque || value.aggregateType() != type ||
                (expected != 0 && value.foreignTypeId() != expected))
            {
                error = "VM value is not the opaque handle required by the ABI declaration";
                return false;
            }
            output.kind = WIO_NATIVE_ABI_OPAQUE;
            output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            output.typeId = expected;
            output.payload.opaque = value.opaquePointer();
            return true;
        }
        case 6:
        case 8:
        {
            const bool object = marshalling == 6;
            if (object && layouts && objects && value.objectIdentity())
                return objects->encode(objects->state, value, declaredType, output, error, *layouts);
            if (!object && kind == TypeArray && value.kind() == Value::Kind::Array && layouts)
                return packNativeSequence(module, value, type, *layouts, output, error, depth, objects);
            if (!object && kind == TypeDictionary && value.kind() == Value::Kind::Dictionary && layouts)
                return packNativeDictionary(module, value, type, *layouts, output, error, depth, objects);
            if (!object && kind == TypeAny && value.kind() == Value::Kind::Any && layouts)
                return packNativeAny(module, value, type, *layouts, output, error, depth, objects);
            const std::uint64_t expected = stableTypeId(module, declaredType);
            if (value.kind() != (object ? Value::Kind::ForeignObject : Value::Kind::RuntimeValue) ||
                value.aggregateType() != type || value.foreignTypeId() != expected || expected == 0)
            {
                error = object ? "VM value is not the foreign object required by the ABI declaration"
                               : "VM value is not the runtime value required by the ABI declaration";
                return false;
            }
            const WioNativeAbiHandle handle = value.foreignHandle();
            if (!validHandle(handle) || handle.ops->typeId(handle.state) != expected)
            {
                error = "VM foreign handle has invalid producer operations or type identity";
                return false;
            }
            output.kind = object ? WIO_NATIVE_ABI_OBJECT : WIO_NATIVE_ABI_RUNTIME_VALUE;
            output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            output.typeId = expected;
            output.payload.handle = handle;
            return true;
        }
        default:
            error = "native marshalling kind is not implemented by this VM bridge slice";
            return false;
        }
    }

    bool encodeOwnedNativeValue(const bytecode::Module& module, const Value& value, const std::uint32_t declaredType,
                                const std::uint8_t marshalling, WioNativeAbiValue& output, std::string& error,
                                const NativePodLayouts* layouts)
    {
        if (!encodeNativeValue(module, value, declaredType, marshalling, output, error, layouts))
            return false;
        if (output.kind != WIO_NATIVE_ABI_BYTES && output.kind != WIO_NATIVE_ABI_TEXT_UTF32 &&
            output.kind != WIO_NATIVE_ABI_POD)
            return true;

        // A POD pack adapter can return producer-owned bytes. Copy those bytes
        // into VM-owned storage so an escaped callback result does not depend
        // on the producer DLL or the completed callback ExecutionResult.
        Value owned = value;
        if (output.kind == WIO_NATIVE_ABI_POD)
        {
            struct Release
            {
                WioNativeAbiValue& wire;
                ~Release()
                {
                    WioNativeAbiReleaseValue(&wire);
                }
            } release{output};
            owned = Value::nativePod(directType(module, declaredType), output.typeId,
                                     {static_cast<const std::byte*>(output.payload.slice.data),
                                      static_cast<std::size_t>(output.payload.slice.size)});
        }
        auto owner = std::make_unique<OwnedCallbackValue>(owned);
        // Encode from the owned copy: no slice may refer to temporary frame storage.
        if (!encodeNativeValue(module, owner->value, declaredType, marshalling, output, error))
            return false;
        output.flags = WIO_NATIVE_ABI_VALUE_OWNED;
        output.owner = {owner.release(), &OwnedCallbackValue::Operations, 1};
        return true;
    }

    NativeValueResult decodeNativeValue(const bytecode::Module& module, const WioNativeAbiValue& value,
                                        const std::uint32_t declaredType, const std::uint8_t marshalling,
                                        const NativePodLayouts* layouts, const unsigned depth,
                                        const NativeObjectAccess* objects)
    {
        if (depth > 64)
            return {{}, "native POD nesting exceeds the bridge depth limit"};
        const bool nullable = isNullable(module, declaredType);
        if (value.kind == WIO_NATIVE_ABI_VOID && nullable)
            return {Value::null(), {}};

        const std::uint32_t type = scalarType(module, declaredType);
        if (type >= module.types.size())
            return {{}, "native result references an invalid bytecode type"};
        const std::uint8_t kind = module.types[type].kind;
        switch (marshalling)
        {
        case 0:
            return value.kind == WIO_NATIVE_ABI_VOID ? NativeValueResult{}
                                                     : NativeValueResult{{}, "native result is not void"};
        case 1:
            if (kind == TypeBool && value.kind == WIO_NATIVE_ABI_BOOL)
                return {Value::boolean(value.payload.boolean), {}};
            if (kind >= TypeI8 && kind <= TypeISize && value.kind == WIO_NATIVE_ABI_I64)
            {
                if (!signedScalarFits(kind, value.payload.signedInteger))
                    return {{}, "native signed scalar result is outside the Wio result type's range"};
                return {Value::signedInteger(value.payload.signedInteger), {}};
            }
            if (((kind >= TypeU8 && kind <= TypeUSize) || kind == TypeByte || kind == TypeChar) &&
                value.kind == WIO_NATIVE_ABI_U64)
            {
                if (!unsignedScalarFits(kind, value.payload.unsignedInteger))
                    return {{}, "native unsigned scalar result is outside the Wio result type's range"};
                return {Value::unsignedInteger(value.payload.unsignedInteger), {}};
            }
            if ((kind == TypeF32 || kind == TypeF64) && value.kind == WIO_NATIVE_ABI_F64)
                return {Value::floating(kind == TypeF32 ? static_cast<float>(value.payload.floatingPoint)
                                                        : value.payload.floatingPoint),
                        {}};
            return {{}, "native scalar result does not match the Wio result type"};
        case 2:
            if (kind != TypeString || value.kind != WIO_NATIVE_ABI_BYTES ||
                (!value.payload.slice.data && value.payload.slice.size != 0) ||
                value.payload.slice.size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
                return {{}, "native UTF-8 string result has an invalid wire shape"};
            return {Value::string(sliceString(value.payload.slice)), {}};
        case 3:
        {
            if (kind != TypeText || value.kind != WIO_NATIVE_ABI_TEXT_UTF32 ||
                value.payload.slice.size % sizeof(char32_t) != 0 ||
                (!value.payload.slice.data && value.payload.slice.size != 0) ||
                value.payload.slice.size / sizeof(char32_t) >
                    static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
                return {{}, "native Unicode text result has an invalid wire shape"};
            const std::size_t count = static_cast<std::size_t>(value.payload.slice.size / sizeof(char32_t));
            std::u32string text(count, U'\0');
            if (count != 0)
                std::memcpy(text.data(), value.payload.slice.data, count * sizeof(char32_t));
            for (const char32_t scalar : text)
            {
                if (!isUnicodeScalar(scalar))
                    return {{}, "native Unicode text result contains an invalid scalar"};
            }
            return {Value::text(std::move(text)), {}};
        }
        case 4:
        {
            std::string typeError;
            const std::uint64_t expected = stableTypeId(module, declaredType);
            if (value.kind != WIO_NATIVE_ABI_POD || !validateWireType(value, expected, typeError) ||
                (!value.payload.slice.data && value.payload.slice.size != 0) ||
                value.payload.slice.size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
                return {{}, typeError.empty() ? "native POD result has an invalid wire shape" : std::move(typeError)};
            const auto* data = static_cast<const std::byte*>(value.payload.slice.data);
            if (layouts && layouts->types.contains(type))
                return unpackNativePod(module, value, type, *layouts, depth);
            return {
                Value::nativePod(type, expected,
                                 std::span<const std::byte>{data, static_cast<std::size_t>(value.payload.slice.size)}),
                {}};
        }
        case 5:
        {
            std::string typeError;
            const std::uint64_t expected = stableTypeId(module, declaredType);
            if (kind != TypeOpaque || (expected != 0 && !validateWireType(value, expected, typeError)) ||
                value.kind != WIO_NATIVE_ABI_OPAQUE)
                return {{},
                        typeError.empty() ? "native opaque result has an invalid wire shape" : std::move(typeError)};
            if (value.owner.state != nullptr && !validHandle(value.owner))
                return {{}, "native opaque result has invalid producer ownership operations"};
            if (nullable && value.payload.opaque == nullptr)
                return {Value::null(), {}};
            return {Value::opaque(type, expected, value.payload.opaque, value.owner), {}};
        }
        case 6:
        case 8:
        {
            const bool object = marshalling == 6;
            if (object && layouts && objects)
                return objects->decode(objects->state, value, declaredType, *layouts);
            std::string typeError;
            const std::uint64_t expected = stableTypeId(module, declaredType);
            if (value.kind != (object ? WIO_NATIVE_ABI_OBJECT : WIO_NATIVE_ABI_RUNTIME_VALUE) ||
                !validateWireType(value, expected, typeError))
                return {{}, typeError.empty() ? "native handle result has an invalid wire kind" : std::move(typeError)};
            if (!validHandle(value.payload.handle) ||
                value.payload.handle.ops->typeId(value.payload.handle.state) != expected)
                return {{}, "native handle result has invalid producer operations or type identity"};
            if (!object && kind == TypeArray && layouts && layouts->sequences.contains(type))
                return unpackNativeSequence(module, value, type, *layouts, depth, objects);
            if (!object && kind == TypeDictionary && layouts && layouts->dictionaries.contains(type))
                return unpackNativeDictionary(module, value, type, *layouts, depth, objects);
            if (!object && kind == TypeAny && layouts && layouts->any.contains(type))
                return unpackNativeAny(module, value, type, *layouts, depth, objects);
            return {object ? Value::foreignObject(type, expected, value.payload.handle)
                           : Value::runtimeValue(type, expected, value.payload.handle),
                    {}};
        }
        default:
            return {{}, "native result marshalling kind is not implemented by this VM bridge slice"};
        }
    }

    bool callbackValueMarshalling(const bytecode::Module& module, const std::uint32_t declaredType,
                                  std::uint8_t& marshalling, const NativePodLayouts* layouts) noexcept
    {
        // Callback borrows need explicit reference tokens and copy-back, not a
        // by-value slice. Do not silently classify them as their pointee type.
        if (declaredType < module.types.size() && module.types[declaredType].kind == TypeReference)
            return false;
        const std::uint32_t type = scalarType(module, declaredType);
        if (type >= module.types.size())
            return false;
        const std::uint8_t kind = module.types[type].kind;
        const bool hostedObject =
            kind == TypeNamed &&
            (module.types[type].nominalKind == NominalObject || module.types[type].nominalKind == NominalInterface) &&
            layouts && layouts->objectContracts.contains(type);
        if (isNullable(module, declaredType) && !hostedObject)
            return false;
        if (hostedObject)
            marshalling = 6;
        else if (kind == TypeVoid)
            marshalling = 0;
        else if (kind == TypeBool || (kind >= TypeI8 && kind <= TypeF64) || kind == TypeByte || kind == TypeChar)
            marshalling = 1;
        else if (kind == TypeString)
            marshalling = 2;
        else if (kind == TypeText)
            marshalling = 3;
        else if (kind == TypeNamed && module.types[type].nominalRepresentation == RepresentationNativePod && layouts &&
                 layouts->types.contains(type))
            marshalling = 4;
        else
            return false;
        return true;
    }

    bool nativeValueMarshalling(const bytecode::Module& module, const std::uint32_t declaredType,
                                std::uint8_t& marshalling) noexcept
    {
        const std::uint32_t type = scalarType(module, declaredType);
        if (type >= module.types.size())
            return false;
        const bytecode::Type& value = module.types[type];
        if (value.kind == TypeVoid)
            marshalling = 0;
        else if (value.kind == TypeBool || (value.kind >= TypeI8 && value.kind <= TypeF64) || value.kind == TypeByte ||
                 value.kind == TypeChar ||
                 (value.kind == TypeNamed && (value.nominalKind == NominalEnum || value.nominalKind == NominalFlagset)))
            marshalling = 1;
        else if (value.kind == TypeString)
            marshalling = 2;
        else if (value.kind == TypeText)
            marshalling = 3;
        else if (value.kind == TypeNamed && value.nominalRepresentation == RepresentationNativePod)
            marshalling = 4;
        else if (value.kind == TypeOpaque)
            marshalling = 5;
        else if (value.kind == TypeNamed &&
                 (value.nominalKind == NominalObject || value.nominalKind == NominalInterface))
            marshalling = 6;
        else if (value.kind == TypeFunction)
            marshalling = 7;
        else if (value.kind == TypeAny || value.kind == TypeArray || value.kind == TypeDictionary ||
                 value.kind == TypeAsyncTask || value.kind == TypeNamed)
            marshalling = 8;
        else
            return false;
        return true;
    }

    std::string nativeFailureMessage(const WioNativeAbiFailure& failure)
    {
        std::string message = sliceString(failure.message);
        if (message.empty())
            message = "native thunk returned ABI status " + std::to_string(static_cast<std::int32_t>(failure.status));
        const std::string category = sliceString(failure.category);
        if (!category.empty())
            message = category + ": " + message;
        if (failure.nativeCode != 0)
            message += " (native code " + std::to_string(failure.nativeCode) + ")";
        return message;
    }
} // namespace wio::vm::detail
