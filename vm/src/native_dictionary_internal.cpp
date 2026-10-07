#include "native_dictionary_internal.h"
#include "dictionary_keys.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint64_t EntryLimit = 1'000'000;
        struct Wires final
        {
            std::vector<WioNativeAbiValue> entries;
            WioNativeAbiValue result;
            ~Wires()
            {
                for (auto& entry : entries)
                    WioNativeAbiReleaseValue(&entry);
                WioNativeAbiReleaseValue(&result);
            }
        };

        std::uint64_t identity(const bytecode::Module& module, std::uint32_t type)
        {
            for (const auto& reflection : module.contract.reflection)
                if (reflection.type == type)
                    return reflection.stableTypeId;
            return 0;
        }
    } // namespace

    bool supportedNativeDictionaryKey(const bytecode::Module& module, std::uint32_t type)
    {
        if (type >= module.types.size())
            return false;
        const auto& key = module.types[type];
        if (key.kind == 28 && (key.nominalKind == 4 || key.nominalKind == 5))
            return key.enumUnderlyingType < module.types.size() && module.types[key.enumUnderlyingType].kind >= 3 &&
                   module.types[key.enumUnderlyingType].kind <= 12;
        return (key.kind >= 2 && key.kind <= 12) || (key.kind >= 15 && key.kind <= 18);
    }

    bool mergeNativeDictionaries(const bytecode::Module& module, const WioNativeDictionaryApi& api,
                                 NativePodLayouts& layouts, std::string& error, bool validateElements)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_DICTIONARY_VERSION || (api.typeCount && !api.types))
            return reject("native dictionary API version or descriptor array is invalid");
        auto candidate = layouts;
        for (std::uint32_t index = 0; index < api.typeCount; ++index)
        {
            const auto& descriptor = api.types[index];
            if (!descriptor.typeId || !descriptor.keyTypeId || !descriptor.valueTypeId ||
                (descriptor.flags & ~WIO_NATIVE_DICTIONARY_ORDERED) || !descriptor.pack || !descriptor.count ||
                !descriptor.read)
                return reject("native dictionary descriptor is incomplete or has unknown flags");
            const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                        { return entry.stableTypeId == descriptor.typeId; });
            if (reflected == module.contract.reflection.end())
                continue;
            if (reflected->type >= module.types.size())
                return reject("native dictionary descriptor references an invalid VM type");
            const auto& type = module.types[reflected->type];
            if (type.kind != 32 || type.arguments.size() != 2 ||
                (module.string(type.name) == "ordered") != bool(descriptor.flags & WIO_NATIVE_DICTIONARY_ORDERED) ||
                identity(module, type.arguments[0]) != descriptor.keyTypeId ||
                identity(module, type.arguments[1]) != descriptor.valueTypeId)
                return reject("native dictionary descriptor does not match its VM declaration");
            const auto [found, inserted] = candidate.dictionaries.emplace(reflected->type, descriptor);
            if (!inserted &&
                (found->second.typeId != descriptor.typeId || found->second.keyTypeId != descriptor.keyTypeId ||
                 found->second.valueTypeId != descriptor.valueTypeId || found->second.flags != descriptor.flags ||
                 found->second.pack != descriptor.pack || found->second.count != descriptor.count ||
                 found->second.read != descriptor.read))
                return reject("native dictionary type is already registered with a different adapter");
        }
        if (validateElements && !validateNativeContainers(module, candidate, error))
            return false;
        layouts = std::move(candidate);
        return true;
    }

    bool packNativeDictionary(const bytecode::Module& module, const Value& value, std::uint32_t type,
                              const NativePodLayouts& layouts, WioNativeAbiValue& output, std::string& error,
                              unsigned depth)
    {
        const auto adapter = layouts.dictionaries.find(type);
        if (adapter == layouts.dictionaries.end())
        {
            error = "VM dictionary requires a producer native dictionary adapter";
            return false;
        }
        if (value.kind() != Value::Kind::Dictionary || value.aggregateType() != type ||
            value.dictionaryIsOrdered() != bool(adapter->second.flags & WIO_NATIVE_DICTIONARY_ORDERED) ||
            value.dictionaryCount() > EntryLimit || depth > 64)
        {
            error = "VM native dictionary has an invalid kind, type, order, size, or nesting depth";
            return false;
        }
        Wires wires;
        wires.entries.resize(value.dictionaryCount() * 2);
        const auto& arguments = module.types[type].arguments;
        std::uint8_t keyMarshalling = 0, valueMarshalling = 0;
        if (!nativeValueMarshalling(module, arguments[0], keyMarshalling) ||
            !nativeValueMarshalling(module, arguments[1], valueMarshalling))
        {
            error = "VM dictionary key/value has no native marshalling contract";
            return false;
        }
        for (std::size_t index = 0; index < value.dictionaryCount(); ++index)
            if (!encodeNativeValue(module, *value.dictionaryKey(index), arguments[0], keyMarshalling,
                                   wires.entries[index * 2], error, &layouts, depth + 1) ||
                !encodeNativeValue(module, *value.dictionaryValue(index), arguments[1], valueMarshalling,
                                   wires.entries[index * 2 + 1], error, &layouts, depth + 1))
                return false;
        WioNativeAbiFailure failure;
        const auto status =
            adapter->second.pack(wires.entries.data(), value.dictionaryCount(), &wires.result, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            error = nativeFailureMessage(failure);
            return false;
        }
        if (!validNativeContainerWire(wires.result, adapter->second.typeId) ||
            !(wires.result.flags & WIO_NATIVE_ABI_VALUE_OWNED))
        {
            error = "producer dictionary pack returned an invalid or non-owned runtime handle";
            return false;
        }
        output = wires.result;
        wires.result = {};
        return true;
    }

    NativeValueResult unpackNativeDictionary(const bytecode::Module& module, const WioNativeAbiValue& value,
                                             std::uint32_t type, const NativePodLayouts& layouts, unsigned depth)
    {
        const auto adapter = layouts.dictionaries.find(type);
        if (adapter == layouts.dictionaries.end() || depth > 64)
            return {{}, "native dictionary result requires an adapter and bounded nesting depth"};
        if (!validNativeContainerWire(value, adapter->second.typeId))
            return {{}, "native dictionary result has an invalid runtime handle"};
        WioNativeAbiFailure failure;
        std::uint64_t count = 0;
        auto status = adapter->second.count(&value, &count, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            return {{}, nativeFailureMessage(failure)};
        }
        if (count > EntryLimit)
            return {{}, "native dictionary result exceeds the bridge entry limit"};
        Wires wires;
        wires.entries.resize(static_cast<std::size_t>(count) * 2);
        status = adapter->second.read(&value, wires.entries.data(), count, &failure);
        if (status != WIO_NATIVE_ABI_OK)
        {
            failure.status = status;
            return {{}, nativeFailureMessage(failure)};
        }
        const auto& arguments = module.types[type].arguments;
        std::uint8_t keyMarshalling = 0, valueMarshalling = 0;
        if (!nativeValueMarshalling(module, arguments[0], keyMarshalling) ||
            !nativeValueMarshalling(module, arguments[1], valueMarshalling))
            return {{}, "native dictionary result key/value has no native marshalling contract"};
        std::vector<Value> entries;
        entries.reserve(wires.entries.size());
        std::unordered_set<const Value*, DictionaryKeyHash, DictionaryKeyEqual> keys;
        keys.reserve(static_cast<std::size_t>(count));
        for (std::size_t index = 0; index < count; ++index)
        {
            auto key =
                decodeNativeValue(module, wires.entries[index * 2], arguments[0], keyMarshalling, &layouts, depth + 1);
            if (!key.succeeded())
                return key;
            auto mapped = decodeNativeValue(module, wires.entries[index * 2 + 1], arguments[1], valueMarshalling,
                                            &layouts, depth + 1);
            if (!mapped.succeeded())
                return mapped;
            entries.push_back(std::move(key.value));
            entries.push_back(std::move(mapped.value));
            if (!keys.insert(&entries[entries.size() - 2]).second)
                return {{}, "native dictionary result contains duplicate keys"};
        }
        return {Value::dictionary(type, std::move(entries), adapter->second.flags & WIO_NATIVE_DICTIONARY_ORDERED), {}};
    }

    bool nativeDictionaryCopyBackOrder(const Value& target, const Value& replacement, std::vector<std::size_t>& order)
    {
        if (target.kind() != Value::Kind::Dictionary || replacement.kind() != Value::Kind::Dictionary ||
            target.aggregateType() != replacement.aggregateType() ||
            target.dictionaryIsOrdered() != replacement.dictionaryIsOrdered() ||
            target.dictionaryCount() != replacement.dictionaryCount())
            return false;
        std::unordered_map<const Value*, std::size_t, DictionaryKeyHash, DictionaryKeyEqual> incoming;
        incoming.reserve(replacement.dictionaryCount());
        for (std::size_t index = 0; index < replacement.dictionaryCount(); ++index)
            if (!DictionaryKeyHash::supported(*replacement.dictionaryKey(index)) ||
                !incoming.emplace(replacement.dictionaryKey(index), index).second)
                return false;
        std::vector<std::size_t> candidate;
        candidate.reserve(target.dictionaryCount());
        for (std::size_t index = 0; index < target.dictionaryCount(); ++index)
        {
            const auto found = incoming.find(target.dictionaryKey(index));
            if (found == incoming.end())
                return false;
            candidate.push_back(found->second);
        }
        order = std::move(candidate);
        return true;
    }
} // namespace wio::vm::detail
