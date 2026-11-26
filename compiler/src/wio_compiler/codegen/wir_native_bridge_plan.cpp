#include "wio/codegen/wir_native_bridge_plan.h"
#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/codegen/wir_native_pod_emitter.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace wio::codegen
{
    namespace
    {
        struct IdHash
        {
            template <class Id> std::size_t operator()(const Id id) const noexcept
            {
                return id.value();
            }
        };
    } // namespace
    WirNativeBridgePlan WirNativeBridgePlan::build(const wir::lowered::Module& source)
    {
        using namespace wir;
        namespace lowered = wir::lowered;
        WirNativeBridgePlan plan;
        auto& view = plan.module;
        view.name = source.name;
        view.contract = source.contract;
        view.types = source.types;
        // The VM owns application/Entry/export dispatch and module lifecycle.
        // This library exports native registries, not a second Wio SDK module.
        view.contract.kind = ModuleKind::Program;
        view.contract.exports.clear();
        view.contract.callTable.entries.clear();
        view.contract.lifecycle = {};
        view.contract.application.reset();
        view.contract.systems.clear();

        std::unordered_map<FunctionId, const lowered::Function*, IdHash> functions;
        std::unordered_map<GlobalId, const lowered::Global*, IdHash> globals;
        std::unordered_map<TypeId, const ReflectionDescriptor*, IdHash> reflection;
        for (const auto& function : source.functions)
            functions.emplace(function.id, &function);
        for (const auto& global : source.globals)
            globals.emplace(global.id, &global);
        for (const auto& descriptor : source.contract.reflection)
            reflection.emplace(descriptor.type, &descriptor);

        std::vector<TypeId> typeQueue;
        std::vector<FunctionId> functionQueue;
        std::vector<GlobalId> globalQueue;
        std::vector<bool> types(source.types.size(), false);
        std::unordered_set<FunctionId, IdHash> retainedFunctions;
        std::unordered_set<GlobalId, IdHash> retainedGlobals;
        std::unordered_set<std::uint64_t> retainedAttributes;
        const auto type = [&](const TypeId id)
        {
            if (id && source.types.tryGet(id) && !types[id.value()])
            {
                types[id.value()] = true;
                typeQueue.push_back(id);
            }
        };
        const auto function = [&](const FunctionId id)
        {
            if (id && functions.contains(id) && retainedFunctions.insert(id).second)
                functionQueue.push_back(id);
        };
        const auto global = [&](const GlobalId id)
        {
            if (id && globals.contains(id) && retainedGlobals.insert(id).second)
                globalQueue.push_back(id);
        };
        type(source.types.voidType());
        type(source.types.boolType());
        type(source.types.i32Type());
        type(source.types.stringType());
        // Preserve all concrete registry bindings, including host-invocable
        // natives not called by Entry. Removing registry entries is not DCE.
        for (const auto& declaration : source.functions)
            if (declaration.nativeBinding && declaration.genericParameters.empty())
                function(declaration.id);
        const auto objects = WirNativeObjectEmitter::eligibleTypes(source);
        for (const auto id : WirNativeObjectEmitter::hostedHierarchyTypes(source, objects))
            type(id);
        std::vector<TypeId> boundaryShapes;
        std::vector<bool> visitedShapes(source.types.size(), false);
        for (const auto& declaration : source.functions)
            if ((declaration.nativeBinding && declaration.genericParameters.empty()) ||
                (declaration.isMethod && objects.contains(declaration.ownerType)))
            {
                boundaryShapes.push_back(declaration.returnType);
                for (const auto& parameter : declaration.parameters)
                    boundaryShapes.push_back(parameter.type);
            }
        bool anyBoundary = false;
        for (std::size_t index = 0; index < boundaryShapes.size(); ++index)
        {
            const auto id = boundaryShapes[index];
            const auto* shape = source.types.tryGet(id);
            if (!shape || visitedShapes[id.value()])
                continue;
            visitedShapes[id.value()] = true;
            anyBoundary |= shape->kind == TypeKind::Any;
            boundaryShapes.insert(boundaryShapes.end(), shape->arguments.begin(), shape->arguments.end());
        }
        // Dynamic Any payload discovery must inspect VM bodies before pruning.
        // Those bodies do not themselves become C++ roots (not even Entry).
        // Without an Any native boundary, VM-only boxes need no C++ adapters.
        if (anyBoundary)
        {
            std::vector<bool> payloads(source.types.size(), false);
            for (const auto& declaration : source.functions)
                for (const auto& block : declaration.blocks)
                    for (const auto& instruction : block.instructions)
                        if (instruction.opcode == lowered::Opcode::AnyBox ||
                            instruction.opcode == lowered::Opcode::AnyCheckedCast ||
                            instruction.opcode == lowered::Opcode::AnyTypeTest)
                            if (instruction.targetType && source.types.tryGet(instruction.targetType) &&
                                !payloads[instruction.targetType.value()])
                            {
                                payloads[instruction.targetType.value()] = true;
                                plan.dynamicPayloadTypes.push_back(instruction.targetType);
                                type(instruction.targetType);
                            }
        }
        // Physical POD aliases/projections are registry metadata, not ordinary
        // dead Wio types. Retain the qualified layout catalog conservatively.
        for (const auto id : WirNativePodEmitter::eligibleTypes(source))
            type(id);

        std::size_t typeCursor = 0, functionCursor = 0, globalCursor = 0;
        bool attributesChanged = true;
        while (typeCursor < typeQueue.size() || functionCursor < functionQueue.size() ||
               globalCursor < globalQueue.size() || attributesChanged)
        {
            while (typeCursor < typeQueue.size())
            {
                const auto id = typeQueue[typeCursor++];
                const auto& value = source.types.get(id);
                // Usually application scheduling is VM-only. If a retained
                // C++ body actually needs application storage, preserve its
                // whole lifecycle/schedule contract instead of leaving calls
                // to missing generated application helpers.
                if (source.contract.application && source.contract.application->type == id)
                {
                    view.contract.application = source.contract.application;
                    view.contract.systems = source.contract.systems;
                    const auto& application = *source.contract.application;
                    for (const auto entry : {application.construct, application.entry, application.start,
                                             application.update, application.close, application.exit})
                        function(entry);
                    for (const auto& system : source.contract.systems)
                    {
                        type(system.type);
                        function(system.start);
                        function(system.update);
                        function(system.close);
                    }
                    for (const auto& stage : application.stages)
                        for (const auto& run : stage.runs)
                        {
                            type(run.targetType);
                            function(run.function);
                            for (const auto& resource : run.resources)
                                type(resource.type);
                        }
                }
                for (const auto argument : value.arguments)
                    type(argument);
                type(value.extentParameter);
                type(value.enumUnderlyingType);
                for (const auto base : value.baseTypes)
                    type(base);
                for (const auto cast : value.castTypes)
                    type(cast);
                for (const auto& field : value.fields)
                    type(field.type);
                for (const auto& method : value.methods)
                {
                    function(method.function);
                    type(method.returnType);
                    for (const auto parameter : method.parameterTypes)
                        type(parameter);
                }
                for (const auto& dispatch : value.dispatchEntries)
                {
                    type(dispatch.contractType);
                    function(dispatch.implementation);
                }
                function(value.defaultConstructor);
                function(value.destructor);
                function(value.fieldInitializer);
                if (const auto found = reflection.find(id); found != reflection.end())
                    for (const auto& method : found->second->methods)
                        function(method.function);
            }
            while (functionCursor < functionQueue.size())
            {
                const auto& value = *functions.at(functionQueue[functionCursor++]);
                type(value.returnType);
                type(value.callableType);
                type(value.ownerType);
                function(value.genericOrigin);
                for (const auto& parameter : value.parameters)
                    type(parameter.type);
                for (const auto& capture : value.captures)
                    type(capture.type);
                for (const auto argument : value.genericParameters)
                    type(argument);
                for (const auto argument : value.specializationArguments)
                    type(argument);
                if (value.nativeBinding)
                {
                    type(value.nativeBinding->result.type);
                    for (const auto& parameter : value.nativeBinding->parameters)
                        type(parameter.type);
                    for (const auto argument : value.nativeBinding->templateArguments)
                        type(argument);
                }
                if (value.coroutine)
                {
                    type(value.coroutine->resultType);
                    for (const auto& slot : value.coroutine->frameSlots)
                        type(slot.type);
                    for (const auto& state : value.coroutine->states)
                        type(state.resultType);
                }
                for (const auto& block : value.blocks)
                {
                    for (const auto& parameter : block.parameters)
                        type(parameter.type);
                    for (const auto& instruction : block.instructions)
                    {
                        type(instruction.resultType);
                        type(instruction.targetType);
                        for (const auto signature : instruction.signatureTypes)
                            type(signature);
                        for (const auto argument : instruction.genericArguments)
                            type(argument);
                        function(instruction.callee);
                        global(instruction.global);
                    }
                }
            }
            while (globalCursor < globalQueue.size())
            {
                const auto& value = *globals.at(globalQueue[globalCursor++]);
                type(value.type);
                function(value.initializer);
            }
            attributesChanged = false;
            for (const auto& attribute : source.contract.attributes)
            {
                // Module/type/function attributes on retained declarations
                // preserve their executable processors and reflection data.
                const bool target = attribute.targetFunction ? retainedFunctions.contains(attribute.targetFunction)
                                    : attribute.targetType   ? types[attribute.targetType.value()]
                                                             : attribute.targetKind == MetadataTargetKind::Module;
                if (!target || !retainedAttributes.insert(attribute.stableId).second)
                    continue;
                attributesChanged = true;
                for (const auto& argument : attribute.arguments)
                    type(argument.type);
                for (const auto& processor : attribute.processors)
                {
                    type(processor.processorType);
                    type(processor.valueType);
                    function(processor.hookFunction);
                }
            }
        }
        // Stable sparse IDs: never renumber functions, globals or types. Void
        // tombstones live only in this emission view, not Typed/Lowered WIR or
        // bytecode. No surviving edge may refer to a tombstone.
        for (std::uint32_t index = 0; index < types.size(); ++index)
            if (!types[index])
                view.types.getMutable(TypeId{index}) = Type{.kind = TypeKind::Void};
        for (const auto& declaration : source.functions)
            if (retainedFunctions.contains(declaration.id))
                view.functions.push_back(declaration);
        for (const auto& value : source.globals)
            if (retainedGlobals.contains(value.id))
                view.globals.push_back(value);
        std::erase_if(view.contract.attributes,
                      [&](const auto& attribute) { return !retainedAttributes.contains(attribute.stableId); });
        std::erase_if(view.contract.reflection,
                      [&](const auto& descriptor) { return !types[descriptor.type.value()]; });
        return plan;
    }
} // namespace wio::codegen
