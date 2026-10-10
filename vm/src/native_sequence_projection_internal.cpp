#include "native_sequence_projection_internal.h"

namespace wio::vm::detail
{
    namespace
    {
        bool locate(const bytecode::Module& module, const Value& root, std::uint32_t type, const Value& target,
                    const NativePodLayouts& layouts, NativeSequenceProjection& output, unsigned depth)
        {
            if (depth > 64 || type >= module.types.size())
                return false;
            if (&root == &target && !output.indices.empty())
            {
                if (module.types[type].kind == 2) // vector<bool> proxy is not a bool reference.
                    return false;
                for (const auto& reflection : module.contract.reflection)
                    if (reflection.type == type && reflection.stableTypeId)
                    {
                        output.type = type;
                        output.typeId = reflection.stableTypeId;
                        return true;
                    }
                return false;
            }
            if (!output.indices.empty() && layouts.types.contains(type))
            {
                NativePodProjection field;
                if (!findNativePodProjection(module, root, type, target, layouts, field, depth) || field.depth > 64)
                    return false;
                output.type = field.type;
                output.typeId = field.typeId;
                output.podTypeId = layouts.types.at(type).typeId;
                output.offset = field.offset;
                output.size = field.size;
                return true;
            }
            if (root.kind() != Value::Kind::Array || !layouts.sequences.contains(type) ||
                module.types[type].kind != 31 || module.types[type].arguments.size() != 1)
                return false;
            const auto elementType = module.types[type].arguments.front();
            for (std::size_t index = 0; index < root.elementCount(); ++index)
            {
                output.indices.push_back(index);
                if (locate(module, *root.element(index), elementType, target, layouts, output, depth + 1))
                    return true;
                output.indices.pop_back();
            }
            return false;
        }
    } // namespace

    bool findNativeSequenceProjection(const bytecode::Module& module, const Value& root, std::uint32_t rootType,
                                      const Value& target, const NativePodLayouts& layouts,
                                      NativeSequenceProjection& output)
    {
        NativeSequenceProjection candidate;
        if (!locate(module, root, rootType, target, layouts, candidate, 0))
            return false;
        output = std::move(candidate);
        return true;
    }
} // namespace wio::vm::detail
