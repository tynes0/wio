#include "symbol.h"

#include "../base/exception.h"

namespace wio
{
    void symbol_table::insert(const std::string& name, const symbol& symbol)
    {
        if (m_symbols.find(name) != m_symbols.end())
            throw invalid_declaration_error("Error: Symbol '" + name + "' already defined.");
        m_symbols[name] = symbol;
    }

    symbol* symbol_table::lookup(const std::string& name)
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end())
            return &(it->second);

        return nullptr;
    }
}
