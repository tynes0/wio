#pragma once

#include <string>

#include "type.h"
#include "wio/ast/ast.h"
#include "wio/common/auto_flags.h"

namespace wio::sema
{
#define SYMBOL_FLAGS(X) X(isMutable) X(isShadowed) X(isStd)
    DEFINE_FLAGS(SymbolFlags, SYMBOL_FLAGS);
#undef SYMBOL_FLAGS
    
    enum class SymbolKind : uint8_t { Variable, Function, Struct, Parameter, Namespace };
    enum class ScopeKind : uint8_t { Global, Function, Block, Class };

    class Scope;

    struct Symbol : RefCountedObject
    {
        std::string name;       // x
        Ref<Type> type = nullptr; // int (sema::Type pointer)
        SymbolKind kind = SymbolKind::Variable; // Variable
        SymbolFlags flags;
        common::Location definitionLoc;
        Ref<Scope> innerScope;

        Symbol() = default;
        Symbol(std::string name, Ref<Type> type, SymbolKind kind, SymbolFlags flags, common::Location loc, Ref<Scope> innerScope = nullptr)
            : name(std::move(name)), type(std::move(type)), kind(kind), flags(flags), definitionLoc(loc), innerScope(std::move(innerScope))
        {
        }
    };
}

MakeFrenumWithNamespace(wio::sema, SymbolKind, Variable, Function, Struct, Parameter, Namespace)
MakeFrenumWithNamespace(wio::sema, ScopeKind, Global, Function, Block, Class)