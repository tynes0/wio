#pragma once

#include "wio/sema/symbol.h"

#include <string_view>

namespace wio::sema::scope_lookup
{
    Ref<Symbol> resolveQualifiedSymbol(const Ref<Scope>& startScope, std::string_view qualifiedName);
}
