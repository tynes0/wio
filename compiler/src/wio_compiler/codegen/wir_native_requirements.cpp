#include "wio/codegen/wir_native_requirements.h"
#include "wio/codegen/wir_native_bridge_plan.h"
#include "wio/bytecode/codec.h"
#include "wio/bytecode/compiler.h"
#include "wio/wir/native_abi_types.h"
#include "std_hash.h"
#include "std_serialization.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <set>

namespace wio::codegen
{
    namespace
    {
        using namespace wir;

        template <class Id> using IdMap = std::map<std::uint32_t, Id>;

        template <class Id> Id mapped(const IdMap<Id>& mapping, const Id id)
        {
            if (!id)
                return {};
            const auto found = mapping.find(id.value());
            if (found == mapping.end())
                throw std::runtime_error("native requirements reference a declaration outside the bridge plan");
            return found->second;
        }

        // Length-prefix keys: nominal names and generic constants may contain
        // punctuation. Never concatenate ambiguous delimiter-only identities.
        void append(std::string& out, const std::string_view value)
        {
            out += std::to_string(value.size()) + ':';
            out.append(value);
        }

        std::string typeKey(const TypeTable& types, const TypeId id, const unsigned depth = 0)
        {
            if (!id)
                return {};
            if (depth > 128)
                throw std::runtime_error("native requirements type identity exceeds its depth limit");
            const auto& type = types.get(id);
            std::string key = std::to_string(static_cast<unsigned>(type.kind)) + ':';
            append(key, type.name);
            append(key, type.isMutable ? "mutable" : "view");
            append(key, type.staticExtent ? std::to_string(*type.staticExtent) : "dynamic");
            append(key, std::to_string(static_cast<unsigned>(type.nominalKind)));
            append(key, std::to_string(static_cast<unsigned>(type.nominalRepresentation)));
            append(key, std::to_string(static_cast<unsigned>(type.nominalValueModel)));
            append(key, std::to_string(static_cast<unsigned>(type.ownership)));
            append(key, std::to_string(static_cast<unsigned>(type.cleanup)));
            append(key, typeKey(types, type.extentParameter, depth + 1));
            for (const auto argument : type.arguments)
                append(key, typeKey(types, argument, depth + 1));
            return key;
        }

        std::string functionKey(const lowered::Module& module, const lowered::Function& function)
        {
            std::string key;
            append(key, function.name);
            append(key, function.specializationKey);
            append(key, typeKey(module.types, function.ownerType));
            append(key, typeKey(module.types, function.returnType));
            for (const auto& parameter : function.parameters)
                append(key, typeKey(module.types, parameter.type));
            return key;
        }

        struct CanonicalPlan final
        {
            lowered::Module module;
            IdMap<TypeId> types;
            IdMap<FunctionId> functions;
            IdMap<GlobalId> globals;

