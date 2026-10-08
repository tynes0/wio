#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/wir/native_abi_types.h"

#include <algorithm>
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
    } // namespace

    std::set<wir::TypeId> WirNativeObjectEmitter::eligibleTypes(const wir::lowered::Module& module)
    {
        std::set<TypeId> candidates;
        std::vector<TypeId> pending;
        const auto collect = [&](TypeId id)
        {
            const Type* type = module.types.tryGet(id);
            if (type && type->kind == TypeKind::Reference && type->arguments.size() == 1)
            {
                id = type->arguments.front();
                type = module.types.tryGet(id);
            }
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

        const auto methodValue = [&](TypeId id, bool allowVoid = false)
        { return wireValue(module, id, allowVoid) || candidates.contains(id); };
        // Reject transitively: no proxy may advertise a method whose object
        // signature depends on a type that failed whole-surface qualification.
        std::size_t previousSize = 0;
        do
        {
            previousSize = candidates.size();
            std::erase_if(candidates,
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
                                         !method->isExternal && !method->nativeBinding &&
                                         method->genericParameters.empty() && !method->parameters.empty() &&
                                         methodIdentity(*reflected, methodId) &&
                                         methodValue(method->returnType, true) &&
                                         std::ranges::all_of(method->parameters | std::views::drop(1),
                                                             [&](const auto& parameter)
                                                             { return methodValue(parameter.type); });
                              };
                              return std::ranges::any_of(type.methods, [&](const auto& method)
                                                         { return !supported(method.function); }) ||
                                     std::ranges::any_of(type.dispatchEntries, [&](const auto& entry)
                                                         { return !supported(entry.implementation); });
                          });
        } while (candidates.size() != previousSize);

        std::set<TypeId> reachable;
        pending.clear();
        const auto include = [&](TypeId id)
        {
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
                if (count)
                {
                    out << "        wio::wir_backend::abi::ObjectArguments<" << count << "> arguments;\n";
                    for (std::size_t index = 1; index < method.parameters.size(); ++index)
                        out << "        arguments[" << (index - 1) << "] = wio::wir_backend::abi::Codec<"
                            << cpp(method.parameters[index].type) << ", "
                            << identity(module, method.parameters[index].type) << "ULL>::write(std::move(_a"
                            << (index - 1) << "));\n";
                }
                out << "        return hostLease().call<" << cpp(method.returnType) << ", "
                    << identity(module, method.returnType) << "ULL>(" << methodIdentity(reflected, method.id) << "ULL, "
                    << (count ? "arguments.data()" : "nullptr") << ", " << count << ");\n    }\n";
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
