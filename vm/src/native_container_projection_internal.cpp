#include "native_container_projection_internal.h"

namespace wio::vm::detail
{
    namespace
    {
        std::uint64_t identity(const bytecode::Module& module, std::uint32_t type)
        {
            for (const auto& reflected : module.contract.reflection)
                if (reflected.type == type)
                    return reflected.stableTypeId;
            return 0;
        }

        bool locate(const bytecode::Module& module, const Value& root, std::uint32_t type, const Value& target,
                    const NativePodLayouts& layouts, NativeContainerProjection& path, unsigned depth)
        {
            if (depth > 64 || type >= module.types.size())
                return false;
            if (&root == &target && !path.steps.empty())
            {
                path.type = type;
                path.typeId = identity(module, type);
                return path.typeId != 0;
            }
            if (!path.steps.empty() && layouts.types.contains(type))
            {
                NativePodProjection field;
                if (!findNativePodProjection(module, root, type, target, layouts, field, depth) || field.depth > 64)
                    return false;
                path.type = field.type;
                path.typeId = field.typeId;
                path.podTypeId = layouts.types.at(type).typeId;
                path.offset = field.offset;
                path.size = field.size;
                return true;
            }
            const auto& declaration = module.types[type];
            if (root.kind() == Value::Kind::Array && declaration.kind == 31 && declaration.arguments.size() == 1 &&
                layouts.sequences.contains(type))
            {
                const auto childType = declaration.arguments.front();
                if (childType >= module.types.size() || module.types[childType].kind == 2)
                    return false; // vector<bool> elements have proxy storage, not a bool address.
                for (std::size_t index = 0; index < root.elementCount(); ++index)
                {
                    path.steps.push_back({WIO_NATIVE_ABI_CONTAINER_INDEX, identity(module, type), index});
                    if (locate(module, *root.element(index), childType, target, layouts, path, depth + 1))
                        return true;
                    path.steps.pop_back();
                }
            }
            if (root.kind() == Value::Kind::Dictionary && declaration.kind == 32 && declaration.arguments.size() == 2 &&
                layouts.dictionaries.contains(type))
            {
                for (std::size_t index = 0; index < root.dictionaryCount(); ++index)
                {
                    path.steps.push_back({WIO_NATIVE_ABI_CONTAINER_VALUE, identity(module, type), 0,
                                          declaration.arguments[0], root.dictionaryKey(index)});
                    if (locate(module, *root.dictionaryValue(index), declaration.arguments[1], target, layouts, path,
                               depth + 1))
                        return true;
                    path.steps.pop_back();
                }
            }
            return false;
        }
    } // namespace

    bool findNativeContainerProjection(const bytecode::Module& module, const Value& root, std::uint32_t rootType,
                                       const Value& target, const NativePodLayouts& layouts,
                                       NativeContainerProjection& output)
    {
        NativeContainerProjection candidate;
        if (!locate(module, root, rootType, target, layouts, candidate, 0))
            return false;
        output = std::move(candidate);
        return true;
    }
} // namespace wio::vm::detail
