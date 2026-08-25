#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>

#include "wio_version.h"

inline constexpr std::uint32_t WIO_MODULE_API_DESCRIPTOR_VERSION = 10u;

enum WioModuleCapability : std::uint32_t
{
    WIO_MODULE_CAP_API_VERSION = 1u << 0,
    WIO_MODULE_CAP_LOAD = 1u << 1,
    WIO_MODULE_CAP_UPDATE = 1u << 2,
    WIO_MODULE_CAP_UNLOAD = 1u << 3,
    WIO_MODULE_CAP_SAVE_STATE = 1u << 4,
    WIO_MODULE_CAP_RESTORE_STATE = 1u << 5,
    WIO_MODULE_CAP_PRODUCT_VERSION = 1u << 6,
    WIO_MODULE_CAP_TYPE_METADATA_V2 = 1u << 7,
    WIO_MODULE_CAP_TEXT_FIELDS = 1u << 8,
    WIO_MODULE_CAP_ATTRIBUTE_METADATA_V1 = 1u << 9,
    WIO_MODULE_CAP_APPLICATION_HOST_V1 = 1u << 10,
    WIO_MODULE_CAP_ASYNC_TASK_HOST_V1 = 1u << 11
};

struct WioModuleProductVersion
{
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
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

namespace wio::sdk
{
    // Carries Wio's semantic integer identity when the host C++ ABI aliases
    // distinct language types (for example u64 and usize on 64-bit targets).
    template <typename TStorage, WioAbiType TAbiType>
    class WioAbiInteger
    {
        static_assert(std::is_integral_v<TStorage>, "WioAbiInteger storage must be an integral C++ type.");
        static_assert(TAbiType == WIO_ABI_UCHAR || TAbiType == WIO_ABI_BYTE ||
                      TAbiType == WIO_ABI_I8 || TAbiType == WIO_ABI_I16 ||
                      TAbiType == WIO_ABI_I32 || TAbiType == WIO_ABI_I64 ||
                      TAbiType == WIO_ABI_U8 || TAbiType == WIO_ABI_U16 ||
                      TAbiType == WIO_ABI_U32 || TAbiType == WIO_ABI_U64 ||
                      TAbiType == WIO_ABI_ISIZE || TAbiType == WIO_ABI_USIZE,
                      "WioAbiInteger requires an integer Wio ABI kind.");

    public:
        using storage_type = TStorage;
        static constexpr WioAbiType abi_type = TAbiType;

        constexpr WioAbiInteger() noexcept = default;
        constexpr WioAbiInteger(const TStorage value) noexcept : value_(value) {}

        [[nodiscard]] constexpr TStorage value() const noexcept { return value_; }
        [[nodiscard]] constexpr explicit operator TStorage() const noexcept { return value_; }

        friend constexpr bool operator==(const WioAbiInteger&, const WioAbiInteger&) noexcept = default;

    private:
        TStorage value_{};
    };

