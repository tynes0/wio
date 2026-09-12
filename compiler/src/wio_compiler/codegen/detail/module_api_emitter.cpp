#include "module_api_emitter.h"

#include "wio/codegen/cpp_generator.h"

#include "wio/ast/attribute_contract.h"
#include "wio/ast/attribute_queries.h"
#include "wio/ast/declaration_queries.h"
#include "wio/codegen/cpp_identifier.h"
#include "wio/common/filesystem/filesystem.h"
#include "wio/common/operator_overload.h"
#include "wio/common/utility.h"
#include "compiler.h"
#include "wio/common/logger.h"
#include "wio/sema/symbol.h"
#include "wio/sema/scope_lookup.h"
#include "wio/sema/constant_evaluator.h"
#include "wio/sema/generic_support.h"
#include "wio/sema/type_queries.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <unordered_set>
#include <utility>

namespace wio::codegen
{
    namespace
    {
        #include "generator_support.inl"
    }

// Translation-unit-private module ABI emission helper.
// Kept beside the generator because it consumes the generator's output primitives.

namespace detail
{
    ModuleApiEmitter::ModuleApiEmitter(CppGenerator& generator)
        : generator_(generator)
    {
    }

    void ModuleApiEmitter::emit(const std::string& value)
    {
        generator_.emit(value);
    }

    void ModuleApiEmitter::emitLine(const std::string& value)
    {
        generator_.emitLine(value);
    }

    void ModuleApiEmitter::emitGeneratedDirective()
    {
        generator_.emitGeneratedDirective();
    }

    void ModuleApiEmitter::emitTabs()
    {
        for (int index = 0; index < generator_.indentationLevel_; ++index)
            generator_.buffer_ << "    ";
    }

    void ModuleApiEmitter::indent()
    {
        generator_.indent();
    }

    void ModuleApiEmitter::dedent()
    {
        generator_.dedent();
    }

