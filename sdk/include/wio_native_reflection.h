#pragma once
#include "wio_native_abi.h"

// Additive canonical reflection sidecar. Does not change WioModuleApi v11.
inline constexpr std::uint32_t WIO_NATIVE_REFLECTION_VERSION = 1;

struct WioNativeFieldDescriptor {
    std::uint64_t stableId;
    const char* name;
    std::uint64_t typeId;
    std::uint32_t visibility;
    bool mutableValue;
    WioNativeAbiThunk get;
    WioNativeAbiThunk set;
};

struct WioNativeMethodDescriptor {
    std::uint64_t stableId;
    const char* name;
    std::uint64_t resultTypeId;
    std::uint32_t parameterCount;
    const std::uint64_t* parameterTypes;
    std::uint32_t slot;
    bool asynchronous;
    std::uint32_t visibility;
    WioNativeAbiThunk invoke;
};

struct WioNativeCaseDescriptor {
    std::uint64_t stableId;
    const char* name;
    std::uint64_t rawValue;
};

struct WioNativeConstructorDescriptor {
    std::uint64_t stableId;
    std::uint32_t parameterCount;
    const std::uint64_t* parameterTypes;
    WioNativeAbiThunk invoke;
};

struct WioNativeTypeDescriptor {
    std::uint64_t stableId;
    const char* name;
    std::uint32_t kind;
    bool exported;
    std::uint64_t valueSize;
    std::uint64_t valueAlignment;
    std::uint32_t fieldCount;
    const WioNativeFieldDescriptor* fields;
    std::uint32_t methodCount;
    const WioNativeMethodDescriptor* methods;
    std::uint32_t caseCount;
    const WioNativeCaseDescriptor* cases;
    std::uint32_t constructorCount;
    const WioNativeConstructorDescriptor* constructors;
};

struct WioNativeAttributeArgument {
    const char* name;
    const char* value;
    std::uint64_t typeId;
    bool usedDefault;
};

struct WioNativeAttributeProcessor {
    std::uint64_t stableId;
    const char* type;
    const char* hook;
    const char* mode;
    std::uint32_t phase;
};

struct WioNativeAttributeDescriptor {
    std::uint64_t stableId;
    const char* name;
    std::uint64_t targetId;
    std::uint32_t targetKind;
    std::uint32_t origin;
    const char* originParent;
    const char* selector;
    std::uint32_t parameterIndex;
    std::uint32_t order;
    std::uint32_t argumentCount;
    const WioNativeAttributeArgument* arguments;
    std::uint32_t processorCount;
    const WioNativeAttributeProcessor* processors;
};

struct WioNativeReflectionApi {
    std::uint32_t version;
    std::uint32_t typeCount;
    const WioNativeTypeDescriptor* types;
    std::uint32_t attributeCount;
    const WioNativeAttributeDescriptor* attributes;
};

using WioGetNativeReflectionApiFn = const WioNativeReflectionApi* (*)();
