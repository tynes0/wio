#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <sstream>
#include <vector>

namespace wio::codegen
{
    namespace
    {
        using namespace wir;

        const lowered::Function* function(const lowered::Module& module, FunctionId id)
        {
            const auto found = std::ranges::find(module.functions, id, &lowered::Function::id);
            return found == module.functions.end() ? nullptr : &*found;
        }

        const ReflectionDescriptor* reflection(const lowered::Module& module, TypeId id)
        {
            const auto found = std::ranges::find(module.contract.reflection, id, &ReflectionDescriptor::type);
            return found == module.contract.reflection.end() ? nullptr : &*found;
        }

        std::uint64_t methodIdentity(const ReflectionDescriptor& type, FunctionId id)
        {
            const auto found = std::ranges::find(type.methods, id, &ReflectedMethodDescriptor::function);
            return found == type.methods.end() ? 0 : found->stableId;
        }

        std::uint64_t identity(const lowered::Module& module, TypeId id)
        {
            if (const auto object = nativeAbiObjectType(module.types, id))
                id = object;
            if (const auto* reflected = reflection(module, id))
                return reflected->stableTypeId;
            return stableModuleHash(nativeAbiTypeKey(module.types, id));
        }

        bool wireValue(const lowered::Module& module, TypeId id, bool allowVoid = false)
        {
            const auto* type = module.types.tryGet(id);
            return type && ((allowVoid && type->kind == TypeKind::Void) ||
                            (type->kind >= TypeKind::Bool && type->kind <= TypeKind::Text));
        }

        // A returned borrow must name receiver/argument storage that survives
        // dispatch. Never advertise a global, projection, local or opaque call
        // result merely because its signature looks like a supported object.
        bool borrowedResultProven(const lowered::Module& module, const lowered::Function& method)
        {
            const auto& result = module.types.get(method.returnType);
            if (result.kind != TypeKind::Reference)
                return true;
            if (result.arguments.size() != 1)
                return false;
            std::function<bool(ValueId, std::set<ValueId>&)> source;
            source = [&](ValueId value, std::set<ValueId>& visiting)
            {
                if (!value || visiting.size() >= 64 || !visiting.insert(value).second)
                    return false;
                for (const auto& parameter : method.parameters)
                    if (parameter.id == value)
                    {
                        const auto& type = module.types.get(parameter.type);
                        return type.kind == TypeKind::Reference && type.arguments == result.arguments &&
                               (!result.isMutable || type.isMutable);
                    }
                for (const auto& block : method.blocks)
                {
                    for (const auto& instruction : block.instructions)
                        if (instruction.result == value)
                        {
                            if (instruction.opcode != lowered::Opcode::Borrow || instruction.operands.size() != 1)
                                return false;
                            const auto& type = module.types.get(instruction.resultType);
                            return type.kind == TypeKind::Reference && type.arguments == result.arguments &&
                                   (!result.isMutable || type.isMutable) &&
                                   source(instruction.operands.front(), visiting);
                        }
                    for (std::size_t index = 0; index < block.parameters.size(); ++index)
                        if (block.parameters[index].id == value)
                        {
                            const auto& type = module.types.get(block.parameters[index].type);
                            if (type.kind != TypeKind::Reference || type.arguments != result.arguments ||
                                (result.isMutable && !type.isMutable))
                                return false;
                            bool incoming = false;
                            for (const auto& predecessor : method.blocks)
                                for (const auto& instruction : predecessor.instructions)
                                    for (const auto& target : instruction.targets)
                                        if (target.block == block.id)
                                        {
                                            auto path = visiting;
                                            if (index >= target.arguments.size() ||
                                                !source(target.arguments[index], path))
                                                return false;
                                            incoming = true;
                                        }
                            return incoming;
                        }
                }
                return false;
            };
            bool returned = false;
            for (const auto& block : method.blocks)
                for (const auto& instruction : block.instructions)
                    if (instruction.opcode == lowered::Opcode::Return)
                    {
                        std::set<ValueId> visiting;
                        if (instruction.operands.size() != 1 || !source(instruction.operands.front(), visiting))
                            return false;
                        returned = true;
                    }
            return returned;
        }
    } // namespace