    void ModuleApiEmitter::emit(const Ref<Program>& program)
    {
        if (!program)
            return;

        emitGeneratedDirective();

        ModuleLifecycleFunctions lifecycleFunctions;
        collectModuleLifecycleFunctions(program->statements, lifecycleFunctions);
        const FunctionDeclaration* applicationEntry = findApplicationEntry(program->statements);
        std::vector<ExportedFunctionInfo> exportedFunctions;
        collectExportedFunctions(program->statements, exportedFunctions);
        std::unordered_map<const sema::StructType*, const ObjectDeclaration*> objectDeclarations;
        std::unordered_map<const sema::StructType*, const ComponentDeclaration*> componentDeclarations;
        indexStructDeclarations(program->statements, objectDeclarations, componentDeclarations);
        std::vector<ExportedTypeInfo> exportedTypes;
        collectExportedTypes(program->statements, exportedFunctions, exportedTypes, objectDeclarations, componentDeclarations);

        if (!lifecycleFunctions.hasAny() && exportedFunctions.empty() && applicationEntry == nullptr)
            return;

        std::uint32_t capabilities = 0;
        if (lifecycleFunctions.apiVersion) capabilities |= (1u << 0);
        if (lifecycleFunctions.load) capabilities |= (1u << 1);
        if (lifecycleFunctions.update) capabilities |= (1u << 2);
        if (lifecycleFunctions.unload) capabilities |= (1u << 3);
        if (lifecycleFunctions.saveState) capabilities |= (1u << 4);
        if (lifecycleFunctions.restoreState) capabilities |= (1u << 5);
        capabilities |= (1u << 6); // product version
        capabilities |= (1u << 7); // type metadata v2
        capabilities |= (1u << 8); // Unicode text dynamic fields
        auto hasRetainedAttributes = [](const std::vector<NodePtr<AttributeStatement>>* attributes)
        {
            return attributes && std::ranges::any_of(*attributes, [](const auto& attribute)
            {
                return attribute && attribute->runtimeRetained;
            });
        };
        bool hasRetainedAttributeMetadata = false;
        for (const auto& exportedFunction : exportedFunctions)
            hasRetainedAttributeMetadata = hasRetainedAttributeMetadata ||
                (exportedFunction.declaration && hasRetainedAttributes(&exportedFunction.declaration->attributes));
        for (const auto& exportedType : exportedTypes)
        {
            hasRetainedAttributeMetadata = hasRetainedAttributeMetadata || hasRetainedAttributes(exportedType.attributes);
            for (const auto& field : exportedType.fields)
                hasRetainedAttributeMetadata = hasRetainedAttributeMetadata ||
                    (field.declaration && hasRetainedAttributes(&field.declaration->attributes));
            for (const auto& method : exportedType.methods)
                hasRetainedAttributeMetadata = hasRetainedAttributeMetadata ||
                    (method.declaration && hasRetainedAttributes(&method.declaration->attributes));
        }
        if (hasRetainedAttributeMetadata) capabilities |= (1u << 9); // retained attribute metadata v1
        if (applicationEntry) capabilities |= (1u << 10); // host-driven application ABI v1
        if (applicationEntry && !applicationEntry->applicationStages.empty())
            capabilities |= (1u << 12); // application schedule inspection v1
        auto asyncResultType = [](const ExportedFunctionInfo& exportInfo) -> Ref<sema::Type>
        {
            if (!exportInfo.declaration || !exportInfo.declaration->isAsync || !exportInfo.functionType)
                return nullptr;
            Ref<sema::Type> taskType = unwrapAliasType(exportInfo.functionType->returnType);
            return taskType && taskType->kind() == sema::TypeKind::AsyncTask
                ? taskType.AsFast<sema::AsyncTaskType>()->valueType
                : nullptr;
        };
        std::vector<size_t> asyncExportIndices;
        for (size_t exportIndex = 0; exportIndex < exportedFunctions.size(); ++exportIndex)
        {
            const auto& exportInfo = exportedFunctions[exportIndex];
            Ref<sema::Type> resultType = asyncResultType(exportInfo);
            if (!resultType || getAbiTypeEnumName(resultType) == "WIO_ABI_UNKNOWN")
                continue;
            const bool parametersAreStable = std::ranges::all_of(
                exportInfo.functionType->paramTypes,
                [&](const Ref<sema::Type>& parameterType)
                {
                    return getAbiTypeEnumName(parameterType) != "WIO_ABI_UNKNOWN";
                });
            if (parametersAreStable)
                asyncExportIndices.push_back(exportIndex);
        }
        if (!asyncExportIndices.empty()) capabilities |= (1u << 11); // async task host ABI v1
        std::uint32_t stateSchemaVersion = (lifecycleFunctions.saveState || lifecycleFunctions.restoreState) ? 1u : 0u;

        struct SdkAttributeTable
        {
            size_t count = 0;
            std::string expression = "nullptr";
        };
        auto emitSdkAttributeTable = [&](const std::string& tableName,
                                         const std::vector<NodePtr<AttributeStatement>>* attributes)
            -> SdkAttributeTable
        {
            std::vector<const AttributeStatement*> retained;
            if (attributes)
            {
                for (const auto& attribute : *attributes)
                    if (attribute && attribute->runtimeRetained)
                        retained.push_back(attribute.Get());
            }
            if (retained.empty())
                return {};

            auto phaseExpression = [](const std::string& phase)
            {
                if (phase == "validation") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_VALIDATION");
                if (phase == "pre") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_PRE");
                if (phase == "post") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_POST");
                if (phase == "finally") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_FINALLY");
                if (phase == "around") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_AROUND");
                if (phase == "derive") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_DERIVE");
                return std::string("WIO_MODULE_ATTRIBUTE_PHASE_UNKNOWN");
            };
            for (size_t attributeIndex = 0; attributeIndex < retained.size(); ++attributeIndex)
            {
                const auto* attribute = retained[attributeIndex];
                if (attribute->processorBindings.empty())
                    continue;
                emitLine("static const WioModuleAttributeProcessorDescriptor " + tableName +
                         "_PROCESSORS_" + std::to_string(attributeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t processorIndex = 0; processorIndex < attribute->processorBindings.size(); ++processorIndex)
                {
                    const auto& processor = attribute->processorBindings[processorIndex];
                    const std::string suffix = processorIndex + 1 < attribute->processorBindings.size() ? "," : "";
                    emitLine("{ \"" + common::wioStringToEscapedCppString(processor.canonicalTypeName) +
                             "\", " + phaseExpression(processor.phase) + ", \"" +
                             common::wioStringToEscapedCppString(processor.hookMode) + "\", " +
                             std::to_string(processorIndex) + "u }" + suffix);
                }
                dedent();
                emitLine("};");
            }

            emitLine("static const WioModuleAttributeDescriptor " + tableName + "[] =");
            emitLine("{");
            indent();
            for (size_t attributeIndex = 0; attributeIndex < retained.size(); ++attributeIndex)
            {
                const auto* attribute = retained[attributeIndex];
                std::string arguments;
                for (size_t argumentIndex = 0; argumentIndex < attribute->args.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0) arguments += ", ";
                    if (argumentIndex < attribute->argumentNames.size() && !attribute->argumentNames[argumentIndex].empty())
                        arguments += attribute->argumentNames[argumentIndex] + "=";
                    arguments += attribute->args[argumentIndex].value;
                }
                const std::string origin = attribute->origin == AttributeOrigin::Composed
                    ? "WIO_MODULE_ATTRIBUTE_COMPOSED"
                    : (attribute->origin == AttributeOrigin::Scoped
                        ? "WIO_MODULE_ATTRIBUTE_SCOPED"
                        : "WIO_MODULE_ATTRIBUTE_DIRECT");
                const std::string processorExpression = attribute->processorBindings.empty()
                    ? "nullptr"
                    : tableName + "_PROCESSORS_" + std::to_string(attributeIndex);
                const std::string stableName = attribute->canonicalName.empty()
                    ? attribute->qualifiedName
                    : attribute->canonicalName;
                const std::string suffix = attributeIndex + 1 < retained.size() ? "," : "";
                emitLine("{ \"" + common::wioStringToEscapedCppString(stableName) + "\", \"" +
                         common::wioStringToEscapedCppString(arguments) + "\", " + origin + ", " +
                         std::to_string(attribute->processorBindings.size()) + "u, " + processorExpression + " }" + suffix);
            }
            dedent();
            emitLine("};");
            return { retained.size(), tableName };
        };

        auto isInvokeCompatibleExport = [&](const ExportedFunctionInfo& exportInfo) -> bool
        {
            auto exportFunctionType = exportInfo.functionType;
            if (!exportFunctionType || (exportInfo.declaration && exportInfo.declaration->isAsync))
                return false;

            if (getAbiTypeEnumName(exportFunctionType->returnType) == "WIO_ABI_UNKNOWN")
                return false;

            for (const auto& parameterType : exportFunctionType->paramTypes)
            {
                if (getAbiTypeEnumName(parameterType) == "WIO_ABI_UNKNOWN")
                    return false;
            }

            return true;
        };

        auto emitSyntheticRawExport = [&](const ExportedFunctionInfo& exportInfo)
        {
            auto exportFunctionType = exportInfo.functionType;
            if (!exportFunctionType || exportInfo.syntheticKind == ExportedFunctionInfo::SyntheticKind::None)
                return;


            emitLine(common::formatString(
                "extern \"C\" WIO_EXPORT {} {}(",
                toCppType(exportFunctionType->returnType),
                exportInfo.symbolName
            ));
            for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
            {
                emitTabs();
                emit(common::formatString(
                    "    {} _wio_arg{}",
                    toCppType(exportFunctionType->paramTypes[paramIndex]),
                    paramIndex
                ));
                if (paramIndex + 1 < exportFunctionType->paramTypes.size())
                    emit(",");
                emit("\n");
            }
            emitLine(")");
            emitLine("{");
            indent();

            switch (exportInfo.syntheticKind)
            {
            case ExportedFunctionInfo::SyntheticKind::TypeConstruct:
            {
                std::string constructorArguments;
                for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    if (paramIndex > 0)
                        constructorArguments += ", ";
                    constructorArguments += "_wio_arg" + std::to_string(paramIndex);
                }

                if (exportInfo.ownerIsObject)
                {
                    emitLine(common::formatString(
                        "auto* instance = wio::runtime::Ref<{}>::Create({}).Detach();",
                        exportInfo.ownerCppTypeName,
                        constructorArguments
                    ));
                }
                else
                {
                    emitLine(common::formatString(
                        "auto* instance = new {}({});",
                        exportInfo.ownerCppTypeName,
                        constructorArguments
                    ));
                }
                emitLine("return reinterpret_cast<std::uintptr_t>(instance);");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeDestroy:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                if (exportInfo.ownerIsObject)
                    emitLine("wio::runtime::RefDeleter<" + exportInfo.ownerCppTypeName + ">::Execute(instance);");
                else
                    emitLine("delete instance;");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeFieldGet:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                {
                    if (exportInfo.valueRequiresBridgeCast)
                        emitLine("return static_cast<" + toCppType(exportFunctionType->returnType) + ">(instance->" + exportInfo.memberCppName + ");");
                    else
                        emitLine("return instance->" + exportInfo.memberCppName + ";");
                }
                else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                    emitLine("return reinterpret_cast<std::uintptr_t>(instance->" + exportInfo.memberCppName + ".Get());");
                else
                    emitLine("return reinterpret_cast<std::uintptr_t>(&instance->" + exportInfo.memberCppName + ");");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeFieldSet:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                {
                    if (exportInfo.valueRequiresBridgeCast)
                        emitLine("instance->" + exportInfo.memberCppName + " = static_cast<" + exportInfo.memberCppTypeName + ">(_wio_arg1);");
                    else
                        emitLine("instance->" + exportInfo.memberCppName + " = _wio_arg1;");
                }
                else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                    emitLine("instance->" + exportInfo.memberCppName + " = wio::runtime::Ref<" + exportInfo.memberCppTypeName + ">(reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(_wio_arg1));");
                else
                    emitLine("instance->" + exportInfo.memberCppName + " = *reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(_wio_arg1);");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeMethod:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                std::string callExpression = "instance->" + exportInfo.memberCppName + "(";
                for (size_t paramIndex = 1; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    if (paramIndex > 1)
                        callExpression += ", ";
                    callExpression += "_wio_arg" + std::to_string(paramIndex);
                }
                callExpression += ")";

                if (exportFunctionType->returnType->isVoid())
                    emitLine(callExpression + ";");
                else
                    emitLine("return " + callExpression + ";");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::None:
                break;
            }

            dedent();
            emitLine("}");
        };

        emitLine();
        for (size_t i = 0; i < exportedFunctions.size(); ++i)
        {
            const auto& exportInfo = exportedFunctions[i];
            auto exportFunctionType = exportInfo.functionType;

            emitSyntheticRawExport(exportInfo);

            if (isInvokeCompatibleExport(exportInfo))
            {
                emitLine("static std::int32_t WIO_INVOKE_EXPORT_" + std::to_string(i) + "(const WioValue* args, std::uint32_t argCount, WioValue* outResult)");
                emitLine("{");
                indent();
                emitLine("if (argCount != " + std::to_string(exportFunctionType->paramTypes.size()) + "u) return WIO_INVOKE_BAD_ARGUMENTS;");
                if (!exportFunctionType->paramTypes.empty())
                    emitLine("if (args == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");

                for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    emitLine(
                        "if (args[" + std::to_string(paramIndex) + "].type != " +
                        getAbiTypeEnumName(exportFunctionType->paramTypes[paramIndex]) +
                        ") return WIO_INVOKE_TYPE_MISMATCH;"
                    );
                }

                if (!exportFunctionType->returnType->isVoid())
                    emitLine("if (outResult == nullptr) return WIO_INVOKE_RESULT_REQUIRED;");

                switch (exportInfo.syntheticKind)
                {
                case ExportedFunctionInfo::SyntheticKind::None:
                {
                    std::string callExpression = exportInfo.internalSymbol;
                    if (!exportInfo.templateArguments.empty())
                        callExpression += formatTemplateArgumentList(exportInfo.templateArguments);
                    callExpression += "(";
                    for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                    {
                        if (paramIndex > 0)
                            callExpression += ", ";

                        callExpression += "args[" + std::to_string(paramIndex) + "].value." + getAbiValueFieldName(exportFunctionType->paramTypes[paramIndex]);
                    }
                    callExpression += ")";

                    if (exportFunctionType->returnType->isVoid())
                    {
                        emitLine(callExpression + ";");
                        emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    }
                    else
                    {
                        emitLine("auto result = " + callExpression + ";");
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                    }
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeConstruct:
                {
                    std::string constructorArguments;
                    for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                    {
                        if (paramIndex > 0)
                            constructorArguments += ", ";

                        constructorArguments += "args[" + std::to_string(paramIndex) + "].value." + getAbiValueFieldName(exportFunctionType->paramTypes[paramIndex]);
                    }

                    if (exportInfo.ownerIsObject)
                        emitLine("auto* instance = wio::runtime::Ref<" + exportInfo.ownerCppTypeName + ">::Create(" + constructorArguments + ").Detach();");
                    else
                        emitLine("auto* instance = new " + exportInfo.ownerCppTypeName + "(" + constructorArguments + ");");
                    emitLine("outResult->type = WIO_ABI_USIZE;");
                    emitLine("outResult->value.v_usize = reinterpret_cast<std::uintptr_t>(instance);");
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeDestroy:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                    if (exportInfo.ownerIsObject)
                        emitLine("wio::runtime::RefDeleter<" + exportInfo.ownerCppTypeName + ">::Execute(instance);");
                    else
                        emitLine("delete instance;");
                    emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeFieldGet:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                    if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                    {
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        if (exportInfo.valueRequiresBridgeCast)
                            emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = static_cast<" + toCppType(exportFunctionType->returnType) + ">(instance->" + exportInfo.memberCppName + ");");
                        else
                        {
                            emitLine("auto result = instance->" + exportInfo.memberCppName + ";");
                            emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                        }
                    }
                    else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                    {
                        emitLine("outResult->type = WIO_ABI_USIZE;");
                        emitLine("outResult->value.v_usize = reinterpret_cast<std::uintptr_t>(instance->" + exportInfo.memberCppName + ".Get());");
                    }
                    else
                    {
                        emitLine("outResult->type = WIO_ABI_USIZE;");
                        emitLine("outResult->value.v_usize = reinterpret_cast<std::uintptr_t>(&instance->" + exportInfo.memberCppName + ");");
                    }
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeFieldSet:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                    if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                    {
                        if (exportInfo.valueRequiresBridgeCast)
                            emitLine("instance->" + exportInfo.memberCppName + " = static_cast<" + exportInfo.memberCppTypeName + ">(args[1].value." + getAbiValueFieldName(exportFunctionType->paramTypes[1]) + ");");
                        else
                            emitLine("instance->" + exportInfo.memberCppName + " = args[1].value." + getAbiValueFieldName(exportFunctionType->paramTypes[1]) + ";");
                    }
                    else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                        emitLine("instance->" + exportInfo.memberCppName + " = wio::runtime::Ref<" + exportInfo.memberCppTypeName + ">(reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(args[1].value.v_usize));");
                    else
                        emitLine("instance->" + exportInfo.memberCppName + " = *reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(args[1].value.v_usize);");
                    emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeMethod:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");

                    std::string callExpression = "instance->" + exportInfo.memberCppName + "(";
                    for (size_t paramIndex = 1; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                    {
                        if (paramIndex > 1)
                            callExpression += ", ";

                        callExpression += "args[" + std::to_string(paramIndex) + "].value." + getAbiValueFieldName(exportFunctionType->paramTypes[paramIndex]);
                    }
                    callExpression += ")";

                    if (exportFunctionType->returnType->isVoid())
                    {
                        emitLine(callExpression + ";");
                        emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    }
                    else
                    {
                        emitLine("auto result = " + callExpression + ";");
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                    }
                    break;
                }
                }

                emitLine("return WIO_INVOKE_OK;");
                dedent();
                emitLine("}");
            }

            if (!exportFunctionType->paramTypes.empty())
            {
                emitLine("static const WioAbiType WIO_EXPORT_PARAM_TYPES_" + std::to_string(i) + "[] =");
                emitLine("{");
                indent();
                for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    std::string suffix = (paramIndex + 1 < exportFunctionType->paramTypes.size()) ? "," : "";
                    emitLine(getAbiTypeEnumName(exportFunctionType->paramTypes[paramIndex]) + suffix);
                }
                dedent();
                emitLine("};");
            }
        }

        for (const size_t exportIndex : asyncExportIndices)
        {
            const auto& exportInfo = exportedFunctions[exportIndex];
            const auto functionType = exportInfo.functionType;
            const Ref<sema::Type> resultType = asyncResultType(exportInfo);
            const std::string suffix = std::to_string(exportIndex);
            const std::string stateType = "WioAsyncExportState_" + suffix;
            const std::string resultCppType = toCppType(resultType);

            emitLine("struct " + stateType + " final");
            emitLine("{");
            indent();
            emitLine("std::atomic<std::uint64_t> references{1u};");
            emitLine(toCppType(functionType->returnType) + " task;");
            emitLine("mutable std::mutex errorMutex{};");
            emitLine("mutable std::string lastError{};");
            dedent();
            emitLine("};");
            emitLine();

            emitLine("static void WioAsyncRetain_" + suffix + "(void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("if (auto* state = static_cast<" + stateType + "*>(opaque)) state->references.fetch_add(1u, std::memory_order_relaxed);");
            dedent();
            emitLine("}");
            emitLine("static void WioAsyncRelease_" + suffix + "(void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state != nullptr && state->references.fetch_sub(1u, std::memory_order_acq_rel) == 1u) delete state;");
            dedent();
            emitLine("}");

            emitLine("static WioAsyncTaskStatus WioAsyncStatus_" + suffix + "(const void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const " + stateType + "*>(opaque);");
            emitLine("if (state == nullptr || !state->task.IsReady()) return WIO_ASYNC_TASK_PENDING;");
            emitLine("if (state->task.IsCancelled()) return WIO_ASYNC_TASK_CANCELLED;");
            emitLine("if (state->task.IsFaulted()) return WIO_ASYNC_TASK_FAULTED;");
            emitLine("return WIO_ASYNC_TASK_READY;");
            dedent();
            emitLine("}");

            emitLine("static void WioAsyncCancel_" + suffix + "(void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("try { if (auto* state = static_cast<" + stateType + "*>(opaque)) state->task.Cancel(); }");
            emitLine("catch (...) { }");
            dedent();
            emitLine("}");

            emitLine("static std::int32_t WioAsyncWaitFor_" + suffix + "(void* opaque, std::uint64_t milliseconds) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("const auto initialStatus = WioAsyncStatus_" + suffix + "(state);");
            emitLine("if (initialStatus == WIO_ASYNC_TASK_CANCELLED) return WIO_ASYNC_CANCELLED;");
            emitLine("if (initialStatus == WIO_ASYNC_TASK_FAULTED) return WIO_ASYNC_FAULTED;");
            emitLine("if (!state->task.WaitFor(milliseconds)) return WIO_ASYNC_TIMED_OUT;");
            emitLine("const auto status = WioAsyncStatus_" + suffix + "(state);");
            emitLine("return status == WIO_ASYNC_TASK_CANCELLED ? WIO_ASYNC_CANCELLED : (status == WIO_ASYNC_TASK_FAULTED ? WIO_ASYNC_FAULTED : WIO_ASYNC_OK);");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");

            emitLine("static std::int32_t WioAsyncGetResult_" + suffix + "(void* opaque, WioValue* outResult) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state == nullptr || outResult == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("const auto status = WioAsyncStatus_" + suffix + "(state);");
            emitLine("if (status == WIO_ASYNC_TASK_PENDING) return WIO_ASYNC_NOT_READY;");
            emitLine("if (status == WIO_ASYNC_TASK_CANCELLED) return WIO_ASYNC_CANCELLED;");
            emitLine("if (status == WIO_ASYNC_TASK_FAULTED) return WIO_ASYNC_FAULTED;");
            emitLine("try");
            emitLine("{");
            indent();
            if (resultType->isVoid())
            {
                emitLine("state->task.Get();");
                emitLine("outResult->type = WIO_ABI_VOID;");
            }
            else
            {
                emitLine("auto result = state->task.Get();");
                emitLine("outResult->type = " + getAbiTypeEnumName(resultType) + ";");
                emitLine("outResult->value." + getAbiValueFieldName(resultType) + " = result;");
            }
            emitLine("return WIO_ASYNC_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("std::lock_guard lock(state->errorMutex);");
            emitLine("state->lastError = error.what();");
            emitLine("return state->task.IsCancelled() ? WIO_ASYNC_CANCELLED : WIO_ASYNC_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");

            emitLine("static std::int32_t WioAsyncOnComplete_" + suffix + "(void* opaque, WioAsyncCompletionFn callback, void* userData, WioAsyncCompletionTarget target) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state == nullptr || callback == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("if (target != WIO_ASYNC_COMPLETION_CURRENT_EXECUTOR && target != WIO_ASYNC_COMPLETION_MAIN_EXECUTOR) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("WioAsyncRetain_" + suffix + "(state);");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("state->task.SharedState()->AddCompletionCallback([state, callback, userData, target]");
            emitLine("{");
            indent();
            emitLine("auto deliver = [state, callback, userData]");
            emitLine("{");
            indent();
            emitLine("try { callback(userData, WioAsyncStatus_" + suffix + "(state)); } catch (...) { }");
            emitLine("WioAsyncRelease_" + suffix + "(state);");
            dedent();
            emitLine("};");
            emitLine("if (target == WIO_ASYNC_COMPLETION_MAIN_EXECUTOR)");
            emitLine("{");
            indent();
            emitLine("if (!wio::runtime::DefaultAsyncMainExecutor().Post(std::move(deliver))) deliver();");
            dedent();
            emitLine("}");
            emitLine("else deliver();");
            dedent();
            emitLine("});");
            emitLine("return WIO_ASYNC_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { WioAsyncRelease_" + suffix + "(state); return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");

            emitLine("static const char* WioAsyncLastError_" + suffix + "(const void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const " + stateType + "*>(opaque);");
            emitLine("if (state == nullptr) return \"async task state is null\";");
            emitLine("std::lock_guard lock(state->errorMutex);");
            emitLine("const std::string message = state->task.SharedState()->FailureMessage();");
            emitLine("if (!message.empty()) state->lastError = message;");
            emitLine("return state->lastError.c_str();");
            dedent();
            emitLine("}");

            emitLine("static const WioAsyncTaskOps WIO_ASYNC_OPS_" + suffix + " =");
            emitLine("{");
            indent();
            emitLine("&WioAsyncRetain_" + suffix + ", &WioAsyncRelease_" + suffix + ", &WioAsyncStatus_" + suffix + ",");
            emitLine("&WioAsyncCancel_" + suffix + ", &WioAsyncWaitFor_" + suffix + ", &WioAsyncGetResult_" + suffix + ",");
            emitLine("&WioAsyncOnComplete_" + suffix + ", &WioAsyncLastError_" + suffix);
            dedent();
            emitLine("};");

            emitLine("static std::int32_t WioAsyncInvoke_" + suffix + "(const WioValue* args, std::uint32_t argCount, WioAsyncTaskHandle* outTask) noexcept");
            emitLine("{");
            indent();
            emitLine("if (outTask == nullptr || argCount != " + std::to_string(functionType->paramTypes.size()) + "u) return WIO_ASYNC_BAD_ARGUMENTS;");
            if (!functionType->paramTypes.empty())
                emitLine("if (args == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
            {
                emitLine("if (args[" + std::to_string(parameterIndex) + "].type != " +
                         getAbiTypeEnumName(functionType->paramTypes[parameterIndex]) + ") return WIO_ASYNC_TYPE_MISMATCH;");
            }
            emitLine("try");
            emitLine("{");
            indent();
            std::string callExpression = exportInfo.internalSymbol;
            if (!exportInfo.templateArguments.empty())
                callExpression += formatTemplateArgumentList(exportInfo.templateArguments);
            callExpression += "(";
            for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
            {
                if (parameterIndex > 0)
                    callExpression += ", ";
                callExpression += "args[" + std::to_string(parameterIndex) + "].value." +
                    getAbiValueFieldName(functionType->paramTypes[parameterIndex]);
            }
            callExpression += ")";
            emitLine("auto* state = new " + stateType + "{1u, " + callExpression + "};");
            emitLine("*outTask = { state, &WIO_ASYNC_OPS_" + suffix + ", " + getAbiTypeEnumName(resultType) + " };");
            emitLine("return WIO_ASYNC_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();
        }

        std::vector<SdkAttributeTable> exportAttributeTables;
        exportAttributeTables.reserve(exportedFunctions.size());
        for (size_t exportIndex = 0; exportIndex < exportedFunctions.size(); ++exportIndex)
        {
            const auto* declaration = exportedFunctions[exportIndex].declaration;
            exportAttributeTables.push_back(emitSdkAttributeTable(
                "WIO_MODULE_EXPORT_ATTRIBUTES_" + std::to_string(exportIndex),
                declaration ? &declaration->attributes : nullptr));
        }

        if (!exportedFunctions.empty())
        {
            emitLine("static const WioModuleExport WIO_MODULE_EXPORTS[] =");
            emitLine("{");
            indent();
            for (size_t i = 0; i < exportedFunctions.size(); ++i)
            {
                const auto& exportInfo = exportedFunctions[i];
                auto exportFunctionType = exportInfo.functionType;
                const std::string invokeExpr = isInvokeCompatibleExport(exportInfo)
                    ? ("&WIO_INVOKE_EXPORT_" + std::to_string(i))
                    : "nullptr";
                std::string paramTypesExpr = exportFunctionType->paramTypes.empty()
                    ? "nullptr"
                    : ("WIO_EXPORT_PARAM_TYPES_" + std::to_string(i));
                std::string suffix = (i + 1 < exportedFunctions.size()) ? "," : "";
                const auto& attributeTable = exportAttributeTables[i];

                emitLine(common::formatString(
                    R"({{ "{}", "{}", {}, {}u, {}, {}, reinterpret_cast<const void*>(&{}), {}u, {} }}{})",
                    common::wioStringToEscapedCppString(exportInfo.logicalName),
                    common::wioStringToEscapedCppString(exportInfo.symbolName),
                    getAbiTypeEnumName(exportFunctionType->returnType),
                    exportFunctionType->paramTypes.size(),
                    paramTypesExpr,
                    invokeExpr,
                    exportInfo.symbolName,
                    attributeTable.count,
                    attributeTable.expression,
                    suffix
                ));
            }
            dedent();
            emitLine("};");
        }

        std::string asyncHostDescriptorExpression = "nullptr";
        if (!asyncExportIndices.empty())
        {
            emitLine("static const WioModuleAsyncExport WIO_MODULE_ASYNC_EXPORTS[] =");
            emitLine("{");
            indent();
            for (size_t asyncIndex = 0; asyncIndex < asyncExportIndices.size(); ++asyncIndex)
            {
                const size_t exportIndex = asyncExportIndices[asyncIndex];
                const auto& exportInfo = exportedFunctions[exportIndex];
                const Ref<sema::Type> resultType = asyncResultType(exportInfo);
                const std::string parameterTypes = exportInfo.functionType->paramTypes.empty()
                    ? "nullptr"
                    : "WIO_EXPORT_PARAM_TYPES_" + std::to_string(exportIndex);
                const std::string tableSuffix = asyncIndex + 1 < asyncExportIndices.size() ? "," : "";
                emitLine("{ \"" + common::wioStringToEscapedCppString(exportInfo.logicalName) + "\", " +
                         getAbiTypeEnumName(resultType) + ", " +
                         std::to_string(exportInfo.functionType->paramTypes.size()) + "u, " + parameterTypes +
                         ", &WioAsyncInvoke_" + std::to_string(exportIndex) + " }" + tableSuffix);
            }
            dedent();
            emitLine("};");
            emitLine("static void WioAsyncHostBindMain() noexcept { try { wio::runtime::BindAsyncMainExecutor(); } catch (...) { } }");
            emitLine("static std::uint64_t WioAsyncHostPumpMain() noexcept { try { return wio::runtime::DrainAsyncMainExecutor(); } catch (...) { return 0u; } }");
            emitLine("static std::uint64_t WioAsyncHostPendingMain() noexcept { return wio::runtime::AsyncMainPendingCount(); }");
            emitLine("static void WioAsyncHostRequestShutdown() noexcept { wio::runtime::ShutdownAsyncRuntime(); }");
            emitLine("static const WioAsyncHostDescriptor WIO_MODULE_ASYNC_HOST =");
            emitLine("{");
            indent();
            emitLine("0u, 0u, &WioAsyncHostBindMain, &WioAsyncHostPumpMain,");
            emitLine("&WioAsyncHostPendingMain, &WioAsyncHostRequestShutdown");
            dedent();
            emitLine("};");
            asyncHostDescriptorExpression = "&WIO_MODULE_ASYNC_HOST";
        }

        std::vector<size_t> commandExportIndices;
        std::vector<size_t> eventExportIndices;
        for (size_t i = 0; i < exportedFunctions.size(); ++i)
        {
            if (exportedFunctions[i].commandName.has_value())
                commandExportIndices.push_back(i);
            if (exportedFunctions[i].eventName.has_value())
                eventExportIndices.push_back(i);
        }

        if (!commandExportIndices.empty())
        {
            emitLine("static const WioModuleCommand WIO_MODULE_COMMANDS[] =");
            emitLine("{");
            indent();
            for (size_t i = 0; i < commandExportIndices.size(); ++i)
            {
                size_t exportIndex = commandExportIndices[i];
                std::string suffix = (i + 1 < commandExportIndices.size()) ? "," : "";
                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedFunctions[exportIndex].commandName.value()) +
                    "\", &WIO_MODULE_EXPORTS[" + std::to_string(exportIndex) + "] }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        if (!eventExportIndices.empty())
        {
            emitLine("static const WioModuleEventHook WIO_MODULE_EVENT_HOOKS[] =");
            emitLine("{");
            indent();
            for (size_t i = 0; i < eventExportIndices.size(); ++i)
            {
                size_t exportIndex = eventExportIndices[i];
                std::string suffix = (i + 1 < eventExportIndices.size()) ? "," : "";
                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedFunctions[exportIndex].logicalName) +
                    "\", \"" + common::wioStringToEscapedCppString(exportedFunctions[exportIndex].eventName.value()) +
                    "\", &WIO_MODULE_EXPORTS[" + std::to_string(exportIndex) + "] }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        struct TypeDescriptorEmissionInfo
        {
            struct EnumMemberInfo
            {
                std::string name;
                std::string valueExpr;
            };

            std::string displayName;
            std::string logicalTypeName;
            std::string kindExpr = "WIO_MODULE_TYPE_DESC_UNKNOWN";
            std::string abiExpr = "WIO_ABI_UNKNOWN";
            std::uint64_t staticArraySize = 0u;
            std::optional<size_t> elementIndex;
            std::optional<size_t> keyIndex;
            std::optional<size_t> valueIndex;
            std::optional<size_t> returnIndex;
            std::vector<size_t> parameterIndices;
            std::vector<size_t> genericArgumentIndices;
            std::vector<EnumMemberInfo> enumMembers;
            std::optional<size_t> constValueTypeIndex;
            std::optional<std::string> constValue;
        };

        std::vector<TypeDescriptorEmissionInfo> emittedTypeDescriptors;
        std::unordered_map<std::string, size_t> emittedTypeDescriptorIndices;

        auto getLogicalTypeName = [](const Ref<sema::StructType>& structType) -> std::string
        {
            if (!structType)
                return {};

            return structType->scopePath.empty()
                ? structType->name
                : common::formatString("{}::{}", structType->scopePath, structType->name);
        };

        std::unordered_map<std::string, const EnumDeclaration*> enumDeclarationsByLogicalName;
        std::unordered_map<std::string, const FlagsetDeclaration*> flagsetDeclarationsByLogicalName;

        std::function<void(const std::vector<NodePtr<Statement>>&, const std::string&)> collectEnumLikeDeclarations =
            [&](const std::vector<NodePtr<Statement>>& nodes, const std::string& scopePath)
        {
            for (const auto& stmt : nodes)
            {
                if (!stmt)
                    continue;

                if (const auto* realmDecl = stmt->as<RealmDeclaration>())
                {
                    const std::string nextScopePath = scopePath.empty()
                        ? realmDecl->name->token.value
                        : common::formatString("{}::{}", scopePath, realmDecl->name->token.value);
                    collectEnumLikeDeclarations(realmDecl->statements, nextScopePath);
                    continue;
                }

                if (const auto* enumDecl = stmt->as<EnumDeclaration>())
                {
                    const std::string logicalName = scopePath.empty()
                        ? enumDecl->name->token.value
                        : common::formatString("{}::{}", scopePath, enumDecl->name->token.value);
                    enumDeclarationsByLogicalName.emplace(logicalName, enumDecl);
                    continue;
                }

                if (const auto* flagsetDecl = stmt->as<FlagsetDeclaration>())
                {
                    const std::string logicalName = scopePath.empty()
                        ? flagsetDecl->name->token.value
                        : common::formatString("{}::{}", scopePath, flagsetDecl->name->token.value);
                    flagsetDeclarationsByLogicalName.emplace(logicalName, flagsetDecl);
                }
            }
        };
        collectEnumLikeDeclarations(program->statements, "");

        auto getEnumUnderlyingAbiExpr = [](const std::vector<NodePtr<AttributeStatement>>& attributes,
                                           std::string_view fallbackAbiExpr) -> std::string
        {
            auto typeArgs = getFirstAttributeArgs(attributes, Attribute::Type);
            if (typeArgs.empty())
                return std::string(fallbackAbiExpr);

            switch (typeArgs[0].type)
            {
            case TokenType::kwI8: return "WIO_ABI_I8";
            case TokenType::kwU8: return "WIO_ABI_U8";
            case TokenType::kwI16: return "WIO_ABI_I16";
            case TokenType::kwU16: return "WIO_ABI_U16";
            case TokenType::kwI32: return "WIO_ABI_I32";
            case TokenType::kwU32: return "WIO_ABI_U32";
            case TokenType::kwI64: return "WIO_ABI_I64";
            case TokenType::kwU64: return "WIO_ABI_U64";
            default: return std::string(fallbackAbiExpr);
            }
        };

        std::function<size_t(const Ref<sema::Type>&)> ensureTypeDescriptor = [&](const Ref<sema::Type>& type) -> size_t
        {
            const std::string displayName = type ? type->toString() : std::string("<unknown>");
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            std::string key = displayName;
            if (resolvedType && resolvedType->kind() == sema::TypeKind::ConstValue)
            {
                const auto constValueType = resolvedType.AsFast<sema::ConstValueType>();
                key = "const<" + (constValueType->valueType ? constValueType->valueType->toString() : std::string("<unknown>")) +
                    ">:" + displayName;
            }
            if (auto it = emittedTypeDescriptorIndices.find(key); it != emittedTypeDescriptorIndices.end())
                return it->second;

            TypeDescriptorEmissionInfo info;
            info.displayName = displayName;

            if (!resolvedType)
            {
                const size_t index = emittedTypeDescriptors.size();
                emittedTypeDescriptorIndices.emplace(key, index);
                emittedTypeDescriptors.push_back(std::move(info));
                return index;
            }

            switch (resolvedType->kind())
            {
            case sema::TypeKind::ConstValue:
            {
                const auto constValueType = resolvedType.AsFast<sema::ConstValueType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_CONST_VALUE";
                info.constValueTypeIndex = ensureTypeDescriptor(constValueType->valueType);
                info.constValue = constValueType->value;
                info.displayName = "const " + emittedTypeDescriptors[*info.constValueTypeIndex].displayName +
                    " = " + constValueType->toString();
                break;
            }
            case sema::TypeKind::Nullable:
            {
                auto nullableType = resolvedType.AsFast<sema::NullableType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_NULLABLE";
                info.abiExpr = getAbiTypeEnumName(nullableType->valueType);
                info.elementIndex = ensureTypeDescriptor(nullableType->valueType);
                info.displayName = emittedTypeDescriptors[*info.elementIndex].displayName + "?";
                break;
            }
            case sema::TypeKind::Primitive:
            {
                const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (primitiveName == "string")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_STRING";
                else if (primitiveName == "text")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_TEXT";
                else if (primitiveName == "opaque")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OPAQUE";
                else if (primitiveName == "any")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_ANY";
                else
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_PRIMITIVE";
                info.abiExpr = getAbiTypeEnumName(resolvedType);
                break;
            }
            case sema::TypeKind::AsyncTask:
            {
                auto taskType = resolvedType.AsFast<sema::AsyncTaskType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_ASYNC_TASK";
                info.elementIndex = ensureTypeDescriptor(taskType->valueType);
                info.displayName = "coroutine<" + emittedTypeDescriptors[*info.elementIndex].displayName + ">";
                break;
            }
            case sema::TypeKind::Array:
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                info.kindExpr = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? "WIO_MODULE_TYPE_DESC_STATIC_ARRAY"
                    : "WIO_MODULE_TYPE_DESC_DYNAMIC_ARRAY";
                info.staticArraySize = static_cast<std::uint64_t>(arrayType->size);
                info.elementIndex = ensureTypeDescriptor(arrayType->elementType);
                const std::string elementName = emittedTypeDescriptors[*info.elementIndex].displayName;
                info.displayName = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? ("[" + elementName + "; " + std::to_string(info.staticArraySize) + "]")
                    : (elementName + "[]");
                break;
            }
            case sema::TypeKind::Dictionary:
            {
                auto dictType = resolvedType.AsFast<sema::DictionaryType>();
                info.kindExpr = dictType->isOrdered ? "WIO_MODULE_TYPE_DESC_TREE" : "WIO_MODULE_TYPE_DESC_DICT";
                info.keyIndex = ensureTypeDescriptor(dictType->keyType);
                info.valueIndex = ensureTypeDescriptor(dictType->valueType);
                info.displayName = std::string(dictType->isOrdered ? "Tree<" : "Dict<") +
                    emittedTypeDescriptors[*info.keyIndex].displayName + ", " +
                    emittedTypeDescriptors[*info.valueIndex].displayName + ">";
                break;
            }
            case sema::TypeKind::Function:
            {
                auto functionType = resolvedType.AsFast<sema::FunctionType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_FUNCTION";
                info.returnIndex = ensureTypeDescriptor(functionType->returnType);
                info.parameterIndices.reserve(functionType->paramTypes.size());
                for (const auto& parameterType : functionType->paramTypes)
                    info.parameterIndices.push_back(ensureTypeDescriptor(parameterType));
                info.displayName = "fn(";
                for (size_t parameterIndex = 0; parameterIndex < info.parameterIndices.size(); ++parameterIndex)
                {
                    if (parameterIndex > 0)
                        info.displayName += ", ";
                    info.displayName += emittedTypeDescriptors[info.parameterIndices[parameterIndex]].displayName;
                }
                info.displayName += ") -> " + emittedTypeDescriptors[*info.returnIndex].displayName;
                break;
            }
            case sema::TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                info.logicalTypeName = getLogicalTypeName(structType);
                const auto genericPrimaryType = structType->genericPrimaryType.Lock();
                const auto& genericParameterTypes = !structType->genericParameterTypes.empty()
                    ? structType->genericParameterTypes
                    : (genericPrimaryType ? genericPrimaryType->genericParameterTypes : structType->genericParameterTypes);
                info.genericArgumentIndices.reserve(structType->genericArguments.size());
                for (size_t genericIndex = 0; genericIndex < structType->genericArguments.size(); ++genericIndex)
                {
                    Ref<sema::Type> genericArgument = structType->genericArguments[genericIndex];
                    if (genericArgument && genericArgument->kind() == sema::TypeKind::ConstValue &&
                        genericIndex < genericParameterTypes.size())
                    {
                        auto parameterType = unwrapAliasType(genericParameterTypes[genericIndex]);
                        if (parameterType && parameterType->kind() == sema::TypeKind::ConstGenericParameter)
                        {
                            const auto value = genericArgument.AsFast<sema::ConstValueType>();
                            const auto parameter = parameterType.AsFast<sema::ConstGenericParameterType>();
                            genericArgument = Compiler::get().getTypeContext().getOrCreateConstValueType(
                                value->value,
                                parameter->valueType
                            );
                        }
                    }
                    info.genericArgumentIndices.push_back(ensureTypeDescriptor(genericArgument));
                }
                if (!info.logicalTypeName.empty())
                {
                    info.displayName = info.logicalTypeName;
                    if (!info.genericArgumentIndices.empty())
                    {
                        info.displayName += "<";
                        for (size_t argumentIndex = 0; argumentIndex < info.genericArgumentIndices.size(); ++argumentIndex)
                        {
                            if (argumentIndex > 0)
                                info.displayName += ", ";
                            info.displayName += emittedTypeDescriptors[info.genericArgumentIndices[argumentIndex]].displayName;
                        }
                        info.displayName += ">";
                    }
                }
                if (structType->isEnum)
                {
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_ENUM";
                    const auto declarationIt = enumDeclarationsByLogicalName.find(info.logicalTypeName);
                    const auto* declaration = declarationIt != enumDeclarationsByLogicalName.end() ? declarationIt->second : nullptr;
                    info.abiExpr = structType->enumUnderlyingType
                        ? getAbiTypeEnumName(structType->enumUnderlyingType)
                        : (declaration ? getEnumUnderlyingAbiExpr(declaration->attributes, "WIO_ABI_I32") : "WIO_ABI_I32");

                    const std::string enumCppName = mangleStructTypeName(structType);
                    if (declaration)
                    {
                        info.enumMembers.reserve(declaration->members.size());
                        for (const auto& member : declaration->members)
                        {
                            info.enumMembers.push_back({
                                member.name->token.value,
                                "WioMakeAbiIntegerValue(" + info.abiExpr +
                                    ", static_cast<std::uint64_t>(static_cast<std::underlying_type_t<" + enumCppName + ">>(" +
                                    enumCppName + "::" + member.name->token.value + ")))"
                            });
                        }
                    }
                }
                else if (structType->isFlagset)
                {
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_FLAGSET";
                    const auto declarationIt = flagsetDeclarationsByLogicalName.find(info.logicalTypeName);
                    const auto* declaration = declarationIt != flagsetDeclarationsByLogicalName.end() ? declarationIt->second : nullptr;
                    info.abiExpr = structType->enumUnderlyingType
                        ? getAbiTypeEnumName(structType->enumUnderlyingType)
                        : (declaration ? getEnumUnderlyingAbiExpr(declaration->attributes, "WIO_ABI_U32") : "WIO_ABI_U32");

                    const std::string flagsetCppName = mangleStructTypeName(structType);
                    if (declaration)
                    {
                        info.enumMembers.reserve(declaration->members.size());
                        for (const auto& member : declaration->members)
                        {
                            info.enumMembers.push_back({
                                member.name->token.value,
                                "WioMakeAbiIntegerValue(" + info.abiExpr +
                                    ", static_cast<std::uint64_t>(static_cast<std::underlying_type_t<" + flagsetCppName + ">>(" +
                                    flagsetCppName + "::" + member.name->token.value + ")))"
                            });
                        }
                    }
                }
                else if (structType->isInterface)
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_INTERFACE";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Option")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OPTION";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Result")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_RESULT";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "ResultUnit")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_UNIT";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Tuple")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_TUPLE";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Queue")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_QUEUE";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "UnorderedSet")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_UNORDERED_SET";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "OrderedSet")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_ORDERED_SET";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Span")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_SPAN";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "ByteBuffer")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_BYTE_BUFFER";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "box")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_BOX";
                else if (objectDeclarations.contains(structType.Get()))
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OBJECT";
                else if (componentDeclarations.contains(structType.Get()))
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_COMPONENT";
                else if (!structType->genericArguments.empty())
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_GENERIC_INSTANCE";
                else
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OPAQUE";
                break;
            }
            default:
                info.kindExpr = "WIO_MODULE_TYPE_DESC_UNKNOWN";
                break;
            }

            const size_t index = emittedTypeDescriptors.size();
            emittedTypeDescriptorIndices.emplace(key, index);
            emittedTypeDescriptors.push_back(std::move(info));
            return index;
        };

        for (const auto& exportedType : exportedTypes)
        {
            for (const auto& field : exportedType.fields)
                ensureTypeDescriptor(field.fieldType);
        }

            if (!emittedTypeDescriptors.empty())
            {
            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
                emitLine("extern const WioModuleTypeDescriptor WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) + ";");

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                if (descriptor.parameterIndices.empty())
                    continue;

                emitLine("static const WioModuleTypeDescriptor* WIO_TYPE_DESCRIPTOR_PARAMS_" + std::to_string(descriptorIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t parameterIndex = 0; parameterIndex < descriptor.parameterIndices.size(); ++parameterIndex)
                {
                    const std::string suffix = (parameterIndex + 1 < descriptor.parameterIndices.size()) ? "," : "";
                    emitLine("&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptor.parameterIndices[parameterIndex]) + suffix);
                }
                dedent();
                emitLine("};");
            }

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                if (descriptor.genericArgumentIndices.empty())
                    continue;

                emitLine("static const WioModuleTypeDescriptor* WIO_TYPE_DESCRIPTOR_GENERIC_ARGS_" + std::to_string(descriptorIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t argumentIndex = 0; argumentIndex < descriptor.genericArgumentIndices.size(); ++argumentIndex)
                {
                    const std::string suffix = (argumentIndex + 1 < descriptor.genericArgumentIndices.size()) ? "," : "";
                    emitLine("&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptor.genericArgumentIndices[argumentIndex]) + suffix);
                }
                dedent();
                emitLine("};");
            }

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                if (descriptor.enumMembers.empty())
                    continue;

                emitLine("static const WioModuleEnumMemberDescriptor WIO_TYPE_DESCRIPTOR_ENUM_MEMBERS_" + std::to_string(descriptorIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t memberIndex = 0; memberIndex < descriptor.enumMembers.size(); ++memberIndex)
                {
                    const auto& member = descriptor.enumMembers[memberIndex];
                    const std::string suffix = (memberIndex + 1 < descriptor.enumMembers.size()) ? "," : "";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(member.name) + "\", " + member.valueExpr + " }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                const std::string logicalTypeExpr = descriptor.logicalTypeName.empty()
                    ? "nullptr"
                    : ("\"" + common::wioStringToEscapedCppString(descriptor.logicalTypeName) + "\"");
                const std::string elementExpr = descriptor.elementIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.elementIndex))
                    : "nullptr";
                const std::string keyExpr = descriptor.keyIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.keyIndex))
                    : "nullptr";
                const std::string valueExpr = descriptor.valueIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.valueIndex))
                    : "nullptr";
                const std::string returnExpr = descriptor.returnIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.returnIndex))
                    : "nullptr";
                const std::string paramExpr = descriptor.parameterIndices.empty()
                    ? "nullptr"
                    : ("WIO_TYPE_DESCRIPTOR_PARAMS_" + std::to_string(descriptorIndex));
                const std::string enumMembersExpr = descriptor.enumMembers.empty()
                    ? "nullptr"
                    : ("WIO_TYPE_DESCRIPTOR_ENUM_MEMBERS_" + std::to_string(descriptorIndex));
                const std::string genericArgumentsExpr = descriptor.genericArgumentIndices.empty()
                    ? "nullptr"
                    : ("WIO_TYPE_DESCRIPTOR_GENERIC_ARGS_" + std::to_string(descriptorIndex));
                const std::string constValueTypeExpr = descriptor.constValueTypeIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.constValueTypeIndex))
                    : "nullptr";
                const std::string constValueExpr = !descriptor.constValue.has_value()
                    ? "nullptr"
                    : ("\"" + common::wioStringToEscapedCppString(*descriptor.constValue) + "\"");

                emitLine(
                    "const WioModuleTypeDescriptor WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                    " = { \"" + common::wioStringToEscapedCppString(descriptor.displayName) +
                    "\", " + logicalTypeExpr +
                    ", " + descriptor.kindExpr +
                    ", " + descriptor.abiExpr +
                    ", " + std::to_string(descriptor.staticArraySize) + "ull, " +
                    elementExpr + ", " + keyExpr + ", " + valueExpr + ", " + returnExpr +
                    ", " + std::to_string(descriptor.parameterIndices.size()) + "u, " + paramExpr +
                    ", " + std::to_string(descriptor.enumMembers.size()) + "u, " + enumMembersExpr +
                    ", WioStableTypeId(\"" + common::wioStringToEscapedCppString(descriptor.displayName) + "\")" +
                    ", " + std::to_string(descriptor.genericArgumentIndices.size()) + "u, " + genericArgumentsExpr +
                    ", " + constValueTypeExpr + ", " + constValueExpr + " };"
                );
                }
            }

            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
                const auto& exportedType = exportedTypes[typeIndex];
                for (const auto& field : exportedType.fields)
                {
                    if (!field.dynamicGetterSymbol.has_value())
                        continue;

                    const size_t descriptorIndex = ensureTypeDescriptor(field.fieldType);
                    Ref<sema::Type> dynamicFieldType = unwrapAliasType(field.fieldType);
                    const bool isTextField = dynamicFieldType && dynamicFieldType->kind() == sema::TypeKind::Primitive &&
                        dynamicFieldType.AsFast<sema::PrimitiveType>()->name == "text";
                    const bool isOptionField = getStdValueStructType(dynamicFieldType, "Option") != nullptr;
                    const bool isResultField = getStdValueStructType(dynamicFieldType, "Result") != nullptr;
                    const bool isUnitField = getStdValueStructType(dynamicFieldType, "ResultUnit") != nullptr;
                    const bool isSpanField = getStdValueStructType(dynamicFieldType, "Span") != nullptr;
                    const bool isByteBufferField = getStdValueStructType(dynamicFieldType, "ByteBuffer") != nullptr;
                    const bool isTupleField = getStdValueStructType(dynamicFieldType, "Tuple") != nullptr;
                    const bool isSequenceContainerField =
                        getStdValueStructType(dynamicFieldType, "Queue") != nullptr ||
                        getStdValueStructType(dynamicFieldType, "UnorderedSet") != nullptr ||
                        getStdValueStructType(dynamicFieldType, "OrderedSet") != nullptr;
                    const bool isPortableBridgeField = isOptionField || isResultField || isUnitField || isSpanField ||
                        isByteBufferField || isTupleField || isSequenceContainerField ||
                        (dynamicFieldType && (dynamicFieldType->kind() == sema::TypeKind::Array ||
                                              dynamicFieldType->kind() == sema::TypeKind::Dictionary));
                    const auto portablePayloadType = isPortableBridgeField
                        ? getSdkDynamicBridgeCppType(dynamicFieldType)
                        : std::optional<std::string>{};
                    const auto portableGetterExpression = isPortableBridgeField
                        ? makeSdkDynamicToHostExpression("instance->" + field.memberCppName, dynamicFieldType)
                        : std::optional<std::string>{};
                    emitLine(
                        "static WioErasedValue* " + *field.dynamicGetterSymbol + "(std::uintptr_t handle)"
                    );
                    emitLine("{");
                    indent();
                    emitLine("auto* instance = reinterpret_cast<" + exportedType.cppTypeName + "*>(handle);");
                    emitLine("if (instance == nullptr) return nullptr;");
                    if (isTextField)
                    {
                        emitLine(
                            std::string("return new WioErasedValueModel<std::string>(") +
                            "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                            ", instance->" + field.memberCppName + ".Utf8());"
                        );
                    }
                    else if (isPortableBridgeField && portablePayloadType.has_value() && portableGetterExpression.has_value())
                    {
                        emitLine(
                            "return new WioErasedValueModel<" + *portablePayloadType + ">(" +
                            "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                            ", " + *portableGetterExpression + ");"
                        );
                    }
                    else
                    {
                        emitLine(
                            "return new WioErasedValueModel<" + field.memberCppTypeName + ">(" +
                            "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                            ", instance->" + field.memberCppName + ");"
                        );
                    }
                    dedent();
                    emitLine("}");
                    emitLine();

                    if (field.dynamicSetterSymbol.has_value())
                    {
                        emitLine(
                            "static std::int32_t " + *field.dynamicSetterSymbol +
                            "(std::uintptr_t handle, const WioErasedValue* value)"
                        );
                        emitLine("{");
                        indent();
                        emitLine("auto* instance = reinterpret_cast<" + exportedType.cppTypeName + "*>(handle);");
                        emitLine("if (instance == nullptr || value == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                        if (isTextField)
                            emitLine("auto* typedValue = dynamic_cast<const WioErasedValueModel<std::string>*>(value);");
                        else if (isPortableBridgeField && portablePayloadType.has_value())
                            emitLine("auto* typedValue = dynamic_cast<const WioErasedValueModel<" + *portablePayloadType + ">*>(value);");
                        else
                            emitLine(
                                "auto* typedValue = dynamic_cast<const WioErasedValueModel<" + field.memberCppTypeName + ">*>(value);"
                            );
                        emitLine("if (typedValue == nullptr) return WIO_INVOKE_TYPE_MISMATCH;");
                        if (isTextField)
                            emitLine("instance->" + field.memberCppName + " = wio::runtime::Text::FromUtf8(typedValue->value);");
                        else if (isPortableBridgeField)
                        {
                            const auto setterExpression = makeSdkDynamicFromHostExpression("typedValue->value", dynamicFieldType);
                            if (setterExpression.has_value())
                                emitLine("instance->" + field.memberCppName + " = " + *setterExpression + ";");
                            else
                                emitLine("return WIO_INVOKE_TYPE_MISMATCH;");
                        }
                        else
                            emitLine("instance->" + field.memberCppName + " = typedValue->value;");
                        emitLine("return WIO_INVOKE_OK;");
                        dedent();
                        emitLine("}");
                        emitLine();
                    }
                }
            }

            std::vector<SdkAttributeTable> typeAttributeTables;
            typeAttributeTables.reserve(exportedTypes.size());
            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
            const auto& exportedType = exportedTypes[typeIndex];
            const auto typeAttributeTable = emitSdkAttributeTable(
                "WIO_MODULE_TYPE_ATTRIBUTES_" + std::to_string(typeIndex),
                exportedType.attributes);
            typeAttributeTables.push_back(typeAttributeTable);
            std::vector<SdkAttributeTable> fieldAttributeTables;
            fieldAttributeTables.reserve(exportedType.fields.size());
            for (size_t fieldIndex = 0; fieldIndex < exportedType.fields.size(); ++fieldIndex)
            {
                const auto* declaration = exportedType.fields[fieldIndex].declaration;
                fieldAttributeTables.push_back(emitSdkAttributeTable(
                    "WIO_MODULE_TYPE_FIELD_ATTRIBUTES_" + std::to_string(typeIndex) + "_" + std::to_string(fieldIndex),
                    declaration ? &declaration->attributes : nullptr));
            }
            std::vector<SdkAttributeTable> methodAttributeTables;
            methodAttributeTables.reserve(exportedType.methods.size());
            for (size_t methodIndex = 0; methodIndex < exportedType.methods.size(); ++methodIndex)
            {
                const auto* declaration = exportedType.methods[methodIndex].declaration;
                methodAttributeTables.push_back(emitSdkAttributeTable(
                    "WIO_MODULE_TYPE_METHOD_ATTRIBUTES_" + std::to_string(typeIndex) + "_" + std::to_string(methodIndex),
                    declaration ? &declaration->attributes : nullptr));
            }

            if (!exportedType.constructors.empty())
            {
                emitLine("static const WioModuleConstructor WIO_MODULE_TYPE_CONSTRUCTORS_" + std::to_string(typeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t constructorIndex = 0; constructorIndex < exportedType.constructors.size(); ++constructorIndex)
                {
                    const auto& constructor = exportedType.constructors[constructorIndex];
                    const std::string suffix = (constructorIndex + 1 < exportedType.constructors.size()) ? "," : "";
                    emitLine(
                        "{ &WIO_MODULE_EXPORTS[" + std::to_string(constructor.exportIndex) + "] }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }

            if (!exportedType.fields.empty())
            {
                emitLine("static const WioModuleField WIO_MODULE_TYPE_FIELDS_" + std::to_string(typeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t fieldIndex = 0; fieldIndex < exportedType.fields.size(); ++fieldIndex)
                {
                    const auto& field = exportedType.fields[fieldIndex];
                    const std::uint32_t flags = 1u | (field.isReadOnly ? 4u : 2u);
                    const std::string suffix = (fieldIndex + 1 < exportedType.fields.size()) ? "," : "";
                    const std::string accessModifierExpr =
                        field.accessModifier == AccessModifier::Protected ? "WIO_MODULE_ACCESS_PROTECTED" :
                        field.accessModifier == AccessModifier::Private ? "WIO_MODULE_ACCESS_PRIVATE" :
                        "WIO_MODULE_ACCESS_PUBLIC";
                    const std::string setterExportExpr = field.setterExportIndex.has_value()
                        ? ("&WIO_MODULE_EXPORTS[" + std::to_string(*field.setterExportIndex) + "]")
                        : "nullptr";
                    const std::string dynamicGetterExpr = field.dynamicGetterSymbol.has_value()
                        ? ("&" + *field.dynamicGetterSymbol)
                        : "nullptr";
                    const std::string dynamicSetterExpr = field.dynamicSetterSymbol.has_value()
                        ? ("&" + *field.dynamicSetterSymbol)
                        : "nullptr";
                    const std::string typeDescriptorExpr = field.fieldType
                        ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(ensureTypeDescriptor(field.fieldType)))
                        : "nullptr";
                    const auto& attributeTable = fieldAttributeTables[fieldIndex];

                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(field.fieldName) +
                        "\", " + getAbiTypeEnumName(field.fieldType) +
                        ", " + typeDescriptorExpr +
                        ", " + std::to_string(flags) + "u, " + accessModifierExpr +
                        ", &WIO_MODULE_EXPORTS[" + std::to_string(field.getterExportIndex) +
                        "], " + setterExportExpr + ", " + dynamicGetterExpr + ", " + dynamicSetterExpr +
                        ", " + std::to_string(attributeTable.count) + "u, " + attributeTable.expression + " }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }

            if (!exportedType.methods.empty())
            {
                emitLine("static const WioModuleMethod WIO_MODULE_TYPE_METHODS_" + std::to_string(typeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t methodIndex = 0; methodIndex < exportedType.methods.size(); ++methodIndex)
                {
                    const auto& method = exportedType.methods[methodIndex];
                    const auto& attributeTable = methodAttributeTables[methodIndex];
                    const std::string suffix = (methodIndex + 1 < exportedType.methods.size()) ? "," : "";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(method.methodName) +
                        "\", &WIO_MODULE_EXPORTS[" + std::to_string(method.exportIndex) + "], " +
                        std::to_string(attributeTable.count) + "u, " + attributeTable.expression + " }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }
        }

        if (!exportedTypes.empty())
        {
            emitLine("static const WioModuleType WIO_MODULE_TYPES[] =");
            emitLine("{");
            indent();
            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
                const auto& exportedType = exportedTypes[typeIndex];
                const std::string suffix = (typeIndex + 1 < exportedTypes.size()) ? "," : "";
                const std::string typeKind = exportedType.isObject ? "WIO_MODULE_TYPE_OBJECT" : "WIO_MODULE_TYPE_COMPONENT";
                const std::string createExportExpr = exportedType.createExportIndex.has_value()
                    ? ("&WIO_MODULE_EXPORTS[" + std::to_string(*exportedType.createExportIndex) + "]")
                    : "nullptr";
                const std::string constructorArrayExpr = exportedType.constructors.empty()
                    ? "nullptr"
                    : ("WIO_MODULE_TYPE_CONSTRUCTORS_" + std::to_string(typeIndex));
                const std::string fieldArrayExpr = exportedType.fields.empty()
                    ? "nullptr"
                    : ("WIO_MODULE_TYPE_FIELDS_" + std::to_string(typeIndex));
                const std::string methodArrayExpr = exportedType.methods.empty()
                    ? "nullptr"
                    : ("WIO_MODULE_TYPE_METHODS_" + std::to_string(typeIndex));
                const auto& typeAttributeTable = typeAttributeTables[typeIndex];

                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedType.logicalName) +
                    "\", \"" + common::wioStringToEscapedCppString(exportedType.symbolName) +
                    "\", " + typeKind +
                    ", " + createExportExpr +
                    ", &WIO_MODULE_EXPORTS[" + std::to_string(exportedType.destroyExportIndex) +
                    "], " + std::to_string(exportedType.constructors.size()) + "u, " + constructorArrayExpr +
                    ", " + std::to_string(exportedType.fields.size()) + "u, " + fieldArrayExpr +
                    ", " + std::to_string(exportedType.methods.size()) + "u, " + methodArrayExpr +
                    ", " + std::to_string(typeAttributeTable.count) + "u, " + typeAttributeTable.expression + " }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        std::string applicationDescriptorExpression = "nullptr";
        if (applicationEntry)
        {
            const std::string applicationName = applicationEntry->applicationName;
            const std::string cppType = Mangler::mangleStruct(applicationName);
            const std::string lifecyclePrefix = "_WF___extension_" + applicationName + "Lifecycle_";
            const std::string receiverSuffix = "_ref_mut_" + applicationName;
            const std::string startFunction = lifecyclePrefix + "Start" + receiverSuffix;
            const std::string updateFunction = lifecyclePrefix + "Update" + receiverSuffix + "_f64";
            const std::string closeFunction = lifecyclePrefix + "Close" + receiverSuffix;
            const std::string exitFunction = lifecyclePrefix + "Exit" + receiverSuffix + "_i32";

            std::string applicationStagesExpression = "nullptr";
            if (!applicationEntry->applicationStages.empty())
            {
                emitLine("static const WioApplicationStageDescriptor WIO_APPLICATION_STAGES[] =");
                emitLine("{");
                indent();
                for (size_t stageIndex = 0; stageIndex < applicationEntry->applicationStages.size(); ++stageIndex)
                {
                    const auto& stage = applicationEntry->applicationStages[stageIndex];
                    std::vector<std::string> flagExpressions;
                    if (stage.fixed) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_FIXED");
                    if (stage.mainThread) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_MAIN_THREAD");
                    if (stage.containsSystem) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_CONTAINS_SYSTEM");
                    if (stage.containsApplication) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_CONTAINS_APPLICATION");
                    if (stage.legacyExplicit) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_LEGACY_EXPLICIT");
                    std::string flags = "0u";
                    if (!flagExpressions.empty())
                    {
                        flags = flagExpressions.front();
                        for (size_t flagIndex = 1; flagIndex < flagExpressions.size(); ++flagIndex)
                            flags += " | " + flagExpressions[flagIndex];
                    }
                    const std::string suffix = stageIndex + 1u == applicationEntry->applicationStages.size() ? "" : ",";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(stage.name) + "\", \"" +
                        common::wioStringToEscapedCppString(stage.after) + "\", " +
                        std::to_string(stage.fixedHz) + ", " + std::to_string(stage.order) + "u, " + flags + " }" + suffix);
                }
                dedent();
                emitLine("};");
                emitLine();
                applicationStagesExpression = "WIO_APPLICATION_STAGES";
            }

            emitLine("struct WioGeneratedApplicationState final");
            emitLine("{");
            indent();
            emitLine(cppType + " application{};");
            emitLine("std::string lastError{};");
            emitLine("std::thread::id owner{};");
            emitLine("bool started = false;");
            emitLine("bool closed = false;");
            emitLine("bool faulted = false;");
            dedent();
            emitLine("};");
            emitLine();

            emitLine("static bool WioApplicationOnOwnerThread(const WioGeneratedApplicationState* state) noexcept");
            emitLine("{");
            indent();
            emitLine("return state != nullptr && state->owner == std::this_thread::get_id();");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationConstruct(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("if (storage == nullptr) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("auto* state = ::new (storage) WioGeneratedApplicationState{};");
            emitLine("state->owner = std::this_thread::get_id();");
            emitLine("return WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_APPLICATION_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static void WioApplicationRollbackStart(WioGeneratedApplicationState* state) noexcept");
            emitLine("{");
            indent();
            emitLine("if (state == nullptr) return;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine(closeFunction + "(&state->application);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& rollbackError)");
            emitLine("{");
            indent();
            emitLine("state->lastError += \"; start rollback failed: \";");
            emitLine("state->lastError += rollbackError.what();");
            dedent();
            emitLine("}");
            emitLine("catch (...) { state->lastError += \"; start rollback failed with a non-standard exception\"; }");
            emitLine("state->closed = true;");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationStart(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || state->closed || state->started) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::BindAsyncMainExecutor();");
            emitLine(startFunction + "(&state->application);");
            emitLine("state->started = true;");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("return state->application.__exitRequested ? WIO_APPLICATION_EXIT_REQUESTED : WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("state->faulted = true;");
            emitLine("state->lastError = error.what();");
            emitLine("WioApplicationRollbackStart(state);");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine("state->lastError = \"unhandled non-standard exception\";");
            emitLine("state->faulted = true;");
            emitLine("WioApplicationRollbackStart(state);");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationUpdate(void* storage, double deltaSeconds) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || !state->started || state->closed) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("if (state->faulted) return WIO_APPLICATION_FAULTED;");
            emitLine("if (state->application.__exitRequested) return WIO_APPLICATION_EXIT_REQUESTED;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(updateFunction + "(&state->application, deltaSeconds);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("return state->application.__exitRequested ? WIO_APPLICATION_EXIT_REQUESTED : WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("state->faulted = true;");
            emitLine("state->lastError = error.what();");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine("state->faulted = true;");
            emitLine("state->lastError = \"unhandled non-standard exception\";");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationRequestExit(void* storage, std::int32_t exitCode) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || state->closed) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine(exitFunction + "(&state->application, exitCode);");
            emitLine("return WIO_APPLICATION_EXIT_REQUESTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { state->faulted = true; return WIO_APPLICATION_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationClose(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || !state->started) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (state->closed) return WIO_APPLICATION_ALREADY_CLOSED;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(closeFunction + "(&state->application);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("state->closed = true;");
            emitLine("return state->faulted ? WIO_APPLICATION_FAULTED : WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("state->closed = true;");
            emitLine("state->faulted = true;");
            emitLine("state->lastError = error.what();");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { state->closed = true; state->faulted = true; return WIO_APPLICATION_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static void WioApplicationDestroy(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr) return;");
            emitLine("if (state->started && !state->closed && WioApplicationOnOwnerThread(state)) (void)WioApplicationClose(state);");
            emitLine("state->~WioGeneratedApplicationState();");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static bool WioApplicationExitRequested(const void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const WioGeneratedApplicationState*>(storage);");
            emitLine("return state != nullptr && state->application.__exitRequested;");
            dedent();
            emitLine("}");
            emitLine("static std::int32_t WioApplicationExitCode(const void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const WioGeneratedApplicationState*>(storage);");
            emitLine("return state != nullptr ? state->application.__exitCode : 1;");
            dedent();
            emitLine("}");
            emitLine("static std::uint64_t WioApplicationPumpMain(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return 0u;");
            emitLine("try { return wio::runtime::DrainAsyncMainExecutor(); }");
            emitLine("catch (...) { state->faulted = true; return 0u; }");
            dedent();
            emitLine("}");
            emitLine("static const char* WioApplicationLastError(const void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const WioGeneratedApplicationState*>(storage);");
            emitLine("return state != nullptr ? state->lastError.c_str() : \"application state is null\";");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static const WioApplicationDescriptor WIO_MODULE_APPLICATION =");
            emitLine("{");
            indent();
            emitLine("\"" + common::wioStringToEscapedCppString(applicationName) + "\",");
            emitLine("sizeof(WioGeneratedApplicationState),");
            emitLine("alignof(WioGeneratedApplicationState),");
            emitLine("WIO_APPLICATION_MAIN_THREAD_AFFINE | WIO_APPLICATION_HOST_OWNS_STORAGE | WIO_APPLICATION_NON_BLOCKING_UPDATE,");
            emitLine("0u,");
            emitLine("&WioApplicationConstruct,");
            emitLine("&WioApplicationStart,");
            emitLine("&WioApplicationUpdate,");
            emitLine("&WioApplicationRequestExit,");
            emitLine("&WioApplicationClose,");
            emitLine("&WioApplicationDestroy,");
            emitLine("&WioApplicationExitRequested,");
            emitLine("&WioApplicationExitCode,");
            emitLine("&WioApplicationPumpMain,");
            emitLine("&WioApplicationLastError,");
            emitLine(std::to_string(applicationEntry->applicationStages.size()) + "u,");
            emitLine("0u,");
            emitLine(applicationStagesExpression);
            dedent();
            emitLine("};");
            applicationDescriptorExpression = "&WIO_MODULE_APPLICATION";
        }

        emitLine("extern \"C\" WIO_EXPORT const WioModuleApi* WioModuleGetApi()");
        emitLine("{");
        indent();

        emitLine("static const WioModuleApi API =");
        emitLine("{");
        indent();
        emitLine("WIO_MODULE_API_DESCRIPTOR_VERSION,");
        emitLine(std::to_string(capabilities) + "u,");
        emitLine(std::to_string(stateSchemaVersion) + "u,");
        emitLine("0u,");
        emitLine(lifecycleFunctions.apiVersion ? "&WioModuleApiVersion," : "nullptr,");
        emitLine(lifecycleFunctions.load ? "&WioModuleLoad," : "nullptr,");
        emitLine(lifecycleFunctions.update ? "&WioModuleUpdate," : "nullptr,");
        emitLine(lifecycleFunctions.saveState ? "&WioModuleSaveState," : "nullptr,");
        emitLine(lifecycleFunctions.restoreState ? "&WioModuleRestoreState," : "nullptr,");
        emitLine(lifecycleFunctions.unload ? "&WioModuleUnload," : "nullptr,");
        emitLine(std::to_string(exportedFunctions.size()) + "u,");
        emitLine(exportedFunctions.empty() ? "nullptr," : "WIO_MODULE_EXPORTS,");
        emitLine(std::to_string(commandExportIndices.size()) + "u,");
        emitLine(commandExportIndices.empty() ? "nullptr," : "WIO_MODULE_COMMANDS,");
        emitLine(std::to_string(eventExportIndices.size()) + "u,");
        emitLine(eventExportIndices.empty() ? "nullptr," : "WIO_MODULE_EVENT_HOOKS,");
        emitLine(std::to_string(exportedTypes.size()) + "u,");
        emitLine(exportedTypes.empty() ? "nullptr," : "WIO_MODULE_TYPES,");
        emitLine("{ WIO_SDK_VERSION_MAJOR, WIO_SDK_VERSION_MINOR, WIO_SDK_VERSION_PATCH },");
        emitLine("sizeof(WioModuleApi),");
        emitLine(applicationDescriptorExpression + ",");
        emitLine(std::to_string(asyncExportIndices.size()) + "u,");
        emitLine(asyncExportIndices.empty() ? "nullptr," : "WIO_MODULE_ASYNC_EXPORTS,");
        emitLine(asyncHostDescriptorExpression);
        dedent();
        emitLine("};");
        emitLine("return &API;");
        dedent();
        emitLine("}");
    }
}

}
