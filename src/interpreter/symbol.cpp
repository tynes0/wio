#include "symbol.h"

#include "../base/exception.h"

namespace wio
{
    bool symbol::is_local() const
    {
        return flags.b1;
    }

    bool symbol::is_global() const
    {
        return flags.b2;
    }

    bool symbol::is_ref() const
    {
        return flags.b3;
    }

    void symbol::set_local(bool flag)
    {
        flags.b1 = flag;
    }

    void symbol::set_global(bool flag)
    {
        flags.b2 = flag;
    }

    void symbol::set_ref(bool flag)
    {
        flags.b3 = flag;
    }

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
