#pragma once

#include <string>
#include <vector>

#include "type.h"
#include "wio/ast/ast.h"
#include "wio/common/auto_flags.h"

namespace wio::sema
{
#define SYMBOL_FLAGS(X) X(isMutable) X(isShadowed) X(isStd) X(isGlobal) X(isPublic) X(isPrivate) X(isProtected) \
    X(isReadOnly) X(isOverride) X(isInterface) X(isEnum) X(isFlagset) X(isFlag)
    DEFINE_FLAGS(SymbolFlags, SYMBOL_FLAGS);
#undef SYMBOL_FLAGS
    
    enum class SymbolKind : uint8_t { Variable, Function, Struct, TypeAlias, Parameter, Namespace, FunctionGroup };
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
        Ref<Type> aliasTargetType = nullptr;

        Symbol() = default;
        Symbol(std::string name, Ref<Type> type, SymbolKind kind, SymbolFlags flags, common::Location loc, Ref<Scope> innerScope = nullptr)
            : name(std::move(name)), type(std::move(type)), kind(kind), flags(flags), definitionLoc(loc), innerScope(std::move(innerScope))
        {
        }
    };
}

MakeFrenumWithNamespace(wio::sema, SymbolKind, Variable, Function, Struct, TypeAlias, Parameter, Namespace, FunctionGroup)
MakeFrenumWithNamespace(wio::sema, ScopeKind, Global, Function, Block, Struct)
