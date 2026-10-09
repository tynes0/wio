#include "native_any_internal.h"
#include "native_sequence_internal.h"

#include <algorithm>

namespace wio::vm::detail
{
    namespace
    {
        struct Wire final
        {
            WioNativeAbiValue value{};
            ~Wire()
            {
                WioNativeAbiReleaseValue(&value);
            }
        };

        bool statusOk(WioNativeAbiStatus status, WioNativeAbiFailure& failure, std::string& error)
        {
            if (status == WIO_NATIVE_ABI_OK)
                return true;
            failure.status = status;
            error = nativeFailureMessage(failure);
            return false;
        }
    } // namespace

    bool mergeNativeAny(const bytecode::Module& module, const WioNativeAnyApi& api, NativePodLayouts& layouts,
                        std::string& error)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_ANY_VERSION || !api.typeId || !api.packNull || !api.isNull ||
            (api.payloadCount && !api.payloads))
            return reject("native any API version, identity, or descriptor array is invalid");
        const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                    { return entry.stableTypeId == api.typeId; });
        if (reflected == module.contract.reflection.end())
            return reject("native any API has no matching VM any identity");
        if (reflected->type >= module.types.size() || module.types[reflected->type].kind != 19)
            return reject("native any API identity does not describe the VM any type");
        auto candidate = layouts;
        auto& target = candidate.any[reflected->type];
        if (target.typeId &&
            (target.typeId != api.typeId || target.packNull != api.packNull || target.isNull != api.isNull))
            return reject("native any is already registered with a different producer adapter");
        target.typeId = api.typeId;
        target.packNull = api.packNull;
        target.isNull = api.isNull;
        for (std::uint32_t index = 0; index < api.payloadCount; ++index)
        {
            const auto& descriptor = api.payloads[index];
            if (!descriptor.typeId || !descriptor.pack || !descriptor.read)
                return reject("native any payload descriptor is incomplete");
            const auto payload = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                      { return entry.stableTypeId == descriptor.typeId; });
            if (payload == module.contract.reflection.end())
                continue;
            if (!supportedNativeContainerValue(module, payload->type, candidate) &&
                !candidate.objectContracts.contains(payload->type))
                return reject("native any payload requires unsupported structural materialization");
            const auto [found, inserted] = target.payloads.emplace(payload->type, descriptor);
            if (!inserted && (found->second.typeId != descriptor.typeId || found->second.pack != descriptor.pack ||
                              found->second.read != descriptor.read))
                return reject("native any payload is already registered with a different adapter");
        }
        layouts = std::move(candidate);
        return true;
    }

    bool packNativeAny(const bytecode::Module& module, const Value& value, std::uint32_t type,
                       const NativePodLayouts& layouts, WioNativeAbiValue& output, std::string& error, unsigned depth,
                       const NativeObjectAccess* objects)
    {
        const auto found = layouts.any.find(type);
        if (found == layouts.any.end() || depth > 64)
        {
            error = "VM any requires a producer adapter and bounded payload depth";
            return false;
        }
        const auto backing = value.nativeAnyBacking();
        if (!backing.isEmpty())
        {
            output.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
            output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            output.typeId = backing.foreignTypeId();
            output.payload.handle = backing.foreignHandle();
            if (!validNativeContainerWire(output, found->second.typeId))
            {
                error = "VM any backing has an invalid producer handle or type identity";
                output = {};
                return false;
            }
            return true;
        }
        Wire payload, packed;
        WioNativeAbiFailure failure{};
        WioNativeAbiStatus status;
        if (value.kind() == Value::Kind::Null)
            status = found->second.packNull(nullptr, &packed.value, &failure);
        else
        {
            const auto descriptor = found->second.payloads.find(value.anyType());
            const auto* source = value.anyPayload();
            if (value.kind() != Value::Kind::Any || !source || descriptor == found->second.payloads.end())
            {
                error = "VM any payload has no supported producer adapter";
                return false;
            }
            std::uint8_t marshalling = 0;
            if (!nativeValueMarshalling(module, value.anyType(), marshalling) ||
                !encodeNativeValue(module, *source, value.anyType(), marshalling, payload.value, error, &layouts,
                                   depth + 1, objects))
                return false;
            status = descriptor->second.pack(&payload.value, &packed.value, &failure);
        }
        if (!statusOk(status, failure, error))
            return false;
        if (!validNativeContainerWire(packed.value, found->second.typeId) ||
            !(packed.value.flags & WIO_NATIVE_ABI_VALUE_OWNED))
        {
            error = "producer any pack returned an invalid or non-owned runtime handle";
            return false;
        }
        if (value.kind() == Value::Kind::Any)
        {
            value.setNativeAnyBacking(Value::runtimeValue(type, found->second.typeId, packed.value.payload.handle));
            const auto installed = value.nativeAnyBacking();
            if (installed.foreignHandle().state != packed.value.payload.handle.state)
            {
                output.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
                output.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                output.typeId = installed.foreignTypeId();
                output.payload.handle = installed.foreignHandle();
                return true;
            }
        }
        output = packed.value;
        packed.value = {};
        return true;
    }

    NativeValueResult unpackNativeAny(const bytecode::Module& module, const WioNativeAbiValue& value,
                                      std::uint32_t type, const NativePodLayouts& layouts, unsigned depth,
                                      const NativeObjectAccess* objects)
    {
        const auto found = layouts.any.find(type);
        if (found == layouts.any.end() || depth > 64 || !validNativeContainerWire(value, found->second.typeId))
            return {{}, "native any result requires a valid producer adapter and bounded payload depth"};
        bool isNull = false;
        WioNativeAbiFailure failure{};
        std::string error;
        if (!statusOk(found->second.isNull(&value, &isNull, &failure), failure, error))
            return {{}, std::move(error)};
        if (isNull)
            return {Value::null(), {}};
        // Preserve the existing native-to-native forwarding contract for
        // payloads the VM cannot inspect. This is a real typed producer handle,
        // not an invented VM payload identity.
        NativeValueResult result{Value::runtimeValue(type, found->second.typeId, value.payload.handle), {}};
        bool selected = false;
        for (const auto& [payloadType, descriptor] : found->second.payloads)
        {
            Wire payload;
            bool matches = false;
            if (!statusOk(descriptor.read(&value, &payload.value, &matches, &failure), failure, error))
                return {{}, std::move(error)};
            if (!matches)
                continue;
            if (selected)
                return {{}, "native any payload matches ambiguous C++ alias identities"};
            std::uint8_t marshalling = 0;
            if (!nativeValueMarshalling(module, payloadType, marshalling))
                return {{}, "native any payload has no native marshalling contract"};
            auto decoded =
                decodeNativeValue(module, payload.value, payloadType, marshalling, &layouts, depth + 1, objects);
            if (!decoded.succeeded())
                return decoded;
            result = {Value::any(payloadType, std::move(decoded.value)), {}};
            result.value.setNativeAnyBacking(Value::runtimeValue(type, found->second.typeId, value.payload.handle));
            selected = true;
        }
        return result;
    }
} // namespace wio::vm::detail