            explicit CanonicalPlan(const lowered::Module& source)
            {
                module = source;
                module.name = "wio-native-requirements-v1";
                auto& contract = module.contract;
                contract.logicalName = module.name;
                contract.stableKey = module.name;
                contract.stableId = stableModuleHash(module.name);
                contract.callTable.stableId = stableModuleHash(contract.stableKey + ":sdk-call-table:v" +
                                                               std::to_string(contract.callTable.descriptorVersion));
                // Compile-time attributes have already become typed bindings
                // and lowered bodies. Their source/FunctionId-based metadata
                // IDs are not emitted by the native reflection table.
                std::set<std::uint64_t> runtimeAttributes;
                for (const auto& attribute : contract.attributes)
                    if (attribute.runtimeRetained)
                        runtimeAttributes.insert(attribute.stableId);
                std::erase_if(contract.attributes, [](const auto& attribute) { return !attribute.runtimeRetained; });
                const auto keepRuntime = [&](auto& ids)
                { std::erase_if(ids, [&](const auto id) { return !runtimeAttributes.contains(id); }); };
                for (auto& reflection : contract.reflection)
                {
                    keepRuntime(reflection.attributes);
                    for (auto& field : reflection.fields)
                        keepRuntime(field.attributes);
                    for (auto& method : reflection.methods)
                        keepRuntime(method.attributes);
                    for (auto& entry : reflection.cases)
                        keepRuntime(entry.attributes);
                }
                std::erase_if(contract.imports,
                              [](const auto& entry)
                              {
                                  return entry.kind != ModuleImportKind::NativeHeader &&
                                         entry.kind != ModuleImportKind::NativeLibrary;
                              });
                for (auto& entry : contract.imports)
                {
                    entry.sourcePath.clear();
                    entry.alias.clear();
                    entry.importedSymbols.clear();
                    entry.importAll = false;
                    entry.stableId =
                        stableModuleHash(std::to_string(static_cast<unsigned>(entry.kind)) + ':' + entry.logicalName);
                }

                std::set<std::uint64_t> importIds;
                std::erase_if(contract.imports,
                              [&](const auto& entry) { return !importIds.insert(entry.stableId).second; });

                // Emission views contain sparse void tombstones. Allocate a
                // dense, key-ordered table before rebinding recursive edges.
                TypeTable dense;
                std::map<std::string, TypeId> keys;
                for (std::uint32_t index = 0; index < dense.size(); ++index)
                    keys.emplace(typeKey(dense, TypeId{index}), TypeId{index});
                std::map<std::string, std::vector<TypeId>> ordered;
                for (std::uint32_t index = 0; index < source.types.size(); ++index)
                    ordered[typeKey(source.types, TypeId{index})].push_back(TypeId{index});
                for (const auto& [key, ids] : ordered)
                {
                    auto found = keys.find(key);
                    if (found == keys.end())
                    {
                        const auto id = dense.intern(Type{
                            .kind = TypeKind::Named, .name = "__native_identity_slot_" + std::to_string(keys.size())});
                        found = keys.emplace(key, id).first;
                    }
                    for (const auto id : ids)
                    {
                        if (source.types.get(id) != source.types.get(ids.front()))
                            throw std::runtime_error("ambiguous native requirements type identity");
                        types.emplace(id.value(), found->second);
                    }
                }
                for (const auto& [key, ids] : ordered)
                    dense.getMutable(keys.at(key)) = source.types.get(ids.front());
                module.types = std::move(dense);

                std::ranges::sort(module.functions, {},
                                  [&](const auto& function) { return functionKey(source, function); });
                std::string previous;
                bool first = true;
                for (std::uint32_t index = 0; index < module.functions.size(); ++index)
                {
                    auto& function = module.functions[index];
                    const auto key = functionKey(source, function);
                    if (!first && key == previous)
                        throw std::runtime_error("ambiguous native requirements function identity");
                    first = false;
                    previous = key;
                    functions.emplace(function.id.value(), FunctionId{index});
                }
                std::ranges::sort(module.globals, {}, &lowered::Global::name);
                for (std::uint32_t index = 0; index < module.globals.size(); ++index)
                    globals.emplace(module.globals[index].id.value(), GlobalId{index});

                for (std::uint32_t index = 0; index < module.types.size(); ++index)
                {
                    auto& type = module.types.getMutable(TypeId{index});
                    typeIds(type.arguments);
                    typeId(type.extentParameter);
                    typeIds(type.baseTypes);
                    typeIds(type.castTypes);
                    typeId(type.enumUnderlyingType);
                    functionId(type.destructor);
                    functionId(type.defaultConstructor);
                    functionId(type.fieldInitializer);
                    for (auto& field : type.fields)
                        typeId(field.type);
                    for (auto& method : type.methods)
                    {
                        typeIds(method.parameterTypes);
                        typeId(method.returnType);
                        functionId(method.function);
                    }
                    for (auto& entry : type.dispatchEntries)
                    {
                        typeId(entry.contractType);
                        functionId(entry.implementation);
                    }
                }
                for (auto& function : module.functions)
                {
                    functionId(function.id);
                    functionId(function.genericOrigin);
                    function.source = {};
                    typeId(function.returnType);
                    typeId(function.callableType);
                    typeId(function.ownerType);
                    typeIds(function.genericParameters);
                    typeIds(function.specializationArguments);
                    for (auto& parameter : function.parameters)
                        normalize(parameter);
                    for (auto& capture : function.captures)
                        typeId(capture.type);
                    if (function.nativeBinding)
                    {
                        auto& binding = *function.nativeBinding;
                        typeId(binding.result.type);
                        for (auto& parameter : binding.parameters)
                            typeId(parameter.type);
                        typeIds(binding.templateArguments);
                    }
                    if (function.coroutine)
                    {
                        typeId(function.coroutine->resultType);
                        for (auto& slot : function.coroutine->frameSlots)
                            typeId(slot.type);
                        for (auto& state : function.coroutine->states)
                            typeId(state.resultType);
                    }
                    for (auto& block : function.blocks)
                    {
                        block.source = {};
                        for (auto& parameter : block.parameters)
                            normalize(parameter);
                        for (auto& instruction : block.instructions)
                        {
                            instruction.source = {};
                            typeId(instruction.resultType);
                            typeId(instruction.targetType);
                            typeIds(instruction.signatureTypes);
                            typeIds(instruction.genericArguments);
                            functionId(instruction.callee);
                            instruction.global = mapped(globals, instruction.global);
                        }
                    }
                }
                for (auto& global : module.globals)
                {
                    global.id = mapped(globals, global.id);
                    typeId(global.type);
                    functionId(global.initializer);
                    global.source = {};
                }
                for (auto& attribute : contract.attributes)
                {
                    typeId(attribute.targetType);
                    functionId(attribute.targetFunction);
                    for (auto& argument : attribute.arguments)
                        typeId(argument.type);
                    for (auto& processor : attribute.processors)
                    {
                        typeId(processor.processorType);
                        typeId(processor.valueType);
                        functionId(processor.hookFunction);
                    }
                }
                for (auto& reflection : contract.reflection)
                {
                    typeId(reflection.type);
                    typeIds(reflection.genericArguments);
                    for (auto& field : reflection.fields)
                        typeId(field.type);
                    for (auto& method : reflection.methods)
                    {
                        functionId(method.function);
                        typeId(method.returnType);
                        typeIds(method.parameterTypes);
                    }
                }
                std::ranges::sort(contract.reflection, {}, &ReflectionDescriptor::logicalName);
                for (auto& system : contract.systems)
                {
                    typeId(system.type);
                    functionId(system.start);
                    functionId(system.update);
                    functionId(system.close);
                }
                if (contract.application)
                {
                    auto& application = *contract.application;
                    typeId(application.type);
                    typeIds(application.systems);
                    for (auto* id : {&application.construct, &application.entry, &application.start,
                                     &application.update, &application.close, &application.exit})
                        functionId(*id);
                    for (auto& stage : application.stages)
                        for (auto& run : stage.runs)
                        {
                            typeId(run.targetType);
                            functionId(run.function);
                            for (auto& resource : run.resources)
                                typeId(resource.type);
                        }
                }
            }

