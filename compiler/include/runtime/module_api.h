#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

inline constexpr std::uint32_t WIO_MODULE_API_DESCRIPTOR_VERSION = 1u;

enum WioModuleCapability : std::uint32_t
{
    WIO_MODULE_CAP_API_VERSION = 1u << 0,
    WIO_MODULE_CAP_LOAD = 1u << 1,
    WIO_MODULE_CAP_UPDATE = 1u << 2,
    WIO_MODULE_CAP_UNLOAD = 1u << 3,
    WIO_MODULE_CAP_SAVE_STATE = 1u << 4,
    WIO_MODULE_CAP_RESTORE_STATE = 1u << 5
};

enum WioAbiType : std::uint32_t
{
    WIO_ABI_UNKNOWN = 0,
    WIO_ABI_VOID,
    WIO_ABI_BOOL,
    WIO_ABI_CHAR,
    WIO_ABI_UCHAR,
    WIO_ABI_BYTE,
    WIO_ABI_I8,
    WIO_ABI_I16,
    WIO_ABI_I32,
    WIO_ABI_I64,
    WIO_ABI_U8,
    WIO_ABI_U16,
    WIO_ABI_U32,
    WIO_ABI_U64,
    WIO_ABI_ISIZE,
    WIO_ABI_USIZE,
    WIO_ABI_F32,
    WIO_ABI_F64
};

union WioValuePayload
{
    bool v_bool;
    char v_char;
    unsigned char v_uchar;
    std::uint8_t v_byte;
    std::int8_t v_i8;
    std::int16_t v_i16;
    std::int32_t v_i32;
    std::int64_t v_i64;
    std::uint8_t v_u8;
    std::uint16_t v_u16;
    std::uint32_t v_u32;
    std::uint64_t v_u64;
    std::intptr_t v_isize;
    std::uintptr_t v_usize;
    float v_f32;
    double v_f64;

    constexpr WioValuePayload() : v_u64(0) {}
};

struct WioValue
{
    WioAbiType type = WIO_ABI_UNKNOWN;
    WioValuePayload value{};
};

enum WioInvokeStatus : std::int32_t
{
    WIO_INVOKE_OK = 0,
    WIO_INVOKE_EXPORT_NOT_FOUND = 1,
    WIO_INVOKE_BAD_ARGUMENTS = 2,
    WIO_INVOKE_TYPE_MISMATCH = 3,
    WIO_INVOKE_RESULT_REQUIRED = 4,
    WIO_INVOKE_NOT_CALLABLE = 5
};

using WioModuleInvokeFn = std::int32_t(*)(const WioValue* args, std::uint32_t argCount, WioValue* outResult);

struct WioModuleExport
{
    const char* logicalName;
    const char* symbolName;
    WioAbiType returnType;
    std::uint32_t parameterCount;
    const WioAbiType* parameterTypes;
    WioModuleInvokeFn invoke;
};

struct WioModuleCommand
{
    const char* commandName;
    const WioModuleExport* exportEntry;
};

struct WioModuleEventHook
{
    const char* hookName;
    const char* eventName;
    const WioModuleExport* exportEntry;
};

struct WioModuleApi
{
    std::uint32_t descriptorVersion;
    std::uint32_t capabilities;
    std::uint32_t stateSchemaVersion;
    std::uint32_t reserved;
    std::uint32_t (*apiVersion)();
    std::int32_t (*load)();
    void (*update)(float);
    std::int32_t (*saveState)();
    std::int32_t (*restoreState)(std::int32_t);
    void (*unload)();
    std::uint32_t exportCount;
    const WioModuleExport* exports;
    std::uint32_t commandCount;
    const WioModuleCommand* commands;
    std::uint32_t eventHookCount;
    const WioModuleEventHook* eventHooks;
};

using WioModuleGetApiFn = const WioModuleApi*(*)();

inline const WioModuleExport* WioFindModuleExport(const WioModuleApi* api, const char* logicalName)
{
    if (api == nullptr || logicalName == nullptr || api->exports == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < api->exportCount; ++i)
    {
        const WioModuleExport& exportEntry = api->exports[i];
        if (exportEntry.logicalName != nullptr && std::strcmp(exportEntry.logicalName, logicalName) == 0)
            return &exportEntry;
    }

    return nullptr;
}

inline std::int32_t WioInvokeModuleExport(const WioModuleApi* api, const char* logicalName, const WioValue* args, std::uint32_t argCount, WioValue* outResult)
{
    const WioModuleExport* exportEntry = WioFindModuleExport(api, logicalName);
    if (exportEntry == nullptr)
        return WIO_INVOKE_EXPORT_NOT_FOUND;

    if (exportEntry->invoke == nullptr)
        return WIO_INVOKE_NOT_CALLABLE;

    return exportEntry->invoke(args, argCount, outResult);
}

inline const WioModuleCommand* WioFindModuleCommand(const WioModuleApi* api, const char* commandName)
{
    if (api == nullptr || commandName == nullptr || api->commands == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < api->commandCount; ++i)
    {
        const WioModuleCommand& commandEntry = api->commands[i];
        if (commandEntry.commandName != nullptr && std::strcmp(commandEntry.commandName, commandName) == 0)
            return &commandEntry;
    }

    return nullptr;
}

inline std::int32_t WioInvokeModuleCommand(const WioModuleApi* api, const char* commandName, const WioValue* args, std::uint32_t argCount, WioValue* outResult)
{
    const WioModuleCommand* commandEntry = WioFindModuleCommand(api, commandName);
    if (commandEntry == nullptr || commandEntry->exportEntry == nullptr)
        return WIO_INVOKE_EXPORT_NOT_FOUND;

    if (commandEntry->exportEntry->invoke == nullptr)
        return WIO_INVOKE_NOT_CALLABLE;

    return commandEntry->exportEntry->invoke(args, argCount, outResult);
}

inline const WioModuleEventHook* WioFindModuleEventHook(const WioModuleApi* api, const char* hookName)
{
    if (api == nullptr || hookName == nullptr || api->eventHooks == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < api->eventHookCount; ++i)
    {
        const WioModuleEventHook& hookEntry = api->eventHooks[i];
        if (hookEntry.hookName != nullptr && std::strcmp(hookEntry.hookName, hookName) == 0)
            return &hookEntry;
    }

    return nullptr;
}

inline const WioModuleEventHook* WioFindFirstModuleEventHookForEvent(const WioModuleApi* api, const char* eventName)
{
    if (api == nullptr || eventName == nullptr || api->eventHooks == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < api->eventHookCount; ++i)
    {
        const WioModuleEventHook& hookEntry = api->eventHooks[i];
        if (hookEntry.eventName != nullptr && std::strcmp(hookEntry.eventName, eventName) == 0)
            return &hookEntry;
    }

    return nullptr;
}

inline std::int32_t WioInvokeModuleEventHook(const WioModuleApi* api, const char* hookName, const WioValue* args, std::uint32_t argCount, WioValue* outResult)
{
    const WioModuleEventHook* hookEntry = WioFindModuleEventHook(api, hookName);
    if (hookEntry == nullptr || hookEntry->exportEntry == nullptr)
        return WIO_INVOKE_EXPORT_NOT_FOUND;

    if (hookEntry->exportEntry->invoke == nullptr)
        return WIO_INVOKE_NOT_CALLABLE;

    return hookEntry->exportEntry->invoke(args, argCount, outResult);
}
