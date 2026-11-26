#pragma once

#include "wio/bytecode/format.h"

#include <compare>
#include <cstdint>
#include <vector>

namespace wio::bytecode
{
    using StringId = std::uint32_t;

    struct Import
    {
        std::uint64_t stableId = 0;
        StringId logicalName = InvalidIndex;
        StringId sourcePath = InvalidIndex;
        StringId alias = InvalidIndex;
        std::vector<StringId> importedSymbols;
        std::uint8_t kind = 0;
        bool importAll = false;

        auto operator<=>(const Import&) const = default;
    };

    struct Export
    {
        std::uint64_t stableId = 0;
        StringId stableKey = InvalidIndex;
        StringId logicalName = InvalidIndex;
        StringId symbolName = InvalidIndex;
        std::uint8_t kind = 0;
        std::uint8_t role = 0;
        StringId roleName = InvalidIndex;
        std::uint32_t function = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::vector<std::uint32_t> parameterTypes;
        std::uint32_t returnType = InvalidIndex;
        std::vector<std::uint32_t> genericArguments;
        std::uint32_t callTableSlot = 0;
        bool isAsync = false;

        auto operator<=>(const Export&) const = default;
    };

    struct AttributeArgument
    {
        StringId name = InvalidIndex;
        StringId sourceText = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        bool usedDefault = false;

        auto operator<=>(const AttributeArgument&) const = default;
    };

    struct AttributeProcessor
    {
        std::uint64_t stableId = 0;
        StringId canonicalTypeName = InvalidIndex;
        StringId hookName = InvalidIndex;
        StringId hookMode = InvalidIndex;
        std::uint8_t phase = 0;
        std::uint32_t processorType = InvalidIndex;
        std::uint32_t hookFunction = InvalidIndex;
        std::uint32_t valueType = InvalidIndex;

        auto operator<=>(const AttributeProcessor&) const = default;
    };

    struct AttributeApplication
    {
        std::uint64_t stableId = 0;
        StringId canonicalName = InvalidIndex;
        StringId originParent = InvalidIndex;
        StringId selector = InvalidIndex;
        std::uint8_t targetKind = 0;
        std::uint8_t origin = 0;
        std::uint64_t targetStableId = 0;
        std::uint32_t targetType = InvalidIndex;
        std::uint32_t targetFunction = InvalidIndex;
        std::uint32_t parameterIndex = 0;
        std::uint32_t sourceOrder = 0;
        bool runtimeRetained = false;
        std::vector<AttributeArgument> arguments;
        std::vector<AttributeProcessor> processors;

        auto operator<=>(const AttributeApplication&) const = default;
    };

    struct ReflectedField
    {
        std::uint64_t stableId = 0;
        StringId name = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint8_t visibility = 0;
        bool isMutable = true;
        std::vector<std::uint64_t> attributes;

        auto operator<=>(const ReflectedField&) const = default;
    };

    struct ReflectedMethod
    {
        std::uint64_t stableId = 0;
        StringId name = InvalidIndex;
        std::uint32_t function = InvalidIndex;
        std::uint32_t returnType = InvalidIndex;
        std::vector<std::uint32_t> parameterTypes;
        std::uint32_t slot = 0;
        bool isAsync = false;
        std::uint8_t visibility = 0;
        std::vector<std::uint64_t> attributes;

        auto operator<=>(const ReflectedMethod&) const = default;
    };

    struct ReflectedCase
    {
        std::uint64_t stableId = 0;
        StringId name = InvalidIndex;
        std::vector<std::uint64_t> attributes;

        auto operator<=>(const ReflectedCase&) const = default;
    };

    struct Reflection
    {
        std::uint64_t stableTypeId = 0;
        StringId logicalName = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint8_t nominalKind = 0;
        bool isExported = false;
        bool runtimeVisible = true;
        std::vector<StringId> genericParameterNames;
        std::vector<std::uint32_t> genericArguments;
        std::vector<std::uint64_t> attributes;
        std::vector<ReflectedField> fields;
        std::vector<ReflectedMethod> methods;
        std::vector<ReflectedCase> cases;

        auto operator<=>(const Reflection&) const = default;
    };

    struct ResourceBinding
    {
        StringId name = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint8_t access = 0;

        auto operator<=>(const ResourceBinding&) const = default;
    };

    struct StageRun
    {
        StringId targetName = InvalidIndex;
        StringId methodName = InvalidIndex;
        std::uint32_t targetType = InvalidIndex;
        std::uint32_t function = InvalidIndex;
        std::vector<ResourceBinding> resources;
        bool applicationTarget = false;
        bool acceptsDelta = true;

        auto operator<=>(const StageRun&) const = default;
    };

    struct Stage
    {
        std::uint64_t stableId = 0;
        StringId name = InvalidIndex;
        StringId after = InvalidIndex;
        std::uint64_t fixedHzBits = 0;
        std::uint32_t order = 0;
        std::uint8_t kind = 0;
        std::uint8_t affinity = 0;
        bool legacyExplicit = false;
        std::vector<StageRun> runs;

        auto operator<=>(const Stage&) const = default;
    };

    struct System
    {
        std::uint64_t stableId = 0;
        StringId logicalName = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint32_t start = InvalidIndex;
        std::uint32_t update = InvalidIndex;
        std::uint32_t close = InvalidIndex;

        auto operator<=>(const System&) const = default;
    };

    struct Application
    {
        std::uint64_t stableId = 0;
        StringId logicalName = InvalidIndex;
        std::uint32_t type = InvalidIndex;
        std::uint32_t construct = InvalidIndex;
        std::uint32_t entry = InvalidIndex;
        std::uint32_t start = InvalidIndex;
        std::uint32_t update = InvalidIndex;
        std::uint32_t close = InvalidIndex;
        std::uint32_t exit = InvalidIndex;
        std::vector<std::uint32_t> systems;
        std::vector<Stage> stages;
        bool hostOwnsStorage = true;
        bool nonBlockingScheduling = true;

        auto operator<=>(const Application&) const = default;
    };

    struct Lifecycle
    {
        std::uint32_t apiVersion = InvalidIndex;
        std::uint32_t load = InvalidIndex;
        std::uint32_t update = InvalidIndex;
        std::uint32_t unload = InvalidIndex;
        std::uint32_t saveState = InvalidIndex;
        std::uint32_t restoreState = InvalidIndex;
        std::uint32_t stateSchemaVersion = 0;

        auto operator<=>(const Lifecycle&) const = default;
    };

    struct Contract
    {
        std::vector<Import> imports;
        std::vector<Export> exports;
        std::vector<AttributeApplication> attributes;
        std::vector<Reflection> reflection;
        std::vector<System> systems;
        bool hasApplication = false;
        Application application;
        Lifecycle lifecycle;
        std::uint64_t callTableStableId = 0;
        std::vector<std::uint64_t> callTableEntries;

        auto operator<=>(const Contract&) const = default;
    };
} // namespace wio::bytecode