            void typeId(TypeId& id) const
            {
                id = mapped(types, id);
            }
            void functionId(FunctionId& id) const
            {
                id = mapped(functions, id);
            }
            void typeIds(std::vector<TypeId>& ids) const
            {
                for (auto& id : ids)
                    typeId(id);
            }
            void normalize(lowered::Parameter& parameter) const
            {
                typeId(parameter.type);
                parameter.source = {};
            }
        };

        void sortedUnique(std::vector<std::string>& values)
        {
            std::ranges::sort(values);
            values.erase(std::unique(values.begin(), values.end()), values.end());
        }
    } // namespace

    WirNativeRequirements WirNativeRequirements::build(const wir::lowered::Module& source)
    {
        const auto plan = WirNativeBridgePlan::build(source);
        const CanonicalPlan canonical{plan.module};
        const auto compiled = bytecode::Compiler{}.compile(canonical.module);
        if (!compiled.succeeded())
            throw std::runtime_error("native requirements normalization failed: " +
                                     compiled.diagnostics().front().message);
        const auto bytes = bytecode::encode(compiled.module());
        std::string identity = "wio-native-requirements-v1\n";
        append(identity, {reinterpret_cast<const char*>(bytes.data()), bytes.size()});
        WirNativeRequirements result;
        std::vector<std::string> payloadIdentity;
        for (const auto id : plan.dynamicPayloadTypes)
        {
            result.dynamicAnyPayloads.push_back(wir::nativeAbiTypeKey(plan.module.types, id));
            payloadIdentity.push_back(typeKey(plan.module.types, id));
        }
        sortedUnique(result.dynamicAnyPayloads);
        sortedUnique(payloadIdentity);
        for (const auto& key : payloadIdentity)
            append(identity, key);
        result.sha256 = runtime::std_hash::Sha256(identity);
        for (const auto& function : plan.module.functions)
        {
            if (function.nativeBinding && function.genericParameters.empty())
                result.nativeBindings.push_back(function.nativeBinding->stableKey);
            else if (!function.blocks.empty())
                result.embeddedWioFunctions.push_back(functionKey(plan.module, function));
        }
        for (const auto& type : plan.module.types.types())
            if (type.kind != wir::TypeKind::Void)
                result.retainedTypes.push_back(wir::nativeAbiTypeKey(
                    plan.module.types,
                    wir::TypeId{static_cast<std::uint32_t>(&type - plan.module.types.types().data())}));
        for (const auto& entry : canonical.module.contract.imports)
            result.nativeImports.push_back(std::string(wir::moduleImportKindName(entry.kind)) + ':' +
                                           entry.logicalName);
        sortedUnique(result.nativeBindings);
        sortedUnique(result.retainedTypes);
        sortedUnique(result.embeddedWioFunctions);
        return result;
    }

    std::string WirNativeRequirements::toJson() const
    {
        using runtime::std_serialization::JsonQuote;
        std::string output = "{\n  \"schemaVersion\": 1,\n  \"kind\": \"wio-native-requirements\",\n"
                             "  \"algorithm\": \"sha256\",\n  \"sha256\": " +
                             JsonQuote(sha256) +
                             ",\n  \"cacheReuseEnabled\": false,\n  \"dependencyClosureIncluded\": false,\n"
                             "  \"abiDescriptorVersion\": " +
                             std::to_string(wir::ModuleAbiDescriptorVersion);
        const auto array = [&](std::string_view name, const auto& values)
        {
            output += ",\n  " + JsonQuote(name) + ": [";
            bool first = true;
            for (const auto& value : values)
            {
                if (!first)
                    output += ',';
                output += '\n' + std::string("    ") + JsonQuote(value);
                first = false;
            }
            output += values.empty() ? "]" : "\n  ]";
        };
        array("nativeBindings", nativeBindings);
        array("retainedTypes", retainedTypes);
        array("dynamicAnyPayloads", dynamicAnyPayloads);
        array("embeddedWioFunctions", embeddedWioFunctions);
        array("nativeImports", nativeImports);
        return output + "\n}\n";
    }
} // namespace wio::codegen
