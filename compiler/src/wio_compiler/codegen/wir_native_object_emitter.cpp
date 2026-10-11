#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/codegen/wir_native_pod_emitter.h"
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

        bool concreteArguments(const TypeTable& types, TypeId id, unsigned depth = 0)
        {
            const auto* type = types.tryGet(id);
            if (!type || depth >= 64 || type->extentParameter || type->kind == TypeKind::Invalid ||
                type->kind == TypeKind::GenericParameter || type->kind == TypeKind::ConstGenericParameter ||
                type->kind == TypeKind::GenericParameterPack || type->kind == TypeKind::TypePack ||
                type->kind == TypeKind::ValuePack || type->kind == TypeKind::PackStorage)
                return false;
            // Specialization arguments identify the type, not its physical
            // fields. Unused string/type/const arguments are valid identities.
            return std::ranges::all_of(type->arguments,
                                       [&](TypeId argument) { return concreteArguments(types, argument, depth + 1); });
        }

        bool supportedHierarchy(const lowered::Module& module, TypeId id, std::set<TypeId>& path,
                                std::vector<unsigned>& heights)
        {
            const auto* type = module.types.tryGet(id);
            if (type && heights[id.value()] != 0)
                return path.size() + heights[id.value()] <= 64;
            if (!type || path.size() >= 64 || !path.insert(id).second)
                return false;
            // Proxy placeholders must not run nested Wio lifecycle or expose
            // stale native field projections, including inherited fields.
            const bool supported =
                type->kind == TypeKind::Named &&
                (type->nominalKind == NominalKind::Object || type->nominalKind == NominalKind::Interface) &&
                type->nominalRepresentation == NominalRepresentation::Wio && concreteArguments(module.types, id) &&
                std::ranges::all_of(
                    type->fields, [&](const auto& field)
                    { return field.visibility != FieldVisibility::Public && wireValue(module, field.type); }) &&
                std::ranges::all_of(type->baseTypes,
                                    [&](TypeId base) { return supportedHierarchy(module, base, path, heights); });
            if (supported)
            {
                unsigned height = 1;
                for (const auto base : type->baseTypes)
                    height = std::max(height, heights[base.value()] + 1);
                heights[id.value()] = height;
            }
            path.erase(id);
            return supported;
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
        std::set<TypeId> boundaries;
        bool anyBoundary = false;
        std::vector<TypeId> pending;
        const auto collect = [&](TypeId id)
        {
            id = nativeAbiObjectType(module.types, id);
            const Type* type = module.types.tryGet(id);
            if (type && type->kind == TypeKind::Named && type->nominalKind == NominalKind::Object &&
                candidates.insert(id).second)
                pending.push_back(id);
        };
        std::set<TypeId> visitedBoundaries;
        std::vector<TypeId> boundaryPending;
        std::size_t boundaryCursor = 0;
        const auto boundary = [&](TypeId id)
        {
            boundaryPending.push_back(id);
            for (; boundaryCursor < boundaryPending.size(); ++boundaryCursor)
            {
                const auto value = boundaryPending[boundaryCursor];
                const auto* type = module.types.tryGet(value);
                if (!type || !visitedBoundaries.insert(value).second)
                    continue;
                anyBoundary |= type->kind == TypeKind::Any;
                if (const auto object = nativeAbiObjectType(module.types, value))
                    boundaries.insert(object);
                collect(value);
                // Walk value shapes iteratively, not nominal template fields.
                // A container may be the only object/Any native boundary.
                if (type->kind == TypeKind::Reference || type->kind == TypeKind::Nullable ||
                    type->kind == TypeKind::AsyncTask || type->kind == TypeKind::Array ||
                    type->kind == TypeKind::Dictionary || type->kind == TypeKind::Function)
                    boundaryPending.insert(boundaryPending.end(), type->arguments.begin(), type->arguments.end());
            }
        };
        for (const auto& declaration : module.functions)
            if (declaration.nativeBinding && declaration.genericParameters.empty())
            {
                for (const auto& parameter : declaration.nativeBinding->parameters)
                    boundary(parameter.type);
                boundary(declaration.nativeBinding->result.type);
            }
        if (anyBoundary)
            for (const auto& declaration : module.functions)
                for (const auto& block : declaration.blocks)
                    for (const auto& instruction : block.instructions)
                        if (instruction.opcode == lowered::Opcode::AnyBox ||
                            instruction.opcode == lowered::Opcode::AnyCheckedCast ||
                            instruction.opcode == lowered::Opcode::AnyTypeTest)
                        {
                            boundary(instruction.targetType);
                        }
        // A Base/interface-only boundary still needs concrete descendant
        // adapters. Never materialize an abstract interface placeholder.
        if (boundaries.empty())
            return {};
        std::vector<std::vector<TypeId>> descendants(module.types.size());
        for (std::uint32_t index = 0; index < module.types.size(); ++index)
        {
            const TypeId id{index};
            const auto& type = module.types.get(id);
            if (type.kind == TypeKind::Named)
                for (const auto base : type.baseTypes)
                    if (module.types.tryGet(base))
                        descendants[base.value()].push_back(id);
        }
        std::set<TypeId> family = boundaries;
        std::vector<TypeId> familyPending(boundaries.begin(), boundaries.end());
        for (std::size_t index = 0; index < familyPending.size(); ++index)
        {
            collect(familyPending[index]);
            for (const auto child : descendants[familyPending[index].value()])
                if (family.insert(child).second)
                    familyPending.push_back(child);
        }
        const auto roots = candidates;
        std::size_t familyCursor = familyPending.size();
        const auto collectFamily = [&](auto&& self, TypeId id, unsigned depth = 0) -> void
        {
            const auto* shape = module.types.tryGet(id);
            if (!shape || depth > 64)
                return;
            if (shape->kind == TypeKind::Function || shape->kind == TypeKind::Array ||
                shape->kind == TypeKind::Dictionary)
            {
                for (const auto argument : shape->arguments)
                    self(self, argument, depth + 1);
                return;
            }
            id = nativeAbiObjectType(module.types, id);
            if (!id)
                return;
            if (family.insert(id).second)
                familyPending.push_back(id);
            for (; familyCursor < familyPending.size(); ++familyCursor)
            {
                collect(familyPending[familyCursor]);
                for (const auto child : descendants[familyPending[familyCursor].value()])
                    if (family.insert(child).second)
                        familyPending.push_back(child);
            }
        };
        // Method-only object dependencies may never appear in a free native
        // declaration. Discover the full signature closure before qualification.
        for (std::size_t index = 0; index < pending.size(); ++index)
            for (const auto& method : module.types.get(pending[index]).methods)
                if (const auto* implementation = function(module, method.function))
                {
                    collectFamily(collectFamily, implementation->returnType);
                    for (const auto& parameter : implementation->parameters | std::views::drop(1))
                        collectFamily(collectFamily, parameter.type);
                }

        std::set<TypeId> contracts;
        const auto pods = WirNativePodEmitter::eligibleTypes(module);
        const auto callbackValue = [&](auto&& self, TypeId id, unsigned depth = 0) -> bool
        {
            const auto& type = module.types.get(id);
            if (depth > 64)
                return false;
            if (type.kind == TypeKind::Array)
                return !type.staticExtent && !type.extentParameter && type.arguments.size() == 1 &&
                       self(self, type.arguments.front(), depth + 1);
            if (type.kind == TypeKind::Dictionary)
            {
                if (type.arguments.size() != 2)
                    return false;
                const auto& key = module.types.get(type.arguments.front());
                return ((key.kind >= TypeKind::Bool && key.kind <= TypeKind::USize) ||
                        (key.kind >= TypeKind::Byte && key.kind <= TypeKind::Text)) &&
                       self(self, type.arguments.back(), depth + 1);
            }
            if (type.kind == TypeKind::Nullable && type.arguments.size() == 1)
                return contracts.contains(type.arguments.front());
            return wireValue(module, id, true) || contracts.contains(id) || type.kind == TypeKind::Any ||
                   std::ranges::find(pods, id) != pods.end();
        };
        const auto methodValue = [&](TypeId id, bool allowVoid = false, bool allowReference = false)
        {
            if (allowReference)
            {
                const auto& reference = module.types.get(id);
                if (reference.kind == TypeKind::Reference && reference.arguments.size() == 1)
                {
                    id = reference.arguments.front();
                    const auto& value = module.types.get(id);
                    return contracts.contains(id) || (value.kind == TypeKind::Nullable && value.arguments.size() == 1 &&
                                                      contracts.contains(value.arguments.front()));
                }
            }
            const auto& type = module.types.get(id);
            if (type.kind == TypeKind::Function)
            {
                if (allowVoid || type.arguments.empty() || !callbackValue(callbackValue, type.arguments.back()))
                    return false;
                for (std::size_t index = 0; index + 1 < type.arguments.size(); ++index)
                {
                    auto argument = type.arguments[index];
                    const auto& reference = module.types.get(argument);
                    if (reference.kind == TypeKind::Reference && reference.arguments.size() == 1)
                    {
                        argument = reference.arguments.front();
                        const auto& base = module.types.get(argument);
                        if (!(base.kind >= TypeKind::Bool && base.kind <= TypeKind::F64) &&
                            base.kind != TypeKind::Byte && base.kind != TypeKind::Char &&
                            base.kind != TypeKind::String && base.kind != TypeKind::Text &&
                            std::ranges::find(pods, argument) == pods.end())
                            return false;
                    }
                    else if (!callbackValue(callbackValue, argument))
                        return false;
                }
                return true;
            }
            if (type.kind == TypeKind::Nullable && type.arguments.size() == 1)
                return contracts.contains(type.arguments.front());
            return wireValue(module, id, allowVoid) || contracts.contains(id);
        };
        // Reject transitively: no proxy may advertise a method whose object
        // signature depends on a type that failed whole-surface qualification.
        std::size_t previousSize = 0;
        std::vector<unsigned> hierarchyHeights(module.types.size());
        do
        {
            previousSize = candidates.size();
            contracts = hostedHierarchyTypes(module, candidates);
            std::erase_if(
                candidates,
                [&](TypeId id)
                {
                    const auto& type = module.types.get(id);
                    const auto* reflected = reflection(module, id);
                    std::set<TypeId> hierarchyPath;
                    if (!reflected || !reflected->stableTypeId ||
                        !supportedHierarchy(module, id, hierarchyPath, hierarchyHeights))
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
        std::vector<std::vector<TypeId>> contractObjects(module.types.size());
        for (const auto candidate : candidates)
            for (const auto contract : hostedHierarchyTypes(module, {candidate}))
                contractObjects[contract.value()].push_back(candidate);
        pending.clear();
        const auto include = [&](auto&& self, TypeId id, unsigned depth = 0) -> void
        {
            const auto* shape = module.types.tryGet(id);
            if (!shape || depth > 64)
                return;
            if (shape->kind == TypeKind::Function || shape->kind == TypeKind::Array ||
                shape->kind == TypeKind::Dictionary)
            {
                for (const auto argument : shape->arguments)
                    self(self, argument, depth + 1);
                return;
            }
            if (const auto object = nativeAbiObjectType(module.types, id))
                id = object;
            if (module.types.tryGet(id))
                for (const auto candidate : contractObjects[id.value()])
                    if (reachable.insert(candidate).second)
                        pending.push_back(candidate);
        };
        for (const auto root : roots)
            include(include, root);
        for (std::size_t index = 0; index < pending.size(); ++index)
            for (const auto& method : module.types.get(pending[index]).methods)
                if (const auto* implementation = function(module, method.function))
                {
                    include(include, implementation->returnType);
                    for (const auto& parameter : implementation->parameters | std::views::drop(1))
                        include(include, parameter.type);
                }
        return reachable;
    }

    std::set<wir::TypeId> WirNativeObjectEmitter::hostedHierarchyTypes(const wir::lowered::Module& module,
                                                                       const std::set<wir::TypeId>& objects)
    {
        std::set<wir::TypeId> result = objects;
        std::vector<wir::TypeId> pending(objects.begin(), objects.end());
        for (std::size_t index = 0; index < pending.size(); ++index)
            if (const auto* type = module.types.tryGet(pending[index]))
                for (const auto base : type->baseTypes)
                    if (result.insert(base).second)
                        pending.push_back(base);
        return result;
    }

    std::string WirNativeObjectEmitter::emitCodecs(const wir::lowered::Module& module,
                                                   const std::set<wir::TypeId>& objects,
                                                   const WirModuleEmitter::TypeSpelling& cpp)
    {
        if (objects.empty())
            return {};
        const auto hierarchy = hostedHierarchyTypes(module, objects);
        std::vector<TypeId> ordered(hierarchy.begin(), hierarchy.end());
        std::vector<std::set<TypeId>> ancestors(module.types.size());
        for (const auto id : hierarchy)
            ancestors[id.value()] = hostedHierarchyTypes(module, {id});
        // Concrete leaf definitions precede any ancestor codec that calls them.
        std::ranges::sort(ordered, [&](TypeId left, TypeId right)
                          { return ancestors[left.value()].size() > ancestors[right.value()].size(); });
        std::ostringstream out;
        out << "namespace wio::wir_backend::abi {\n";
        for (const auto id : ordered)
        {
            const auto stableId = identity(module, id);
            out << "template<> struct Codec<" << cpp(id) << ", " << stableId << "ULL>"
                << " : ExactObjectCodec<typename " << cpp(id) << "::element_type, " << stableId << "ULL> {\n"
                << "    using Handle = " << cpp(id) << ";\n"
                << "    using Exact = ExactObjectCodec<typename Handle::element_type, " << stableId << "ULL>;\n"
                << "    static Handle read(const WioNativeAbiValue& value) {\n";
            for (const auto child : ordered)
                if (child != id && objects.contains(child) && ancestors[child.value()].contains(id))
                    out << "        if (value.typeId == " << identity(module, child) << "ULL) return Handle(Codec<"
                        << cpp(child) << ", " << identity(module, child) << "ULL>::read(value));\n";
            out << "        return Exact::read(value);\n    }\n"
                << "    static WioNativeAbiValue write(Handle value) {\n";
            for (const auto child : ordered)
                if (child != id && objects.contains(child) && ancestors[child.value()].contains(id))
                    out << "        if (auto* concrete = dynamic_cast<typename " << cpp(child)
                        << "::element_type*>(value.Get())) return Codec<" << cpp(child) << ", "
                        << identity(module, child) << "ULL>::write(" << cpp(child) << "(concrete));\n";
            out << "        return Exact::write(std::move(value));\n    }\n};\n";
        }
        out << "} // namespace wio::wir_backend::abi\n";
        return out.str();
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
                << "        : HostObject(host, wio::wir_backend::SkipHostedConstructor{})";
            for (const auto ancestor : hostedHierarchyTypes(module, {id}))
                if (ancestor != id && module.types.get(ancestor).nominalKind == NominalKind::Interface)
                    out << ", _wio_obj" << ancestor.value() << "(wio::wir_backend::SkipHostedConstructor{})";
            out << " {}\n";
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
                        out << "        arguments[" << (index - 1)
                            << "] = " << WirModuleEmitter::nativeValueCodec(module, method.parameters[index].type, cpp)
                            << "::write(std::move(_a" << (index - 1) << "));\n";
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
                        << "            if (borrowed.kind == WIO_NATIVE_ABI_REFERENCE) {\n"
                        << "                require(borrowed.typeId == " << resultId << "ULL);\n"
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
                    if (hostedHierarchyTypes(module, {id}).contains(base))
                        out << "            if (borrowed.kind == WIO_NATIVE_ABI_OBJECT) {\n"
                            << "                require(borrowed.typeId == " << identity(module, id)
                            << "ULL && borrowed.flags == WIO_NATIVE_ABI_VALUE_OWNED && "
                               "!borrowed.owner.state && !borrowed.owner.ops && borrowed.payload.handle.state && "
                               "borrowed.payload.handle.generation == 1);\n"
                            << "                auto receiver = wio::wir_backend::abi::Codec<" << cpp(base) << ", "
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
                            << "                return " << cpp(method.returnType) << "::raw(static_cast<typename "
                            << cpp(base) << "::element_type*>(this))" << (resultType.isMutable ? "" : ".asView()")
                            << ";\n"
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
