#include "scope.h"
#include "../base/exception.h"

#include "../variables/variable_base.h"
#include "../variables/function.h"

namespace wio
{
    void scope::insert(const std::string& name, const symbol& symbol)
    {
        if (m_symbols.find(name) != m_symbols.end())
            throw invalid_declaration_error("Error: Symbol '" + name + "' already defined in this scope.");
        m_symbols[name] = symbol;
    }

    void scope::insert_to_global(const std::string& name, const symbol& symbol)
    {
        if (m_symbols.find(name) != m_symbols.end())
            throw invalid_declaration_error("Error: Symbol '" + name + "' already defined in this scope.");

        if (m_parent && m_type != scope_type::global)
        {
            ref<scope> glob = m_parent;
            while (glob->get_type() != scope_type::global)
                glob = glob->m_parent;
            if (glob->lookup(name))
                throw invalid_declaration_error("Error: Symbol '" + name + "' already defined in this scope.");
            glob->insert(name, symbol);
        }
        else
        {
            m_symbols[name] = symbol;
        }
    }

    symbol* scope::lookup(const std::string& name)
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end())
            return &(it->second);

        if (m_parent)
        {
            if (m_type == scope_type::function_body)
            {
                ref<scope> parent = m_parent;
                while (parent->get_type() != scope_type::global)
                    parent = parent->m_parent;
                return parent->lookup(name);
            }
            return m_parent->lookup(name);
        }

        return lookup_builtin(name);
    }

    symbol* scope::lookup_only_global(const std::string& name)
    {
        ref<scope> parent = m_parent;

        if (parent)
        {
            while (parent->get_type() != scope_type::global)
                parent = parent->m_parent;
            return parent->lookup(name);
        }

        return lookup_builtin(name);
    }

    symbol* scope::lookup_current_and_global(const std::string& name)
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end())
            return &(it->second);

        ref<scope> parent = m_parent;

        if (parent)
        {
            while (parent->get_type() != scope_type::global)
                parent = parent->m_parent;
            return parent->lookup(name);
        }

        return lookup_builtin(name);
    }

    symbol* scope::lookup_builtin(const std::string& name)
    {
        auto it = builtin_scope->m_symbols.find(name);
        if (it != builtin_scope->m_symbols.end())
            return &(it->second);
        return nullptr;
    }
}