    using WioUChar = WioAbiInteger<unsigned char, WIO_ABI_UCHAR>;
    using WioByte = WioAbiInteger<std::uint8_t, WIO_ABI_BYTE>;
    using WioI8 = WioAbiInteger<std::int8_t, WIO_ABI_I8>;
    using WioI16 = WioAbiInteger<std::int16_t, WIO_ABI_I16>;
    using WioI32 = WioAbiInteger<std::int32_t, WIO_ABI_I32>;
    using WioI64 = WioAbiInteger<std::int64_t, WIO_ABI_I64>;
    using WioU8 = WioAbiInteger<std::uint8_t, WIO_ABI_U8>;
    using WioU16 = WioAbiInteger<std::uint16_t, WIO_ABI_U16>;
    using WioU32 = WioAbiInteger<std::uint32_t, WIO_ABI_U32>;
    using WioU64 = WioAbiInteger<std::uint64_t, WIO_ABI_U64>;
    using WioISize = WioAbiInteger<std::intptr_t, WIO_ABI_ISIZE>;
    using WioUSize = WioAbiInteger<std::uintptr_t, WIO_ABI_USIZE>;
}

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

inline WioValue WioMakeAbiIntegerValue(const WioAbiType abiType, const std::uint64_t value)
{
    WioValue result{};
    result.type = abiType;

    switch (abiType)
    {
    case WIO_ABI_I8:
        result.value.v_i8 = static_cast<std::int8_t>(value);
        break;
    case WIO_ABI_I16:
        result.value.v_i16 = static_cast<std::int16_t>(value);
        break;
    case WIO_ABI_I32:
        result.value.v_i32 = static_cast<std::int32_t>(value);
        break;
    case WIO_ABI_I64:
        result.value.v_i64 = static_cast<std::int64_t>(value);
        break;
    case WIO_ABI_U8:
        result.value.v_u8 = static_cast<std::uint8_t>(value);
        break;
    case WIO_ABI_U16:
        result.value.v_u16 = static_cast<std::uint16_t>(value);
        break;
    case WIO_ABI_U32:
        result.value.v_u32 = static_cast<std::uint32_t>(value);
        break;
    case WIO_ABI_U64:
        result.value.v_u64 = static_cast<std::uint64_t>(value);
        break;
    case WIO_ABI_ISIZE:
        result.value.v_isize = static_cast<std::intptr_t>(value);
        break;
    case WIO_ABI_USIZE:
        result.value.v_usize = static_cast<std::uintptr_t>(value);
        break;
    default:
        result.type = WIO_ABI_UNKNOWN;
        break;
    }

    return result;
}

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

enum WioModuleAttributeOrigin : std::uint32_t
{
    WIO_MODULE_ATTRIBUTE_DIRECT = 0u,
    WIO_MODULE_ATTRIBUTE_COMPOSED = 1u,
    WIO_MODULE_ATTRIBUTE_SCOPED = 2u
};

enum WioModuleAttributeProcessorPhase : std::uint32_t
{
    WIO_MODULE_ATTRIBUTE_PHASE_UNKNOWN = 0u,
    WIO_MODULE_ATTRIBUTE_PHASE_VALIDATION = 1u,
    WIO_MODULE_ATTRIBUTE_PHASE_PRE = 2u,
    WIO_MODULE_ATTRIBUTE_PHASE_POST = 3u,
    WIO_MODULE_ATTRIBUTE_PHASE_FINALLY = 4u,
    WIO_MODULE_ATTRIBUTE_PHASE_AROUND = 5u,
    WIO_MODULE_ATTRIBUTE_PHASE_DERIVE = 6u
};

struct WioModuleAttributeProcessorDescriptor
{
    const char* canonicalTypeName;
    WioModuleAttributeProcessorPhase phase;
    const char* hookMode;
    std::uint32_t order;
};

struct WioModuleAttributeDescriptor
{
    const char* canonicalName;
    const char* argumentText;
    WioModuleAttributeOrigin origin;
    std::uint32_t processorCount;
    const WioModuleAttributeProcessorDescriptor* processors;
};

struct WioModuleExport
{
    const char* logicalName;
    const char* symbolName;
    WioAbiType returnType;
    std::uint32_t parameterCount;
    const WioAbiType* parameterTypes;
    WioModuleInvokeFn invoke;
    const void* rawFunction;
    std::uint32_t attributeCount;
    const WioModuleAttributeDescriptor* attributes;
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

enum WioModuleFieldFlag : std::uint32_t
{
    WIO_MODULE_FIELD_READABLE = 1u << 0,
    WIO_MODULE_FIELD_WRITABLE = 1u << 1,
    WIO_MODULE_FIELD_READONLY = 1u << 2
};

enum WioModuleAccessModifier : std::uint32_t
{
    WIO_MODULE_ACCESS_UNKNOWN = 0u,
    WIO_MODULE_ACCESS_PUBLIC = 1u,
    WIO_MODULE_ACCESS_PROTECTED = 2u,
    WIO_MODULE_ACCESS_PRIVATE = 3u
};

enum WioModuleTypeDescriptorKind : std::uint32_t
{
    WIO_MODULE_TYPE_DESC_UNKNOWN = 0u,
    WIO_MODULE_TYPE_DESC_PRIMITIVE = 1u,
    WIO_MODULE_TYPE_DESC_STRING = 2u,
    WIO_MODULE_TYPE_DESC_OBJECT = 3u,
    WIO_MODULE_TYPE_DESC_COMPONENT = 4u,
    WIO_MODULE_TYPE_DESC_DYNAMIC_ARRAY = 5u,
    WIO_MODULE_TYPE_DESC_STATIC_ARRAY = 6u,
    WIO_MODULE_TYPE_DESC_DICT = 7u,
    WIO_MODULE_TYPE_DESC_TREE = 8u,
    WIO_MODULE_TYPE_DESC_FUNCTION = 9u,
    WIO_MODULE_TYPE_DESC_OPAQUE = 10u,
    WIO_MODULE_TYPE_DESC_ENUM = 11u,
    WIO_MODULE_TYPE_DESC_FLAGSET = 12u,
    WIO_MODULE_TYPE_DESC_NULLABLE = 13u,
    WIO_MODULE_TYPE_DESC_TEXT = 14u,
    WIO_MODULE_TYPE_DESC_OPTION = 15u,
    WIO_MODULE_TYPE_DESC_RESULT = 16u,
    WIO_MODULE_TYPE_DESC_TUPLE = 17u,
    WIO_MODULE_TYPE_DESC_QUEUE = 18u,
    WIO_MODULE_TYPE_DESC_UNORDERED_SET = 19u,
    WIO_MODULE_TYPE_DESC_ORDERED_SET = 20u,
    WIO_MODULE_TYPE_DESC_SPAN = 21u,
    WIO_MODULE_TYPE_DESC_BYTE_BUFFER = 22u,
    WIO_MODULE_TYPE_DESC_BOX = 23u,
    WIO_MODULE_TYPE_DESC_ANY = 24u,
    WIO_MODULE_TYPE_DESC_INTERFACE = 25u,
    WIO_MODULE_TYPE_DESC_ASYNC_TASK = 26u,
    WIO_MODULE_TYPE_DESC_GENERIC_INSTANCE = 27u,
    WIO_MODULE_TYPE_DESC_UNIT = 28u,
    WIO_MODULE_TYPE_DESC_CONST_VALUE = 29u
};

[[nodiscard]] constexpr std::uint64_t WioStableTypeId(const std::string_view name) noexcept
{
    std::uint64_t hash = 14695981039346656037ull;
    for (const char character : name)
    {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 1099511628211ull;
    }
    return hash;
}

struct WioModuleEnumMemberDescriptor
{
    const char* name;
    WioValue value;
};

struct WioModuleTypeDescriptor
{
    const char* displayName;
    const char* logicalTypeName;
    WioModuleTypeDescriptorKind kind;
    WioAbiType abiType;
    std::uint64_t staticArraySize;
    const WioModuleTypeDescriptor* elementType;
    const WioModuleTypeDescriptor* keyType;
    const WioModuleTypeDescriptor* valueType;
    const WioModuleTypeDescriptor* returnType;
    std::uint32_t parameterCount;
    const WioModuleTypeDescriptor* const* parameterTypes;
    std::uint32_t enumMemberCount;
    const WioModuleEnumMemberDescriptor* enumMembers;
    std::uint64_t stableTypeId;
    std::uint32_t genericArgumentCount;
    const WioModuleTypeDescriptor* const* genericArguments;
    const WioModuleTypeDescriptor* constValueType;
    const char* constValue;
};

struct WioErasedValue
{
    virtual ~WioErasedValue() = default;
    [[nodiscard]] virtual const WioModuleTypeDescriptor* descriptor() const noexcept = 0;
};

template <typename TValue>
struct WioErasedValueModel final : WioErasedValue
{
    explicit WioErasedValueModel(const WioModuleTypeDescriptor* descriptorValue, TValue initialValue)
        : descriptorValue(descriptorValue),
          value(std::move(initialValue))
    {
    }

