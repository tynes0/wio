#include "native_object_internal.h"

#include <algorithm>

namespace wio::vm::detail
{
    bool mergeNativeObjects(const bytecode::Module& module, const WioNativeObjectApi& api, NativePodLayouts& layouts,
                            std::string& error)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_OBJECT_VERSION || (api.typeCount && !api.types))
            return reject("native object API version or descriptor array is invalid");
        auto candidate = layouts;
        for (std::uint32_t index = 0; index < api.typeCount; ++index)
        {
            const auto& descriptor = api.types[index];
            constexpr auto Flags = WIO_NATIVE_OBJECT_HOST_PROXY | WIO_NATIVE_OBJECT_WEAK_CACHE;
            if (!descriptor.typeId || descriptor.flags != Flags || !descriptor.create || !descriptor.host ||
                !descriptor.weak || !descriptor.lock)
                return reject("native object descriptor is incomplete or has incompatible capabilities");
            const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                        { return entry.stableTypeId == descriptor.typeId; });
            if (reflected == module.contract.reflection.end())
                continue;
            if (reflected->type >= module.types.size())
                return reject("native object descriptor references an invalid VM type");
            const auto& type = module.types[reflected->type];
            if (type.kind != 28 || type.nominalKind != 2)
                return reject("native object descriptor does not describe a concrete VM object");
            const auto [found, inserted] = candidate.objects.emplace(reflected->type, descriptor);
            const auto& previous = found->second;
            if (!inserted && (previous.typeId != descriptor.typeId || previous.flags != descriptor.flags ||
                              previous.create != descriptor.create || previous.host != descriptor.host ||
                              previous.weak != descriptor.weak || previous.lock != descriptor.lock))
                return reject("native object type is already registered with a different producer adapter");
        }
        layouts = std::move(candidate);
        return true;
    }
} // namespace wio::vm::detail
