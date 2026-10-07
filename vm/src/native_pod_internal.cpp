#include "native_pod_internal.h"
#include "native_pod_array_codec.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t TypeNamed = 28;
        constexpr std::uint8_t NominalComponent = 1;
        constexpr std::uint8_t RepresentationNativePod = 1;
        struct WireGuard
        {
            WioNativeAbiValue value;
            ~WireGuard()
            {
                WioNativeAbiReleaseValue(&value);
            }
        };

        struct FieldWires
        {
            std::vector<WioNativeAbiValue> values;
            std::vector<std::vector<std::byte>> arrays;
            ~FieldWires()
            {
                for (auto& value : values)
                    WioNativeAbiReleaseValue(&value);
            }
        };

        std::uint64_t identity(const bytecode::Module& module, const std::uint32_t type)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == type)
                    return reflected.stableTypeId;
            return 0;
        }

        bool concreteArgument(const bytecode::Module& module, std::uint32_t type, unsigned depth = 0)
        {
            if (type >= module.types.size() || depth > 64)
                return false;
            const auto& definition = module.types[type];
            if (definition.kind == 0 || definition.kind == 21 || definition.kind == 22 || definition.kind == 24 ||
                definition.kind == 25 || definition.kind == 26 || definition.extentParameter != bytecode::InvalidIndex)
                return false;
            return std::ranges::all_of(definition.arguments,
                                       [&](auto argument) { return concreteArgument(module, argument, depth + 1); });
        }
    } // namespace

    std::uint32_t nativePodPointerType(const bytecode::Module& module, std::uint32_t type) noexcept
    {
        if (type < module.types.size() && module.types[type].kind == 30)
        {
            const auto& arguments = module.types[type].arguments;
            if (arguments.size() != 1)
                return bytecode::InvalidIndex;
            type = arguments.front();
        }
        return type < module.types.size() && module.types[type].kind == 20 ? type : bytecode::InvalidIndex;
    }

    bool mergeNativePodLayouts(const bytecode::Module& module, const WioNativePodLayoutApi& api,
                               NativePodLayouts& layouts, std::string& error)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_POD_LAYOUT_VERSION || (api.typeCount && !api.types))
            return reject("native POD layout API version or descriptor array is invalid");
        NativePodLayouts candidate = layouts;
        for (std::uint32_t index = 0; index < api.typeCount; ++index)
        {
            const auto& descriptor = api.types[index];
            if (!descriptor.typeId || !descriptor.valueSize || !descriptor.valueAlignment ||
                (descriptor.valueAlignment & (descriptor.valueAlignment - 1)) ||
                descriptor.valueSize % descriptor.valueAlignment ||
                descriptor.valueSize > (std::numeric_limits<std::size_t>::max)() || !descriptor.pack ||
                !descriptor.read || (descriptor.fieldCount && !descriptor.fields))
                return reject("native POD layout descriptor is incomplete or has invalid size/alignment");
            const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                        { return entry.stableTypeId == descriptor.typeId; });
            // A shared producer can contain types unused by this bytecode module.
            if (reflected == module.contract.reflection.end())
                continue;
            if (reflected->type >= module.types.size())
                return reject("native POD layout references an invalid VM type");
            const auto& type = module.types[reflected->type];
            if (type.kind != TypeNamed || type.nominalKind != NominalComponent ||
                type.nominalRepresentation != RepresentationNativePod || !type.hasNativeBinding ||
                descriptor.fieldCount != type.fields.size())
                return reject("native POD layout does not match a native component declaration");
            if (!std::ranges::all_of(type.arguments, [&](auto argument) { return concreteArgument(module, argument); }))
                return reject("native POD layout requires concrete template arguments");
            for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
            {
                const auto& field = type.fields[fieldIndex];
                const auto& native = descriptor.fields[fieldIndex];
                if (!native.name || module.string(field.name) != native.name || field.type >= module.types.size() ||
                    module.types[field.type].kind != native.typeKind || !native.typeId || !native.size ||
                    native.offset > descriptor.valueSize || native.size > descriptor.valueSize - native.offset)
                    return reject("native POD field name/type/layout does not match its VM declaration");
                const auto stableId = identity(module, field.type);
                if (stableId && stableId != native.typeId)
                    return reject("native POD field stable type identity does not match its VM declaration");
                std::uint8_t marshalling = 0;
                const bool pointer = nativePodPointerType(module, field.type) != bytecode::InvalidIndex;
                if (module.types[field.type].kind != 31 && (!nativeValueMarshalling(module, field.type, marshalling) ||
                                                            (marshalling != 1 && marshalling != 4 && !pointer)))
                    return reject("native POD field requires unsupported VM materialization");
                if (module.types[field.type].kind == 30 && !pointer)
                    return reject("native POD nullable fields must be opaque pointers");
                if (pointer && native.size != sizeof(void*))
                    return reject("native POD opaque field does not have pointer-sized storage");
                for (std::size_t previous = 0; previous < fieldIndex; ++previous)
                {
                    const auto& other = descriptor.fields[previous];
                    if (native.offset < other.offset + other.size && other.offset < native.offset + native.size)
                        return reject("native POD field layout contains overlapping fields");
                }
            }
            NativePodLayout layout{descriptor.typeId, descriptor.valueSize, descriptor.valueAlignment,
                                   descriptor.pack,   descriptor.read,      {}};
            layout.fields.reserve(descriptor.fieldCount);
            for (std::uint32_t fieldIndex = 0; fieldIndex < descriptor.fieldCount; ++fieldIndex)
            {
                const auto& field = descriptor.fields[fieldIndex];
                layout.fields.push_back({field.typeId, field.offset, field.size, field.typeKind});
            }
            const auto [existing, inserted] = candidate.types.emplace(reflected->type, layout);
            if (!inserted && existing->second != layout)
                return reject("native POD type is already registered with a different layout/adapter");
        }
        // Nested element layouts may appear later in the producer descriptor
        // table. Validate fixed-array byte extents against the complete snapshot.
        for (const auto& [type, layout] : candidate.types)
            for (std::size_t index = 0; index < layout.fields.size(); ++index)
            {
                const auto fieldType = module.types[type].fields[index].type;
                if (module.types[fieldType].kind != 31)
                    continue;
                if (!NativePodArrayCodec::validateByteSize(module, fieldType, candidate,
                                                           static_cast<std::size_t>(layout.fields[index].size), error))
                    return reject("native POD fixed-array field has an unsupported or mismatched byte layout");
            }
        layouts = std::move(candidate);
        error.clear();
        return true;
    }

    bool mergeNativePodDefaults(const bytecode::Module& module, const WioNativePodDefaultApi& api,
                                NativePodLayouts& layouts, std::string& error)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_POD_DEFAULT_VERSION || (api.typeCount && !api.types))
            return reject("native POD default API version or descriptor array is invalid");
        auto candidate = layouts.defaults;
        for (std::uint32_t index = 0; index < api.typeCount; ++index)
        {
            const auto& descriptor = api.types[index];
            if (!descriptor.typeId || !descriptor.create)
                return reject("native POD default descriptor requires a type identity and factory");
            const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                        { return entry.stableTypeId == descriptor.typeId; });
            if (reflected == module.contract.reflection.end())
                continue;
            const auto layout = layouts.types.find(reflected->type);
            if (layout == layouts.types.end() || layout->second.typeId != descriptor.typeId)
                return reject("native POD default factory has no matching registered layout");
            const auto [found, inserted] = candidate.emplace(reflected->type, descriptor.create);
            if (!inserted && found->second != descriptor.create)
                return reject("native POD default factory is already registered with a different adapter");
        }
        layouts.defaults = std::move(candidate);
        error.clear();
        return true;
    }

    NativeValueResult defaultNativePod(const bytecode::Module& module, std::uint32_t type,
                                       const NativePodLayouts& layouts)
    {
        const auto factory = layouts.defaults.find(type);
        const auto layout = layouts.types.find(type);
        if (factory == layouts.defaults.end() || layout == layouts.types.end())
            return {{}, "native POD default construction requires a registered producer default factory"};
        WireGuard result{};
        WioNativeAbiFailure failure{};
        const auto status = factory->second(&result.value, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            return {{}, "native POD default construction failed: " + nativeFailureMessage(failure)};
        }
        const auto& wire = result.value;
        if (wire.kind != WIO_NATIVE_ABI_POD || wire.typeId != layout->second.typeId ||
            wire.payload.slice.size != layout->second.size || !wire.payload.slice.data ||
            !(wire.flags & WIO_NATIVE_ABI_VALUE_OWNED) || (wire.flags & WIO_NATIVE_ABI_VALUE_BORROWED) ||
            !wire.owner.state || !wire.owner.ops || wire.owner.ops->abiVersion != WIO_NATIVE_ABI_VERSION ||
            !wire.owner.ops->retain || !wire.owner.ops->release || !wire.owner.ops->typeId ||
            wire.owner.ops->typeId(wire.owner.state) != wire.typeId)
            return {{}, "native POD default factory returned an invalid owned wire value"};
        return unpackNativePod(module, wire, type, layouts, 0);
    }

    bool packNativePod(const bytecode::Module& module, const Value& value, const std::uint32_t type,
                       const NativePodLayouts& layouts, WioNativeAbiValue& output, std::string& error,
                       const unsigned depth)
    {
        const auto found = layouts.types.find(type);
        if (found == layouts.types.end() || value.kind() != Value::Kind::Component || value.aggregateType() != type ||
            value.fieldCount() != module.types[type].fields.size())
        {
            error = "VM component has no compatible native POD layout adapter";
            return false;
        }
        FieldWires fields;
        fields.values.resize(value.fieldCount());
        fields.arrays.resize(value.fieldCount());
        for (std::size_t index = 0; index < fields.values.size(); ++index)
        {
            const auto fieldType = module.types[type].fields[index].type;
            if (module.types[fieldType].kind == 31)
            {
                auto& bytes = fields.arrays[index];
                bytes.resize(static_cast<std::size_t>(found->second.fields[index].size));
                if (!NativePodArrayCodec::pack(module, *value.field(index), fieldType, layouts, bytes, error,
                                               depth + 1))
                    return false;
                auto& wire = fields.values[index];
                wire.kind = WIO_NATIVE_ABI_POD;
                wire.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                wire.typeId = found->second.fields[index].typeId;
                wire.payload.slice = {bytes.data(), bytes.size()};
                continue;
            }
            std::uint8_t marshalling = 0;
            if (!nativeValueMarshalling(module, fieldType, marshalling) ||
                !encodeNativeValue(module, *value.field(index), fieldType, marshalling, fields.values[index], error,
                                   &layouts, depth + 1))
                return false;
            if (nativePodPointerType(module, fieldType) != bytecode::InvalidIndex &&
                fields.values[index].kind == WIO_NATIVE_ABI_OPAQUE)
                fields.values[index].typeId = found->second.fields[index].typeId;
        }
        WireGuard packed;
        const Value backing = value.nativePodBacking();
        WioNativeAbiValue prototype;
        if (!backing.isEmpty() && !encodeNativeValue(module, backing, type, 4, prototype, error, &layouts, depth + 1))
            return false;
        WioNativeAbiFailure failure{};
        const auto status = found->second.pack(backing.isEmpty() ? nullptr : &prototype, fields.values.data(),
                                               fields.values.size(), &packed.value, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            error = "native POD packing failed: " + nativeFailureMessage(failure);
            return false;
        }
        const auto& wire = packed.value;
        if (wire.kind != WIO_NATIVE_ABI_POD || wire.typeId != found->second.typeId ||
            wire.payload.slice.size != found->second.size || !wire.payload.slice.data ||
            !(wire.flags & WIO_NATIVE_ABI_VALUE_OWNED) || !wire.owner.state || !wire.owner.ops ||
            wire.owner.ops->abiVersion != WIO_NATIVE_ABI_VERSION || !wire.owner.ops->retain ||
            !wire.owner.ops->release || !wire.owner.ops->typeId ||
            wire.owner.ops->typeId(wire.owner.state) != wire.typeId)
        {
            error = "native POD pack adapter returned an invalid owned wire value";
            return false;
        }
        output = packed.value;
        packed.value = {};
        return true;
    }

    NativeValueResult unpackNativePod(const bytecode::Module& module, const WioNativeAbiValue& value,
                                      const std::uint32_t type, const NativePodLayouts& layouts, const unsigned depth)
    {
        const auto found = layouts.types.find(type);
        if (found == layouts.types.end() || value.payload.slice.size != found->second.size)
            return {{}, "native POD result byte size does not match the registered layout"};
        std::vector<Value> fields;
        fields.reserve(module.types[type].fields.size());
        for (std::size_t index = 0; index < module.types[type].fields.size(); ++index)
        {
            WireGuard field;
            WioNativeAbiFailure failure{};
            const auto status = found->second.read(&value, index, &field.value, &failure);
            if (status != WIO_NATIVE_ABI_OK)
            {
                failure.status = status;
                return {{}, "native POD field read failed: " + nativeFailureMessage(failure)};
            }
            const auto fieldType = module.types[type].fields[index].type;
            if (module.types[fieldType].kind == 31)
            {
                if (field.value.kind != WIO_NATIVE_ABI_POD ||
                    field.value.typeId != found->second.fields[index].typeId ||
                    field.value.payload.slice.size != found->second.fields[index].size ||
                    !field.value.payload.slice.data)
                    return {{}, "native POD array field adapter returned an invalid wire shape"};
                auto decoded =
                    NativePodArrayCodec::unpack(module,
                                                {static_cast<const std::byte*>(field.value.payload.slice.data),
                                                 static_cast<std::size_t>(field.value.payload.slice.size)},
                                                fieldType, layouts, depth + 1);
                if (!decoded.succeeded())
                    return decoded;
                fields.push_back(std::move(decoded.value));
                continue;
            }
            std::uint8_t marshalling = 0;
            if (!nativeValueMarshalling(module, fieldType, marshalling))
                return {{}, "native POD field marshalling is unavailable"};
            if (nativePodPointerType(module, fieldType) != bytecode::InvalidIndex)
            {
                const bool nullableNull = module.types[fieldType].kind == 30 &&
                                          field.value.kind == WIO_NATIVE_ABI_VOID &&
                                          (field.value.flags & WIO_NATIVE_ABI_VALUE_NULLABLE);
                if (!nullableNull && (field.value.kind != WIO_NATIVE_ABI_OPAQUE ||
                                      field.value.typeId != found->second.fields[index].typeId))
                    return {{}, "native POD opaque field adapter returned an invalid wire identity/shape"};
                // The sidecar identifies the declared field (opaque or opaque?),
                // while the general bridge identifies the unwrapped payload type.
                field.value.typeId = identity(module, nativePodPointerType(module, fieldType));
            }
            auto decoded = decodeNativeValue(module, field.value, fieldType, marshalling, &layouts, depth + 1);
            if (!decoded.succeeded())
                return decoded;
            fields.push_back(std::move(decoded.value));
        }
        Value decoded = Value::component(type, std::move(fields));
        decoded.setNativePodBacking(Value::nativePod(type, found->second.typeId,
                                                     {static_cast<const std::byte*>(value.payload.slice.data),
                                                      static_cast<std::size_t>(value.payload.slice.size)}));
        return {std::move(decoded), {}};
    }
} // namespace wio::vm::detail