    std::set<wir::TypeId> WirNativeObjectEmitter::eligibleTypes(const wir::lowered::Module& module)
    {
        std::set<TypeId> candidates;
        std::vector<TypeId> pending;
        const auto collect = [&](TypeId id)
        {
            id = nativeAbiObjectType(module.types, id);
            const Type* type = module.types.tryGet(id);
            if (type && type->kind == TypeKind::Named && type->nominalKind == NominalKind::Object &&
                candidates.insert(id).second)
                pending.push_back(id);
        };
        for (const auto& declaration : module.functions)
            if (declaration.nativeBinding && declaration.genericParameters.empty())
            {
                for (const auto& parameter : declaration.nativeBinding->parameters)
                    collect(parameter.type);
                collect(declaration.nativeBinding->result.type);
            }
        const auto roots = candidates;
        // Method-only object dependencies may never appear in a free native
        // declaration. Discover the full signature closure before qualification.
        for (std::size_t index = 0; index < pending.size(); ++index)
            for (const auto& method : module.types.get(pending[index]).methods)
                if (const auto* implementation = function(module, method.function))
                {
                    collect(implementation->returnType);
                    for (const auto& parameter : implementation->parameters | std::views::drop(1))
                        collect(parameter.type);
                }

        const auto methodValue = [&](TypeId id, bool allowVoid = false, bool allowReference = false)
        {
            if (allowReference)
            {
                const auto& reference = module.types.get(id);
                if (reference.kind == TypeKind::Reference && reference.arguments.size() == 1)
                {
                    id = reference.arguments.front();
                    const auto& value = module.types.get(id);
                    return candidates.contains(id) ||
                           (value.kind == TypeKind::Nullable && value.arguments.size() == 1 &&
                            candidates.contains(value.arguments.front()));
                }
            }
            const auto& type = module.types.get(id);
            if (type.kind == TypeKind::Nullable && type.arguments.size() == 1)
                return candidates.contains(type.arguments.front());
            return wireValue(module, id, allowVoid) || candidates.contains(id);
        };
        // Reject transitively: no proxy may advertise a method whose object
        // signature depends on a type that failed whole-surface qualification.
        std::size_t previousSize = 0;
        do
        {
            previousSize = candidates.size();
            std::erase_if(
                candidates,
                [&](TypeId id)
                {
                    const auto& type = module.types.get(id);
                    const auto* reflected = reflection(module, id);
                    const bool onlyRootBases = std::ranges::all_of(
                        type.baseTypes,
                        [&](TypeId base)
                        {
                            const auto& root = module.types.get(base);
                            return root.kind == TypeKind::Named && root.nominalKind == NominalKind::Object &&
                                   root.name == "object" && root.fields.empty() && root.methods.empty() &&
                                   root.baseTypes.empty() && !root.defaultConstructor && !root.destructor;
                        });
                    if (!reflected || !reflected->stableTypeId || !type.arguments.empty() || !onlyRootBases ||
                        type.nominalRepresentation != NominalRepresentation::Wio ||
                        std::ranges::any_of(type.fields,
                                            [&](const auto& field)
                                            {
                                                // Placeholder fields must not run nested Wio
                                                // lifecycle or expose stale native projections.
                                                return field.visibility == FieldVisibility::Public ||
                                                       !wireValue(module, field.type);
                                            }))
                        return true;
                    const auto supported = [&](FunctionId methodId)
                    {
                        const auto* method = function(module, methodId);
                        return method && method->isMethod && !method->isAbstract && !method->isAsync &&
                               !method->isExternal && !method->nativeBinding && method->genericParameters.empty() &&
                               !method->parameters.empty() && methodIdentity(*reflected, methodId) &&
                               methodValue(method->returnType, true, true) && borrowedResultProven(module, *method) &&
                               std::ranges::all_of(method->parameters | std::views::drop(1), [&](const auto& parameter)
                                                   { return methodValue(parameter.type, false, true); });
                    };
                    return std::ranges::any_of(type.methods,
                                               [&](const auto& method) { return !supported(method.function); }) ||
                           std::ranges::any_of(type.dispatchEntries,
                                               [&](const auto& entry) { return !supported(entry.implementation); });
                });
        } while (candidates.size() != previousSize);

        std::set<TypeId> reachable;
        pending.clear();
        const auto include = [&](TypeId id)
        {
            if (const auto object = nativeAbiObjectType(module.types, id))
                id = object;
            if (candidates.contains(id) && reachable.insert(id).second)
                pending.push_back(id);
        };
        for (const auto root : roots)
            include(root);
        for (std::size_t index = 0; index < pending.size(); ++index)
            for (const auto& method : module.types.get(pending[index]).methods)
                if (const auto* implementation = function(module, method.function))
                {
                    include(implementation->returnType);
                    for (const auto& parameter : implementation->parameters | std::views::drop(1))
                        include(parameter.type);
                }
        return reachable;
    }