    [[nodiscard]] const WioModuleTypeDescriptor* descriptor() const noexcept override
    {
        return descriptorValue;
    }

    const WioModuleTypeDescriptor* descriptorValue = nullptr;
    TValue value;
};

using WioModuleFieldDynamicGetFn = WioErasedValue*(*)(std::uintptr_t handle);
using WioModuleFieldDynamicSetFn = std::int32_t(*)(std::uintptr_t handle, const WioErasedValue* value);

enum WioModuleTypeKind : std::uint32_t
{
    WIO_MODULE_TYPE_COMPONENT = 1u,
    WIO_MODULE_TYPE_OBJECT = 2u
};

struct WioModuleField
{
    const char* fieldName;
    WioAbiType fieldType;
    const WioModuleTypeDescriptor* typeDescriptor;
    std::uint32_t flags;
    WioModuleAccessModifier accessModifier;
    const WioModuleExport* getterExport;
    const WioModuleExport* setterExport;
    WioModuleFieldDynamicGetFn dynamicGetter;
    WioModuleFieldDynamicSetFn dynamicSetter;
    std::uint32_t attributeCount;
    const WioModuleAttributeDescriptor* attributes;
};

struct WioModuleMethod
{
    const char* methodName;
    const WioModuleExport* exportEntry;
    std::uint32_t attributeCount;
    const WioModuleAttributeDescriptor* attributes;
};

struct WioModuleConstructor
{
    const WioModuleExport* exportEntry;
};

struct WioModuleType
{
    const char* logicalName;
    const char* symbolName;
    WioModuleTypeKind kind;
    const WioModuleExport* createExport;
    const WioModuleExport* destroyExport;
    std::uint32_t constructorCount;
    const WioModuleConstructor* constructors;
    std::uint32_t fieldCount;
    const WioModuleField* fields;
    std::uint32_t methodCount;
    const WioModuleMethod* methods;
    std::uint32_t attributeCount;
    const WioModuleAttributeDescriptor* attributes;
};

enum WioApplicationStatus : std::int32_t
{
    WIO_APPLICATION_OK = 0,
    WIO_APPLICATION_EXIT_REQUESTED = 1,
    WIO_APPLICATION_ALREADY_CLOSED = 2,
    WIO_APPLICATION_INVALID_STATE = 3,
    WIO_APPLICATION_FAULTED = 4,
    WIO_APPLICATION_WRONG_THREAD = 5
};

enum WioApplicationFlag : std::uint32_t
{
    WIO_APPLICATION_MAIN_THREAD_AFFINE = 1u << 0,
    WIO_APPLICATION_HOST_OWNS_STORAGE = 1u << 1,
    WIO_APPLICATION_NON_BLOCKING_UPDATE = 1u << 2
};

using WioApplicationConstructFn = std::int32_t(*)(void* storage);
using WioApplicationStartFn = std::int32_t(*)(void* storage);
using WioApplicationUpdateFn = std::int32_t(*)(void* storage, double deltaSeconds);
using WioApplicationRequestExitFn = std::int32_t(*)(void* storage, std::int32_t exitCode);
using WioApplicationCloseFn = std::int32_t(*)(void* storage);
using WioApplicationDestroyFn = void(*)(void* storage);
using WioApplicationBoolQueryFn = bool(*)(const void* storage);
using WioApplicationExitCodeFn = std::int32_t(*)(const void* storage);
using WioApplicationPumpMainFn = std::uint64_t(*)(void* storage);
using WioApplicationLastErrorFn = const char*(*)(const void* storage);

// The host allocates stateSize bytes with stateAlignment, then calls construct.
// This keeps application state host-owned and prevents a hidden allocator from
// crossing the module boundary. All operations are main-thread-affine and
// contain Wio/native failures as WioApplicationStatus values.
struct WioApplicationDescriptor
{
    const char* logicalName;
    std::uint64_t stateSize;
    std::uint64_t stateAlignment;
    std::uint32_t flags;
    std::uint32_t reserved;
    WioApplicationConstructFn construct;
    WioApplicationStartFn start;
    WioApplicationUpdateFn update;
    WioApplicationRequestExitFn requestExit;
    WioApplicationCloseFn close;
    WioApplicationDestroyFn destroy;
    WioApplicationBoolQueryFn exitRequested;
    WioApplicationExitCodeFn exitCode;
    WioApplicationPumpMainFn pumpMain;
    WioApplicationLastErrorFn lastError;
};

enum WioAsyncTaskStatus : std::int32_t
{
    WIO_ASYNC_TASK_PENDING = 0,
    WIO_ASYNC_TASK_READY = 1,
    WIO_ASYNC_TASK_CANCELLED = 2,
    WIO_ASYNC_TASK_FAULTED = 3
};

enum WioAsyncOperationStatus : std::int32_t
{
    WIO_ASYNC_OK = 0,
    WIO_ASYNC_TIMED_OUT = 1,
    WIO_ASYNC_NOT_READY = 2,
    WIO_ASYNC_CANCELLED = 3,
    WIO_ASYNC_FAULTED = 4,
    WIO_ASYNC_BAD_ARGUMENTS = 5,
    WIO_ASYNC_TYPE_MISMATCH = 6
};

enum WioAsyncCompletionTarget : std::uint32_t
{
    WIO_ASYNC_COMPLETION_CURRENT_EXECUTOR = 0u,
    WIO_ASYNC_COMPLETION_MAIN_EXECUTOR = 1u
};

using WioAsyncCompletionFn = void(*)(void* userData, WioAsyncTaskStatus status);

struct WioAsyncTaskOps
{
    void (*retain)(void* state);
    void (*release)(void* state);
    WioAsyncTaskStatus (*status)(const void* state);
    void (*cancel)(void* state);
    std::int32_t (*waitFor)(void* state, std::uint64_t milliseconds);
    std::int32_t (*getResult)(void* state, WioValue* outResult);
    std::int32_t (*onComplete)(void* state, WioAsyncCompletionFn callback,
                               void* userData, WioAsyncCompletionTarget target);
    const char* (*lastError)(const void* state);
};

// A task handle owns one reference to state. Copying it requires ops->retain;
// every retained handle must eventually call ops->release. Completion callback
// registrations retain state internally until their callback has returned.
struct WioAsyncTaskHandle
{
    void* state;
    const WioAsyncTaskOps* ops;
    WioAbiType resultType;
};

using WioModuleAsyncInvokeFn = std::int32_t(*)(const WioValue* args,
                                                std::uint32_t argCount,
                                                WioAsyncTaskHandle* outTask);

struct WioModuleAsyncExport
{
    const char* logicalName;
    WioAbiType resultType;
    std::uint32_t parameterCount;
    const WioAbiType* parameterTypes;
    WioModuleAsyncInvokeFn invoke;
};

struct WioAsyncHostDescriptor
{
    std::uint32_t flags;
    std::uint32_t reserved;
    void (*bindMain)();
    std::uint64_t (*pumpMain)();
    std::uint64_t (*pendingMain)();
    void (*requestShutdown)();
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
    std::uint32_t typeCount;
    const WioModuleType* types;
    WioModuleProductVersion productVersion;
    std::uint32_t descriptorSize;
    const WioApplicationDescriptor* application;
    std::uint32_t asyncExportCount;
    const WioModuleAsyncExport* asyncExports;
    const WioAsyncHostDescriptor* asyncHost;
};

using WioModuleGetApiFn = const WioModuleApi*(*)();

inline const WioModuleAttributeDescriptor* WioFindModuleAttribute(
    const WioModuleAttributeDescriptor* attributes,
    const std::uint32_t attributeCount,
    const char* canonicalName)
{
    if (attributes == nullptr || canonicalName == nullptr)
        return nullptr;
    for (std::uint32_t index = 0; index < attributeCount; ++index)
    {
        if (attributes[index].canonicalName != nullptr &&
            std::strcmp(attributes[index].canonicalName, canonicalName) == 0)
            return &attributes[index];
    }
    return nullptr;
}

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

inline const WioModuleAsyncExport* WioFindModuleAsyncExport(const WioModuleApi* api, const char* logicalName)
{
    if (api == nullptr || logicalName == nullptr || api->asyncExports == nullptr)
        return nullptr;
    for (std::uint32_t index = 0u; index < api->asyncExportCount; ++index)
    {
        const WioModuleAsyncExport& entry = api->asyncExports[index];
        if (entry.logicalName != nullptr && std::strcmp(entry.logicalName, logicalName) == 0)
            return &entry;
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

inline std::uint32_t WioCountModuleEventHooksForEvent(const WioModuleApi* api, const char* eventName)
{
    if (api == nullptr || eventName == nullptr || api->eventHooks == nullptr)
        return 0;

    std::uint32_t count = 0;
    for (std::uint32_t i = 0; i < api->eventHookCount; ++i)
    {
        const WioModuleEventHook& hookEntry = api->eventHooks[i];
        if (hookEntry.eventName != nullptr && std::strcmp(hookEntry.eventName, eventName) == 0)
            ++count;
    }

    return count;
}

inline std::int32_t WioBroadcastModuleEvent(const WioModuleApi* api, const char* eventName, const WioValue* args, std::uint32_t argCount)
{
    if (api == nullptr || eventName == nullptr || api->eventHooks == nullptr)
        return WIO_INVOKE_EXPORT_NOT_FOUND;

    bool foundAny = false;
    for (std::uint32_t i = 0; i < api->eventHookCount; ++i)
    {
        const WioModuleEventHook& hookEntry = api->eventHooks[i];
        if (hookEntry.eventName == nullptr || std::strcmp(hookEntry.eventName, eventName) != 0)
            continue;

        foundAny = true;
        if (hookEntry.exportEntry == nullptr)
            return WIO_INVOKE_EXPORT_NOT_FOUND;
        if (hookEntry.exportEntry->invoke == nullptr)
            return WIO_INVOKE_NOT_CALLABLE;

        const std::int32_t status = hookEntry.exportEntry->invoke(args, argCount, nullptr);
        if (status != WIO_INVOKE_OK)
            return status;
    }

    return foundAny ? WIO_INVOKE_OK : WIO_INVOKE_EXPORT_NOT_FOUND;
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

inline const WioModuleType* WioFindModuleType(const WioModuleApi* api, const char* logicalName)
{
    if (api == nullptr || logicalName == nullptr || api->types == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < api->typeCount; ++i)
    {
        const WioModuleType& typeEntry = api->types[i];
        if (typeEntry.logicalName != nullptr && std::strcmp(typeEntry.logicalName, logicalName) == 0)
            return &typeEntry;
    }

    return nullptr;
}

inline const WioModuleField* WioFindModuleField(const WioModuleType* typeEntry, const char* fieldName)
{
    if (typeEntry == nullptr || fieldName == nullptr || typeEntry->fields == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < typeEntry->fieldCount; ++i)
    {
        const WioModuleField& fieldEntry = typeEntry->fields[i];
        if (fieldEntry.fieldName != nullptr && std::strcmp(fieldEntry.fieldName, fieldName) == 0)
            return &fieldEntry;
    }

    return nullptr;
}

inline const WioModuleField* WioFindModuleField(const WioModuleApi* api, const char* logicalTypeName, const char* fieldName)
{
    return WioFindModuleField(WioFindModuleType(api, logicalTypeName), fieldName);
}

inline const WioModuleMethod* WioFindModuleMethod(const WioModuleType* typeEntry, const char* methodName)
{
    if (typeEntry == nullptr || methodName == nullptr || typeEntry->methods == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < typeEntry->methodCount; ++i)
    {
        const WioModuleMethod& methodEntry = typeEntry->methods[i];
        if (methodEntry.methodName != nullptr && std::strcmp(methodEntry.methodName, methodName) == 0)
            return &methodEntry;
    }

    return nullptr;
}

inline const WioModuleMethod* WioFindModuleMethod(const WioModuleApi* api, const char* logicalTypeName, const char* methodName)
{
    return WioFindModuleMethod(WioFindModuleType(api, logicalTypeName), methodName);
}

inline const WioModuleMethod* WioFindModuleMethodOverload(const WioModuleType* typeEntry,
                                                          const char* methodName,
                                                          WioAbiType returnType,
                                                          std::uint32_t parameterCount,
                                                          const WioAbiType* parameterTypes)
{
    if (typeEntry == nullptr || methodName == nullptr || typeEntry->methods == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < typeEntry->methodCount; ++i)
    {
        const WioModuleMethod& methodEntry = typeEntry->methods[i];
        const WioModuleExport* exportEntry = methodEntry.exportEntry;
        if (methodEntry.methodName == nullptr || std::strcmp(methodEntry.methodName, methodName) != 0)
            continue;
        if (exportEntry == nullptr || exportEntry->invoke == nullptr)
            continue;
        if (exportEntry->returnType != returnType || exportEntry->parameterCount != parameterCount)
            continue;

        bool matches = true;
        for (std::uint32_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            const WioAbiType actualType = exportEntry->parameterTypes != nullptr
                ? exportEntry->parameterTypes[parameterIndex]
                : WIO_ABI_UNKNOWN;

            if (parameterTypes == nullptr || actualType != parameterTypes[parameterIndex])
            {
                matches = false;
                break;
            }
        }

        if (matches)
            return &methodEntry;
    }

    return nullptr;
}

inline const WioModuleMethod* WioFindModuleMethodOverload(const WioModuleApi* api,
                                                          const char* logicalTypeName,
                                                          const char* methodName,
                                                          WioAbiType returnType,
                                                          std::uint32_t parameterCount,
                                                          const WioAbiType* parameterTypes)
{
    return WioFindModuleMethodOverload(
        WioFindModuleType(api, logicalTypeName),
        methodName,
        returnType,
        parameterCount,
        parameterTypes
    );
}

inline const WioModuleConstructor* WioFindModuleConstructor(const WioModuleType* typeEntry,
                                                            std::uint32_t parameterCount,
                                                            const WioAbiType* parameterTypes)
{
    if (typeEntry == nullptr || typeEntry->constructors == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < typeEntry->constructorCount; ++i)
    {
        const WioModuleConstructor& constructorEntry = typeEntry->constructors[i];
        const WioModuleExport* exportEntry = constructorEntry.exportEntry;
        if (exportEntry == nullptr || exportEntry->invoke == nullptr)
            continue;

        if (exportEntry->parameterCount != parameterCount)
            continue;

        bool matches = true;
        for (std::uint32_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
        {
            const WioAbiType actualType = exportEntry->parameterTypes != nullptr
                ? exportEntry->parameterTypes[parameterIndex]
                : WIO_ABI_UNKNOWN;

            if (parameterTypes == nullptr || actualType != parameterTypes[parameterIndex])
            {
                matches = false;
                break;
            }
        }

        if (matches)
            return &constructorEntry;
    }

    return nullptr;
}
