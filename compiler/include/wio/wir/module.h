#pragma once

#include "wio/wir/id.h"
#include "wio/wir/type.h"

#include <cstdint>
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

    struct ReflectionDescriptor
    {
        std::uint64_t stableTypeId = 0;
        std::string logicalName;
        TypeId type;
        NominalKind nominalKind = NominalKind::None;
        bool isExported = false;

        auto operator<=>(const ReflectionDescriptor&) const = default;
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
        std::vector<ReflectionDescriptor> reflection;
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
}
