#include "native_sequence_internal.h"
#include "native_dictionary_internal.h"

#include <algorithm>
#include <limits>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint64_t ElementLimit = 1'000'000;

        struct WireGuard final
        {
            WioNativeAbiValue value;
            ~WireGuard()
            {
                WioNativeAbiReleaseValue(&value);
            }
        };

        struct ElementWires final
        {
            std::vector<WioNativeAbiValue> values;
            ~ElementWires()
            {
                for (auto& value : values)
                    WioNativeAbiReleaseValue(&value);
            }
        };

        std::uint64_t identity(const bytecode::Module& module, std::uint32_t type)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == type)
                    return reflected.stableTypeId;
            return 0;
        }

    } // namespace

    bool supportedNativeContainerValue(const bytecode::Module& module, std::uint32_t type,
                                       const NativePodLayouts& layouts)
    {
        if (type >= module.types.size())
            return false;
        const auto& definition = module.types[type];
        if (definition.kind == 30)
            return nativePodPointerType(module, type) != bytecode::InvalidIndex ||
                   (definition.arguments.size() == 1 && layouts.objectContracts.contains(definition.arguments.front()));
        std::uint8_t marshalling = 0;
        if (!nativeValueMarshalling(module, type, marshalling))
            return false;
        if (marshalling == 4)
            return layouts.types.contains(type);
        if (marshalling == 6)
            return layouts.objectContracts.contains(type);
        if (definition.kind == 19)
            return layouts.any.contains(type);
        return (marshalling >= 1 && marshalling <= 5) || (definition.kind == 31 && layouts.sequences.contains(type)) ||
               (definition.kind == 32 && layouts.dictionaries.contains(type));
    }

    bool validNativeContainerWire(const WioNativeAbiValue& value, std::uint64_t typeId)
    {
        const auto& handle = value.payload.handle;
        return value.kind == WIO_NATIVE_ABI_RUNTIME_VALUE && value.typeId == typeId &&
               (value.flags & (WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_BORROWED)) && handle.state &&
               handle.ops && handle.ops->abiVersion == WIO_NATIVE_ABI_VERSION && handle.ops->retain &&
               handle.ops->release && handle.ops->typeId && handle.ops->typeId(handle.state) == typeId;
    }
    bool validateNativeContainers(const bytecode::Module& module, const NativePodLayouts& layouts, std::string& error)
    {
        for (const auto& [type, descriptor] : layouts.sequences)
            if (!supportedNativeContainerValue(module, module.types[type].arguments.front(), layouts))
            {
                error = "native sequence element requires unsupported structural materialization";
                return false;
            }
        for (const auto& [type, descriptor] : layouts.dictionaries)
            if (!supportedNativeDictionaryKey(module, module.types[type].arguments.front()) ||
                !supportedNativeContainerValue(module, module.types[type].arguments.back(), layouts))
            {
                error = "native dictionary key/value requires unsupported structural materialization";
                return false;
            }
        return true;
    }

    bool mergeNativeSequences(const bytecode::Module& module, const WioNativeSequenceApi& api,
                              NativePodLayouts& layouts, std::string& error, bool validateElements)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_SEQUENCE_VERSION || (api.typeCount && !api.types))
            return reject("native sequence API version or descriptor array is invalid");
        auto candidate = layouts;
        for (std::uint32_t index = 0; index < api.typeCount; ++index)
        {
            const auto& descriptor = api.types[index];
            if (!descriptor.typeId || !descriptor.elementTypeId || !descriptor.pack || !descriptor.count ||
                !descriptor.read)
                return reject("native sequence descriptor is incomplete");
            const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                        { return entry.stableTypeId == descriptor.typeId; });
            if (reflected == module.contract.reflection.end())
                continue;
            if (reflected->type >= module.types.size())
                return reject("native sequence descriptor references an invalid VM type");
            const auto& type = module.types[reflected->type];
            if (type.kind != 31 || type.arguments.size() != 1 ||
                type.staticExtent != (std::numeric_limits<std::uint64_t>::max)() ||
                type.extentParameter != bytecode::InvalidIndex ||
                identity(module, type.arguments.front()) != descriptor.elementTypeId)
                return reject("native sequence descriptor does not match its VM array declaration");
            const auto [found, inserted] = candidate.sequences.emplace(reflected->type, descriptor);
            if (!inserted &&
                (found->second.typeId != descriptor.typeId || found->second.elementTypeId != descriptor.elementTypeId ||
                 found->second.pack != descriptor.pack || found->second.count != descriptor.count ||
                 found->second.read != descriptor.read))
                return reject("native sequence type is already registered with a different adapter");
        }
        if (validateElements && !validateNativeContainers(module, candidate, error))
            return false;
        layouts = std::move(candidate);
        return true;
    }

    bool packNativeSequence(const bytecode::Module& module, const Value& value, std::uint32_t type,
                            const NativePodLayouts& layouts, WioNativeAbiValue& output, std::string& error,
                            unsigned depth, const NativeObjectAccess* objects)
    {
        const auto adapter = layouts.sequences.find(type);
        if (adapter == layouts.sequences.end())
        {
            error = "VM array requires a producer native sequence adapter";
            return false;
        }
        if (value.kind() != Value::Kind::Array || value.elementCount() > ElementLimit || depth > 64)
        {
            error = "VM native sequence has an invalid value kind, size, or nesting depth";
            return false;
        }
        const auto elementType = module.types[type].arguments.front();
        std::uint8_t marshalling = 0;
        if (!nativeValueMarshalling(module, elementType, marshalling))
        {
            error = "VM native sequence element has no native marshalling contract";
            return false;
        }
        ElementWires elements;
        elements.values.resize(value.elementCount());
        for (std::size_t index = 0; index < value.elementCount(); ++index)
            if (!encodeNativeValue(module, *value.element(index), elementType, marshalling, elements.values[index],
                                   error, &layouts, depth + 1, objects))
                return false;
        WireGuard result;
        WioNativeAbiFailure failure;
        const auto status =
            adapter->second.pack(elements.values.data(), elements.values.size(), &result.value, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            error = nativeFailureMessage(failure);
            return false;
        }
        if (!validNativeContainerWire(result.value, adapter->second.typeId) ||
            !(result.value.flags & WIO_NATIVE_ABI_VALUE_OWNED))
        {
            error = "producer sequence pack returned an invalid or non-owned runtime handle";
            return false;
        }
        output = result.value;
        result.value = {};
        return true;
    }

    NativeValueResult unpackNativeSequence(const bytecode::Module& module, const WioNativeAbiValue& value,
                                           std::uint32_t type, const NativePodLayouts& layouts, unsigned depth,
                                           const NativeObjectAccess* objects)
    {
        const auto adapter = layouts.sequences.find(type);
        if (adapter == layouts.sequences.end() || depth > 64)
            return {{}, "native sequence result requires an adapter and bounded nesting depth"};
        if (!validNativeContainerWire(value, adapter->second.typeId))
            return {{}, "native sequence result has an invalid runtime handle"};
        std::uint64_t count = 0;
        WioNativeAbiFailure failure;
        auto status = adapter->second.count(&value, &count, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            return {{}, nativeFailureMessage(failure)};
        }
        if (count > ElementLimit)
            return {{}, "native sequence result exceeds the bridge element limit"};
        const auto elementType = module.types[type].arguments.front();
        std::uint8_t marshalling = 0;
        if (!nativeValueMarshalling(module, elementType, marshalling))
            return {{}, "native sequence result element has no native marshalling contract"};
        std::vector<Value> elements;
        elements.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t index = 0; index < count; ++index)
        {
            WireGuard element;
            status = adapter->second.read(&value, index, &element.value, &failure);
            if (status != WIO_NATIVE_ABI_OK)
            {
                failure.status = status;
                return {{}, nativeFailureMessage(failure)};
            }
            auto decoded =
                decodeNativeValue(module, element.value, elementType, marshalling, &layouts, depth + 1, objects);
            if (!decoded.succeeded())
                return decoded;
            elements.push_back(std::move(decoded.value));
        }
        return {Value::array(std::move(elements)), {}};
    }

    bool containsNativeSequencePlace(const Value& root, const Value& target, unsigned depth)
    {
        if (depth > 64)
            return false;
        if (root.kind() == Value::Kind::Dictionary)
        {
            for (std::size_t index = 0; index < root.dictionaryCount(); ++index)
            {
                const auto* key = root.dictionaryKey(index);
                const auto* cell = root.dictionaryValue(index);
                if (key == &target || cell == &target || containsNativeSequencePlace(*cell, target, depth + 1))
                    return true;
            }
            return false;
        }
        const bool array = root.kind() == Value::Kind::Array;
        const auto count = array ? root.elementCount() : root.fieldCount();
        for (std::size_t index = 0; index < count; ++index)
        {
            const auto* cell = array ? root.element(index) : root.field(index);
            if (cell && (cell == &target || containsNativeSequencePlace(*cell, target, depth + 1)))
                return true;
        }
        return false;
    }

    bool compatibleNativeSequenceCopyBack(const Value& target, const Value& replacement, unsigned depth)
    {
        if (depth > 64)
            return false;
        if (target.kind() == Value::Kind::Dictionary)
        {
            std::vector<std::size_t> order;
            if (!nativeDictionaryCopyBackOrder(target, replacement, order))
                return false;
            for (std::size_t index = 0; index < order.size(); ++index)
                if (!compatibleNativeSequenceCopyBack(*target.dictionaryValue(index),
                                                      *replacement.dictionaryValue(order[index]), depth + 1))
                    return false;
            return true;
        }
        if (target.kind() != Value::Kind::Array)
            return true;
        if (replacement.kind() != Value::Kind::Array || target.elementCount() != replacement.elementCount())
            return false;
        for (std::size_t index = 0; index < target.elementCount(); ++index)
            if (!compatibleNativeSequenceCopyBack(*target.element(index), *replacement.element(index), depth + 1))
                return false;
        return true;
    }
} // namespace wio::vm::detail
