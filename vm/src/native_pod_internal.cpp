#include "native_pod_internal.h"

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
    } // namespace

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
                if (!nativeValueMarshalling(module, field.type, marshalling) || (marshalling != 1 && marshalling != 4))
                    return reject("native POD field requires unsupported VM materialization");
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
        layouts = std::move(candidate);
        error.clear();
        return true;
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
        for (std::size_t index = 0; index < fields.values.size(); ++index)
        {
            const auto fieldType = module.types[type].fields[index].type;
            std::uint8_t marshalling = 0;
            if (!nativeValueMarshalling(module, fieldType, marshalling) ||
                !encodeNativeValue(module, *value.field(index), fieldType, marshalling, fields.values[index], error,
                                   &layouts, depth + 1))
                return false;
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
            std::uint8_t marshalling = 0;
            if (!nativeValueMarshalling(module, fieldType, marshalling))
                return {{}, "native POD field marshalling is unavailable"};
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
