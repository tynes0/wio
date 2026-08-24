#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "type.h"
#include "wio/ast/ast.h"
#include "wio/common/auto_flags.h"

namespace wio::sema
{
#define SYMBOL_FLAGS(X) X(isMutable) X(isConst) X(isShadowed) X(isStd) X(isGlobal) X(isPublic) X(isPrivate) X(isProtected) \
    X(isReadOnly) X(isOverride) X(isInterface) X(isEnum) X(isFlagset) X(isFlag) X(isParameterPack) X(isExtension)
    DEFINE_FLAGS(SymbolFlags, SYMBOL_FLAGS);
#undef SYMBOL_FLAGS
    
    enum class SymbolKind : uint8_t { Variable, Function, Struct, TypeAlias, Parameter, Namespace, FunctionGroup, Attribute };
    enum class ScopeKind : uint8_t { Global, Function, Block, Struct };

    class Scope;

    struct Symbol : RefCountedObject
    {
        std::string name;       // x
        std::string scopePath;
        Ref<Type> type = nullptr; // int (sema::Type pointer)
        SymbolKind kind = SymbolKind::Variable; // Variable
        SymbolFlags flags;
        common::Location definitionLoc;
        Ref<Scope> innerScope;

        std::vector<Ref<Symbol>> overloads;
        std::vector<std::string> genericParameterNames;
        std::vector<Ref<Type>> genericParameterTypes;
        // Aligned with genericParameterNames. Null entries have no default.
        std::vector<Ref<Type>> genericParameterDefaults;
        bool hasGenericParameterPack = false;
        std::vector<std::vector<Ref<Type>>> resolvedGenericInstantiations;
        Ref<Type> aliasTargetType = nullptr;
        Ref<Type> extensionTargetType = nullptr;
        std::string extensionMemberName;
        Ref<Symbol> extensionImplementation = nullptr;
        // Interface/base declarations whose C++ virtual slots this method
        // implements. Generic base methods may have a different mangled name
        // before their owner type arguments are substituted, so codegen emits
        // small forwarding bridges for these symbols.
        std::vector<WeakRef<Symbol>> overriddenSymbols;

        std::vector<std::string> attributeTargets;
        // Realm-qualified source identity used by reflection and policy
        // matching. Unlike scopePath this is never C++-mangled.
        std::string attributeCanonicalName;
        std::vector<std::string> attributeRetention;
        std::vector<std::string> attributeConflictGroups;
        std::vector<NodePtr<AttributeStatement>> attributeComposition;
        std::vector<std::string> attributeRequiredAttributes;
        std::vector<std::string> attributeRequiredAnyAttributes;
        std::vector<std::string> attributeConflictingAttributes;
        std::vector<std::string> attributeOnlyWithAttributes;
        std::vector<std::string> attributeBeforeAttributes;
        std::vector<std::string> attributeAfterAttributes;
        std::vector<std::string> attributeImpliedAttributes;
        std::vector<std::string> attributeProcessorTypes;
        std::vector<std::string> attributeProcessorPhases;
        std::vector<std::string> attributeProcessorCanonicalTypes;
        std::vector<std::string> attributeProcessorCppTypes;
        // Aligned with processor phases. -1 means non-validator, 0 rejects,
        // and 1 accepts. Validator bodies are evaluated by the compiler and
        // are never emitted or executed at runtime.
        std::vector<std::int8_t> attributeProcessorValidationResults;
        std::vector<std::string> attributeProcessorDiagnostics;
        std::vector<std::string> attributeParameterNames;
        std::vector<Ref<Type>> attributeParameterTypes;
        std::vector<bool> attributeParameterHasDefault;
        // Aligned with attributeParameterNames. Invalid tokens represent
        // required parameters or defaults that could not be folded.
        std::vector<Token> attributeParameterDefaults;
        size_t attributeCardinalityMin = 0;
        size_t attributeCardinalityMax = 1;
        bool attributeHasExplicitCardinality = false;
        bool attributeRepeatable = false;
        bool attributeInherited = false;
        bool attributeScoped = false;

        Symbol() = default;
        Symbol(std::string name, Ref<Type> type, SymbolKind kind, SymbolFlags flags, common::Location loc, Ref<Scope> innerScope = nullptr)
            : name(std::move(name)), type(std::move(type)), kind(kind), flags(flags), definitionLoc(loc), innerScope(std::move(innerScope))
        {
        }
    };
}

MakeFrenumWithNamespace(wio::sema, SymbolKind, Variable, Function, Struct, TypeAlias, Parameter, Namespace, FunctionGroup, Attribute)
MakeFrenumWithNamespace(wio::sema, ScopeKind, Global, Function, Block, Struct)
