#pragma once

#include <cstdint>
#include <cstring>

// Stable sidecar consumed by future C++, bytecode and VM loaders. It does not
// change WioModuleApi v11; generated modules may expose this descriptor while
// older modules keep working through the legacy name-based API.
inline constexpr std::uint32_t WIO_SDK_MODULE_CONTRACT_VERSION = 1u;

enum WioSdkModuleKind : std::uint32_t
{
    WIO_SDK_MODULE_PROGRAM = 0u,
    WIO_SDK_MODULE_WIO_LIBRARY = 1u,
    WIO_SDK_MODULE_NATIVE_LIBRARY = 2u
};

enum WioSdkExportKind : std::uint32_t
{
    WIO_SDK_EXPORT_FUNCTION = 0u,
    WIO_SDK_EXPORT_GENERIC_FUNCTION_SPECIALIZATION = 1u,
    WIO_SDK_EXPORT_OBJECT_TYPE = 2u,
    WIO_SDK_EXPORT_COMPONENT_TYPE = 3u
};

enum WioSdkExportRole : std::uint32_t
{
    WIO_SDK_EXPORT_ORDINARY = 0u,
    WIO_SDK_EXPORT_COMMAND = 1u,
    WIO_SDK_EXPORT_EVENT_HOOK = 2u
};

enum WioSdkLifecycleFlag : std::uint32_t
{
    WIO_SDK_LIFECYCLE_API_VERSION = 1u << 0,
    WIO_SDK_LIFECYCLE_LOAD = 1u << 1,
    WIO_SDK_LIFECYCLE_UPDATE = 1u << 2,
    WIO_SDK_LIFECYCLE_UNLOAD = 1u << 3,
    WIO_SDK_LIFECYCLE_SAVE_STATE = 1u << 4,
    WIO_SDK_LIFECYCLE_RESTORE_STATE = 1u << 5
};

struct WioSdkExportDescriptor
{
    std::uint64_t stableId;
    const char* stableKey;
    const char* logicalName;
    const char* symbolName;
    const char* roleName;
    std::uint32_t callTableSlot;
    WioSdkExportKind kind;
    WioSdkExportRole role;
    std::uint32_t reserved;
};

struct WioSdkReflectionDescriptor
{
    std::uint64_t stableTypeId;
    const char* logicalName;
    std::uint32_t kind;
    std::uint32_t flags;
};

using WioSdkInvokeFn = std::int32_t(*)(
    const void* arguments,
    std::uint32_t argumentCount,
    void* outResult,
    void* context);

struct WioSdkCallEntry
{
    std::uint64_t stableId;
    WioSdkInvokeFn invoke;
    void* context;
};

struct WioSdkModuleContract
{
    std::uint32_t descriptorVersion;
    std::uint32_t descriptorSize;
    WioSdkModuleKind kind;
    std::uint32_t stateSchemaVersion;
    std::uint64_t stableId;
    const char* stableKey;
    std::uint32_t lifecycleFlags;
    std::uint32_t exportCount;
    const WioSdkExportDescriptor* exports;
    std::uint32_t reflectionCount;
    const WioSdkReflectionDescriptor* reflection;
    std::uint32_t callTableCount;
    const WioSdkCallEntry* callTable;
};

using WioGetSdkModuleContractFn = const WioSdkModuleContract*(*)();

[[nodiscard]] constexpr std::uint64_t WioStableSdkId(const char* text) noexcept
{
    if (text == nullptr) return 0;
    std::uint64_t hash = 14695981039346656037ull;
    while (*text != '\0')
    {
        hash ^= static_cast<std::uint8_t>(*text++);
        hash *= 1099511628211ull;
    }
    return hash;
}

[[nodiscard]] inline const WioSdkExportDescriptor* WioFindSdkExport(
    const WioSdkModuleContract* contract,
    const std::uint64_t stableId) noexcept
{
    if (contract == nullptr || contract->exports == nullptr || stableId == 0)
        return nullptr;
    for (std::uint32_t index = 0; index < contract->exportCount; ++index)
        if (contract->exports[index].stableId == stableId)
            return &contract->exports[index];
    return nullptr;
}

[[nodiscard]] inline bool WioValidateSdkModuleContract(
    const WioSdkModuleContract* contract) noexcept
{
    if (contract == nullptr || contract->descriptorVersion != WIO_SDK_MODULE_CONTRACT_VERSION ||
        contract->descriptorSize < sizeof(WioSdkModuleContract) || contract->stableId == 0 ||
        contract->stableKey == nullptr || contract->stableId != WioStableSdkId(contract->stableKey) ||
        contract->exportCount != contract->callTableCount)
        return false;
    if (contract->exportCount != 0 && (contract->exports == nullptr || contract->callTable == nullptr))
        return false;
    for (std::uint32_t index = 0; index < contract->exportCount; ++index)
    {
        const WioSdkExportDescriptor& entry = contract->exports[index];
        if (entry.stableId == 0 || entry.stableKey == nullptr ||
            entry.stableId != WioStableSdkId(entry.stableKey) || entry.logicalName == nullptr ||
            entry.symbolName == nullptr ||
            (entry.role != WIO_SDK_EXPORT_ORDINARY && entry.roleName == nullptr) ||
            entry.callTableSlot != index || contract->callTable[index].stableId != entry.stableId ||
            contract->callTable[index].invoke == nullptr)
            return false;
    }
    const bool saves = (contract->lifecycleFlags & WIO_SDK_LIFECYCLE_SAVE_STATE) != 0;
    const bool restores = (contract->lifecycleFlags & WIO_SDK_LIFECYCLE_RESTORE_STATE) != 0;
    return saves == restores;
}
