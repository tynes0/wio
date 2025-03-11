#include "scope.h"
#include <iostream> // Hata mesajlarý için

namespace wio
{
    void scope::insert(const std::string& name, const symbol& symbol)
    {
        if (m_symbols.find(name) != m_symbols.end())
            throw std::runtime_error("Error: Symbol '" + name + "' already defined in this scope.");
        m_symbols[name] = symbol;
    }

    symbol* scope::lookup(const std::string& name)
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end())
            return &(it->second);

        if (m_parent)
            return m_parent->lookup(name);

        return nullptr;
    }
}