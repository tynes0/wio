#include "native_pod_array_codec.h"

#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t TypeNamed = 28;
        constexpr std::uint8_t TypeArray = 31;

        template <class Visitor> bool scalarStorage(std::uint8_t kind, Visitor&& visitor)
        {
            switch (kind)
            {
            case 2:
                return visitor.template operator()<bool>();
            case 3:
                return visitor.template operator()<std::int8_t>();
            case 4:
                return visitor.template operator()<std::int16_t>();
            case 5:
                return visitor.template operator()<std::int32_t>();
            case 6:
                return visitor.template operator()<std::int64_t>();
            case 7:
                return visitor.template operator()<std::intptr_t>();
            case 8:
                return visitor.template operator()<std::uint8_t>();
            case 9:
                return visitor.template operator()<std::uint16_t>();
            case 10:
                return visitor.template operator()<std::uint32_t>();
            case 11:
                return visitor.template operator()<std::uint64_t>();
            case 12:
                return visitor.template operator()<std::uintptr_t>();
            case 13:
                return visitor.template operator()<float>();
            case 14:
                return visitor.template operator()<double>();
            case 15:
            case 16:
                return visitor.template operator()<std::uint8_t>();
            default:
                return false;
            }
        }

        bool reject(std::string& error, const char* message)
        {
            error = message;
            return false;
        }

        struct WireGuard
        {
            WioNativeAbiValue value;
            ~WireGuard()
            {
                WioNativeAbiReleaseValue(&value);
            }
        };
    } // namespace

    bool NativePodArrayCodec::byteSize(const bytecode::Module& module, std::uint32_t type,
                                       const NativePodLayouts& layouts, std::size_t& size, std::string& error,
                                       unsigned depth)
    {
        if (depth > 64 || type >= module.types.size())
            return reject(error, "native POD array has an invalid type or excessive nesting");
        const auto& definition = module.types[type];
        if (scalarStorage(definition.kind,
                          [&]<class T>()
                          {
                              size = sizeof(T);
                              return true;
                          }))
            return true;
        if (definition.kind == TypeNamed && (definition.nominalKind == 4 || definition.nominalKind == 5))
            return byteSize(module, definition.enumUnderlyingType, layouts, size, error, depth + 1);
        if (definition.kind == TypeNamed && definition.nominalRepresentation == 1)
        {
            const auto found = layouts.types.find(type);
            if (found == layouts.types.end() || !found->second.size ||
                found->second.size > (std::numeric_limits<std::size_t>::max)())
                return reject(error, "native POD array element has no registered layout");
            size = static_cast<std::size_t>(found->second.size);
            return true;
        }
        if (definition.kind != TypeArray || definition.arguments.size() != 1 ||
            definition.staticExtent == (std::numeric_limits<std::uint64_t>::max)() ||
            definition.extentParameter != bytecode::InvalidIndex)
            return reject(error, "native POD array requires a concrete fixed extent and supported element type");
        std::size_t elementSize = 0;
        if (!byteSize(module, definition.arguments.front(), layouts, elementSize, error, depth + 1))
            return false;
        if (definition.staticExtent > (std::numeric_limits<std::size_t>::max)() / elementSize)
            return reject(error, "native POD array byte size overflows the host size type");
        // This is the minimum extent. Empty std::array storage differs between
        // C++ libraries; validateByteSize uses the producer's actual field size.
        size = definition.staticExtent ? static_cast<std::size_t>(definition.staticExtent) * elementSize : 1;
        return true;
    }

    bool NativePodArrayCodec::validateByteSize(const bytecode::Module& module, std::uint32_t type,
                                               const NativePodLayouts& layouts, std::size_t size, std::string& error,
                                               unsigned depth)
    {
        std::size_t minimum = 0;
        if (!byteSize(module, type, layouts, minimum, error, depth))
            return false;
        const auto& definition = module.types[type];
        if (definition.kind != TypeArray)
            return size == minimum || reject(error, "native POD array element byte extent is invalid");
        if (!definition.staticExtent)
            return size != 0 || reject(error, "empty native POD array has zero storage extent");
        if (size < minimum || size % definition.staticExtent)
            return reject(error, "native POD array has an invalid contiguous storage extent");
        return validateByteSize(module, definition.arguments.front(), layouts,
                                size / static_cast<std::size_t>(definition.staticExtent), error, depth + 1);
    }

    bool NativePodArrayCodec::pack(const bytecode::Module& module, const Value& value, std::uint32_t type,
                                   const NativePodLayouts& layouts, std::span<std::byte> bytes, std::string& error,
                                   unsigned depth)
    {
        if (!validateByteSize(module, type, layouts, bytes.size(), error, depth))
            return reject(error, "native POD array packing has an invalid byte extent or element layout");
        const auto& definition = module.types[type];
        if (definition.kind == TypeArray)
        {
            if (value.kind() != Value::Kind::Array || value.elementCount() != definition.staticExtent)
                return reject(error, "VM native POD array does not match its fixed element count");
            const auto elementSize = value.elementCount() ? bytes.size() / value.elementCount() : 0;
            for (std::size_t index = 0; index < value.elementCount(); ++index)
                if (!pack(module, *value.element(index), definition.arguments.front(), layouts,
                          bytes.subspan(index * elementSize, elementSize), error, depth + 1))
                    return false;
            return true;
        }
        WireGuard wire;
        std::uint8_t marshalling = 0;
        if (!nativeValueMarshalling(module, type, marshalling) ||
            !encodeNativeValue(module, value, type, marshalling, wire.value, error, &layouts, depth + 1))
            return false;
        if (definition.kind == TypeNamed && definition.nominalRepresentation == 1 && definition.nominalKind == 1)
        {
            if (wire.value.kind != WIO_NATIVE_ABI_POD || wire.value.payload.slice.size != bytes.size() ||
                !wire.value.payload.slice.data)
                return reject(error, "native POD array element returned an invalid producer byte extent");
            std::memcpy(bytes.data(), wire.value.payload.slice.data, bytes.size());
            return true;
        }
        const auto kind =
            definition.kind == TypeNamed ? module.types[definition.enumUnderlyingType].kind : definition.kind;
        return scalarStorage(kind,
                             [&]<class T>()
                             {
                                 T scalar{};
                                 if constexpr (std::is_same_v<T, bool>)
                                     scalar = wire.value.payload.boolean;
                                 else if constexpr (std::is_floating_point_v<T>)
                                     scalar = static_cast<T>(wire.value.payload.floatingPoint);
                                 else if constexpr (std::is_signed_v<T>)
                                     scalar = static_cast<T>(wire.value.payload.signedInteger);
                                 else
                                 {
                                     if (wire.value.payload.unsignedInteger > (std::numeric_limits<T>::max)())
                                         return reject(
                                             error, "native POD array unsigned element is outside its storage range");
                                     scalar = static_cast<T>(wire.value.payload.unsignedInteger);
                                 }
                                 std::memcpy(bytes.data(), &scalar, sizeof(T));
                                 return true;
                             });
    }

    NativeValueResult NativePodArrayCodec::unpack(const bytecode::Module& module, std::span<const std::byte> bytes,
                                                  std::uint32_t type, const NativePodLayouts& layouts, unsigned depth)
    {
        std::string error;
        if (!validateByteSize(module, type, layouts, bytes.size(), error, depth))
            return {{}, std::move(error)};
        const auto& definition = module.types[type];
        if (definition.kind == TypeArray)
        {
            const auto elementSize =
                definition.staticExtent ? bytes.size() / static_cast<std::size_t>(definition.staticExtent) : 0;
            std::vector<Value> elements;
            elements.reserve(static_cast<std::size_t>(definition.staticExtent));
            for (std::size_t index = 0; index < definition.staticExtent; ++index)
            {
                auto element = unpack(module, bytes.subspan(index * elementSize, elementSize),
                                      definition.arguments.front(), layouts, depth + 1);
                if (!element.succeeded())
                    return element;
                elements.push_back(std::move(element.value));
            }
            return {Value::array(std::move(elements)), {}};
        }
        WioNativeAbiValue wire;
        if (definition.kind == TypeNamed && definition.nominalRepresentation == 1 && definition.nominalKind == 1)
        {
            wire.kind = WIO_NATIVE_ABI_POD;
            wire.typeId = layouts.types.at(type).typeId;
            wire.payload.slice = {bytes.data(), bytes.size()};
            return decodeNativeValue(module, wire, type, 4, &layouts, depth + 1);
        }
        const auto kind =
            definition.kind == TypeNamed ? module.types[definition.enumUnderlyingType].kind : definition.kind;
        const bool valid = scalarStorage(kind,
                                         [&]<class T>()
                                         {
                                             T scalar{};
                                             if constexpr (std::is_same_v<T, bool>)
                                             {
                                                 const bool zero = false;
                                                 const bool one = true;
                                                 if (std::memcmp(bytes.data(), &zero, sizeof(bool)) == 0)
                                                     scalar = false;
                                                 else if (std::memcmp(bytes.data(), &one, sizeof(bool)) == 0)
                                                     scalar = true;
                                                 else
                                                     return false;
                                                 wire.kind = WIO_NATIVE_ABI_BOOL;
                                                 wire.payload.boolean = scalar;
                                             }
                                             else
                                             {
                                                 std::memcpy(&scalar, bytes.data(), sizeof(T));
                                                 if constexpr (std::is_floating_point_v<T>)
                                                 {
                                                     wire.kind = WIO_NATIVE_ABI_F64;
                                                     wire.payload.floatingPoint = scalar;
                                                 }
                                                 else if constexpr (std::is_signed_v<T>)
                                                 {
                                                     wire.kind = WIO_NATIVE_ABI_I64;
                                                     wire.payload.signedInteger = scalar;
                                                 }
                                                 else
                                                 {
                                                     wire.kind = WIO_NATIVE_ABI_U64;
                                                     wire.payload.unsignedInteger = scalar;
                                                 }
                                             }
                                             return true;
                                         });
        return valid ? decodeNativeValue(module, wire, type, 1, &layouts, depth + 1)
                     : NativeValueResult{{}, "native POD array has an invalid scalar representation"};
    }
} // namespace wio::vm::detail
