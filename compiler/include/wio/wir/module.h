#pragma once

#include "wio/wir/id.h"
#include "wio/wir/type.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wio::wir
{
    inline constexpr std::uint32_t ModuleAbiDescriptorVersion = 11u;

    enum class ModuleKind : std::uint8_t
    {
        Program,
        WioLibrary,
        NativeLibrary
    };

    enum class ModuleImportKind : std::uint8_t
    {
        WioModule,
        StandardModule,
        NativeHeader,
        NativeLibrary
    };

    enum class ModuleExportKind : std::uint8_t
    {
        Function,
        GenericFunctionSpecialization,
        ObjectType,
        ComponentType
    };

    enum class ModuleExportRole : std::uint8_t
    {
        Ordinary,
        Command,
        EventHook
    };

    enum class MetadataTargetKind : std::uint8_t
    {
        Module,
        Function,
        Type,
        Field,
        Method,
        Parameter,
        EnumCase,
        Application,
        System,
        Handler
    };

    enum class AttributeOriginKind : std::uint8_t
    {
        Direct,
        Inherited,
        Scoped,
        Composed,
        Generated,
        Compiler
    };

    enum class AttributeProcessorPhase : std::uint8_t
    {
        Validation,
        Derive,
        Pre,
        Post,
        Finally,
        Around,
        Unknown
    };

    enum class ApplicationStageKind : std::uint8_t
    {
        Variable,
        Fixed
    };

    enum class ApplicationAffinity : std::uint8_t
    {
        Inherit,
        Main,
        Worker
    };

    enum class ResourceAccess : std::uint8_t
    {
        Read,
        Write
    };

    struct ModuleImport
    {
        std::uint64_t stableId = 0;
        std::string logicalName;
        std::string sourcePath;
        std::string alias;
        std::vector<std::string> importedSymbols;
        ModuleImportKind kind = ModuleImportKind::WioModule;
        bool importAll = false;

        auto operator<=>(const ModuleImport&) const = default;
    };

    struct ModuleExport
    {
        std::uint64_t stableId = 0;
        std::string stableKey;
        std::string logicalName;
        std::string symbolName;
        ModuleExportKind kind = ModuleExportKind::Function;
        ModuleExportRole role = ModuleExportRole::Ordinary;
        std::string roleName;
        FunctionId function;
        TypeId type;
        std::vector<TypeId> parameterTypes;
        TypeId returnType;
        std::vector<TypeId> genericArguments;
        std::uint32_t callTableSlot = 0;
        bool isAsync = false;

        auto operator<=>(const ModuleExport&) const = default;
    };

    struct AttributeArgumentDescriptor
    {
        std::string name;
        std::string sourceText;
        TypeId type;
        bool usedDefault = false;

        auto operator<=>(const AttributeArgumentDescriptor&) const = default;
    };

    struct AttributeProcessorDescriptor
    {
        std::uint64_t stableId = 0;
        std::string canonicalTypeName;
        std::string hookName;
        std::string hookMode;
        AttributeProcessorPhase phase = AttributeProcessorPhase::Unknown;
        TypeId valueType;

        auto operator<=>(const AttributeProcessorDescriptor&) const = default;
    };

    struct AttributeApplicationDescriptor
    {
        std::uint64_t stableId = 0;
        std::string canonicalName;
        std::string originParent;
        std::string selector;
        MetadataTargetKind targetKind = MetadataTargetKind::Module;
        AttributeOriginKind origin = AttributeOriginKind::Direct;
        std::uint64_t targetStableId = 0;
        TypeId targetType;
        FunctionId targetFunction;
        std::uint32_t parameterIndex = 0;
        std::uint32_t sourceOrder = 0;
        bool runtimeRetained = false;
        std::vector<AttributeArgumentDescriptor> arguments;
        std::vector<AttributeProcessorDescriptor> processors;

        auto operator<=>(const AttributeApplicationDescriptor&) const = default;
    };

    struct ReflectedFieldDescriptor
    {
        std::uint64_t stableId = 0;
        std::string name;
        TypeId type;
        FieldVisibility visibility = FieldVisibility::Private;
        bool isMutable = true;
        std::vector<std::uint64_t> attributes;

        auto operator<=>(const ReflectedFieldDescriptor&) const = default;
    };

    struct ReflectedMethodDescriptor
    {
        std::uint64_t stableId = 0;
        std::string name;
        FunctionId function;
        TypeId returnType;
        std::vector<TypeId> parameterTypes;
        std::uint32_t slot = 0;
        bool isAsync = false;
        std::vector<std::uint64_t> attributes;

        auto operator<=>(const ReflectedMethodDescriptor&) const = default;
    };

    struct ReflectedCaseDescriptor
    {
        std::uint64_t stableId = 0;
        std::string name;
        std::vector<std::uint64_t> attributes;

        auto operator<=>(const ReflectedCaseDescriptor&) const = default;
    };

    struct ReflectionDescriptor
    {
        std::uint64_t stableTypeId = 0;
        std::string logicalName;
        TypeId type;
        NominalKind nominalKind = NominalKind::None;
        bool isExported = false;
        bool runtimeVisible = true;
        std::vector<std::uint64_t> attributes;
        std::vector<ReflectedFieldDescriptor> fields;
        std::vector<ReflectedMethodDescriptor> methods;
        std::vector<ReflectedCaseDescriptor> cases;

        auto operator<=>(const ReflectionDescriptor&) const = default;
    };

    struct ApplicationResourceBinding
    {
        std::string name;
        TypeId type;
        ResourceAccess access = ResourceAccess::Read;

        auto operator<=>(const ApplicationResourceBinding&) const = default;
    };

    struct ApplicationStageRun
    {
        std::string targetName;
        std::string methodName;
        TypeId targetType;
        FunctionId function;
        std::vector<ApplicationResourceBinding> resources;
        bool applicationTarget = false;
        bool acceptsDelta = true;

        auto operator<=>(const ApplicationStageRun&) const = default;
    };

    struct ApplicationStageDescriptor
    {
        std::uint64_t stableId = 0;
        std::string name;
        std::string after;
        double fixedHz = 0.0;
        std::uint32_t order = 0;
        ApplicationStageKind kind = ApplicationStageKind::Variable;
        ApplicationAffinity affinity = ApplicationAffinity::Inherit;
        bool legacyExplicit = false;
        std::vector<ApplicationStageRun> runs;

        auto operator<=>(const ApplicationStageDescriptor&) const = default;
    };

    struct SystemDescriptor
    {
        std::uint64_t stableId = 0;
        std::string logicalName;
        TypeId type;
        FunctionId start;
        FunctionId update;
        FunctionId close;

        auto operator<=>(const SystemDescriptor&) const = default;
    };

    struct ApplicationDescriptor
    {
        std::uint64_t stableId = 0;
        std::string logicalName;
        TypeId type;
        FunctionId entry;
        FunctionId start;
        FunctionId update;
        FunctionId close;
        FunctionId exit;
        std::vector<TypeId> systems;
        std::vector<ApplicationStageDescriptor> stages;
        bool hostOwnsStorage = true;
        bool nonBlockingScheduling = true;

        auto operator<=>(const ApplicationDescriptor&) const = default;
    };

    struct ModuleLifecycle
    {
        FunctionId apiVersion;
        FunctionId load;
        FunctionId update;
        FunctionId unload;
        FunctionId saveState;
        FunctionId restoreState;
        std::uint32_t stateSchemaVersion = 0;

        [[nodiscard]] bool supportsStateTransfer() const
        {
            return saveState && restoreState;
        }

        auto operator<=>(const ModuleLifecycle&) const = default;
    };

    struct SdkCallTable
    {
        std::uint32_t descriptorVersion = ModuleAbiDescriptorVersion;
        std::uint64_t stableId = 0;
        std::vector<std::uint64_t> entries;

        auto operator<=>(const SdkCallTable&) const = default;
    };

    // This is the backend-neutral module boundary. C++, bytecode and VM
    // loaders consume the same identities and slots instead of rediscovering
    // exports from source attributes independently.
    struct ModuleContract
    {
        ModuleKind kind = ModuleKind::Program;
        std::uint64_t stableId = 0;
        std::string logicalName;
        std::string stableKey;
        std::vector<ModuleImport> imports;
        std::vector<ModuleExport> exports;
        std::vector<AttributeApplicationDescriptor> attributes;
        std::vector<ReflectionDescriptor> reflection;
        std::vector<SystemDescriptor> systems;
        std::optional<ApplicationDescriptor> application;
        ModuleLifecycle lifecycle;
        SdkCallTable callTable;

        auto operator<=>(const ModuleContract&) const = default;
    };

    [[nodiscard]] constexpr std::uint64_t stableModuleHash(const std::string_view text) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char byte : text)
        {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    [[nodiscard]] std::string_view moduleKindName(ModuleKind kind);
    [[nodiscard]] std::string_view moduleImportKindName(ModuleImportKind kind);
    [[nodiscard]] std::string_view moduleExportKindName(ModuleExportKind kind);
    [[nodiscard]] std::string_view moduleExportRoleName(ModuleExportRole role);
    [[nodiscard]] std::string_view metadataTargetKindName(MetadataTargetKind kind);
    [[nodiscard]] std::string_view attributeOriginKindName(AttributeOriginKind origin);
    [[nodiscard]] std::string_view attributeProcessorPhaseName(AttributeProcessorPhase phase);
    [[nodiscard]] std::string_view applicationStageKindName(ApplicationStageKind kind);
    [[nodiscard]] std::string_view applicationAffinityName(ApplicationAffinity affinity);
    [[nodiscard]] std::string_view resourceAccessName(ResourceAccess access);
}