    std::string WirNativeObjectEmitter::emit(const wir::lowered::Module& module, const std::set<wir::TypeId>& objects,
                                             const WirModuleEmitter::TypeSpelling& cpp,
                                             const MethodSpelling& legacyMethod)
    {
        if (objects.empty())
            return {};
        std::ostringstream out;
        out << "#include <wir_object_abi.h>\n";
        for (const auto id : objects)
        {
            const auto& type = module.types.get(id);
            const auto& reflected = *reflection(module, id);
            const auto object = "_wio_obj" + std::to_string(id.value());
            const auto proxy = "_native_object_proxy_" + std::to_string(id.value());
            out << "struct " << proxy << " final : wio::wir_backend::abi::HostObject<" << object << "> {\n"
                << "    explicit " << proxy << "(const WioNativeObjectHost& host)\n"
                << "        : HostObject(host, wio::wir_backend::SkipConstructor{}) { _wio_hosted_proxy = true; }\n";
            const auto emitMethod = [&](const lowered::Function& method, const std::string& name)
            {
                out << "    " << cpp(method.returnType) << ' ' << name << '(';
                for (std::size_t index = 1; index < method.parameters.size(); ++index)
                {
                    if (index > 1)
                        out << ", ";
                    out << cpp(method.parameters[index].type) << " _a" << (index - 1);
                }
                out << ") override {\n";
                const auto count = method.parameters.size() - 1;
                const bool hasBorrows =
                    std::ranges::any_of(method.parameters | std::views::drop(1), [&](const auto& parameter)
                                        { return module.types.get(parameter.type).kind == TypeKind::Reference; });
                if (count)
                {
                    out << "        wio::wir_backend::abi::" << (hasBorrows ? "ObjectReferences<" : "ObjectArguments<")
                        << count << "> arguments;\n";
                    if (hasBorrows)
                        out << "        try {\n";
                    for (std::size_t index = 1; index < method.parameters.size(); ++index)
                    {
                        const auto& parameter = module.types.get(method.parameters[index].type);
                        if (parameter.kind == TypeKind::Reference)
                        {
                            const auto base = parameter.arguments.front();
                            out << "        arguments.borrow<" << cpp(base) << ", " << identity(module, base) << "ULL>("
                                << (index - 1) << ", _a" << (index - 1) << ", "
                                << (parameter.isMutable ? "true" : "false") << ");\n";
                            continue;
                        }
                        out << "        arguments[" << (index - 1) << "] = wio::wir_backend::abi::Codec<"
                            << cpp(method.parameters[index].type) << ", "
                            << identity(module, method.parameters[index].type) << "ULL>::write(std::move(_a"
                            << (index - 1) << "));\n";
                    }
                }
                const auto& resultType = module.types.get(method.returnType);
                const bool borrowedResult = resultType.kind == TypeKind::Reference;
                out << "        "
                    << (hasBorrows
                            ? (module.types.get(method.returnType).kind == TypeKind::Void ? "" : "auto result = ")
                            : "return ")
                    << "hostLease()." << (borrowedResult ? "callMapped<" : "call<") << cpp(method.returnType);
                if (!borrowedResult)
                    out << ", " << identity(module, method.returnType) << "ULL";
                out << ">(" << methodIdentity(reflected, method.id) << "ULL, "
                    << (count ? "arguments.data()" : "nullptr") << ", " << count;
                if (borrowedResult)
                {
                    const auto base = resultType.arguments.front();
                    const auto resultId = identity(module, base);
                    out << ", [&](const WioNativeAbiValue& borrowed) -> " << cpp(method.returnType) << " {\n"
                        << "            using wio::wir_backend::abi::require;\n"
                        << "            require(borrowed.typeId == " << resultId << "ULL);\n"
                        << "            if (borrowed.kind == WIO_NATIVE_ABI_REFERENCE) {\n"
                        << "                require(borrowed.flags == (WIO_NATIVE_ABI_VALUE_BORROWED"
                        << (resultType.isMutable ? " | WIO_NATIVE_ABI_VALUE_MUTABLE" : "")
                        << ") && !borrowed.owner.state && !borrowed.owner.ops && "
                           "borrowed.payload.mutableSlice.size == sizeof(WioNativeAbiValue));\n";
                    for (std::size_t index = 1; index < method.parameters.size(); ++index)
                    {
                        const auto& parameter = module.types.get(method.parameters[index].type);
                        if (parameter.kind == TypeKind::Reference && parameter.arguments.front() == base &&
                            (!resultType.isMutable || parameter.isMutable))
                            out << "                if (borrowed.payload.mutableSlice.data == arguments[" << (index - 1)
                                << "].payload.mutableSlice.data) return _a" << (index - 1)
                                << (resultType.isMutable ? "" : ".asView()") << ";\n";
                    }
                    out << "            }\n";
                    if (base == id)
                        out << "            if (borrowed.kind == WIO_NATIVE_ABI_OBJECT) {\n"
                            << "                require(borrowed.flags == WIO_NATIVE_ABI_VALUE_OWNED && "
                               "!borrowed.owner.state && !borrowed.owner.ops && borrowed.payload.handle.state && "
                               "borrowed.payload.handle.generation == 1);\n"
                            << "                auto receiver = wio::wir_backend::abi::Codec<" << cpp(id) << ", "
                            << resultId << "ULL>::read(borrowed);\n"
                            << "                auto* hosted = dynamic_cast<const wio::wir_backend::abi::HostObject<"
                            << object << ">*>(receiver.Get());\n"
                            << "                require(hosted != nullptr);\n"
                            << "                auto returnedHost = hosted->hostLease().host();\n"
                            << "                auto expectedHost = hostLease().host();\n"
                            << "                require(returnedHost.owner.state == expectedHost.owner.state && "
                               "returnedHost.objectId == expectedHost.objectId && "
                               "returnedHost.generation == expectedHost.generation && "
                               "returnedHost.invoke == expectedHost.invoke);\n"
                            << "                return " << cpp(method.returnType) << "::raw(this)"
                            << (resultType.isMutable ? "" : ".asView()") << ";\n"
                            << "            }\n";
                    out << "            throw wio::wir_backend::abi::Error(WIO_NATIVE_ABI_TYPE_MISMATCH);\n"
                        << "        }";
                }
                out << ");\n";
                if (hasBorrows)
                {
                    out << "        arguments.commit();\n";
                    if (module.types.get(method.returnType).kind != TypeKind::Void)
                        out << "        return result;\n";
                    out << "        } catch (...) { try { arguments.commit(); } catch (...) {} throw; }\n";
                }
                out << "    }\n";
            };
            for (const auto& entry : type.dispatchEntries)
                emitMethod(*function(module, entry.implementation), "_dispatch_" +
                                                                        std::to_string(entry.contractType.value()) +
                                                                        "_" + std::to_string(entry.slot));
            std::set<std::string> names;
            for (const auto& method : type.methods)
                if (method.visibility == FieldVisibility::Public)
                {
                    const auto& implementation = *function(module, method.function);
                    const auto name = legacyMethod(method, implementation);
                    if (names.insert(name).second)
                        emitMethod(implementation, name);
                }
            out << "};\nusing _native_object_" << id.value() << " = wio::wir_backend::abi::ObjectAdapter<" << object
                << ", " << proxy << ", " << reflected.stableTypeId << "ULL>;\n";
        }
        out << "static const WioNativeObjectDescriptor _native_objects[] = {\n";
        for (const auto id : objects)
        {
            const auto adapter = "_native_object_" + std::to_string(id.value());
            out << "    {" << reflection(module, id)->stableTypeId
                << "ULL, WIO_NATIVE_OBJECT_HOST_PROXY | WIO_NATIVE_OBJECT_WEAK_CACHE, &" << adapter << "::create, &"
                << adapter << "::host, &" << adapter << "::weak, &" << adapter << "::lock},\n";
        }
        out << "};\nstatic const WioNativeObjectApi _native_object_api{WIO_NATIVE_OBJECT_VERSION, " << objects.size()
            << ", _native_objects};\n"
            << "extern \"C\" WIO_WIR_EXPORT const WioNativeObjectApi* WioGetNativeObjectApi() noexcept {\n"
            << "    return &_native_object_api;\n}\n";
        return out.str();
    }
} // namespace wio::codegen
