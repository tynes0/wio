#include "scope.h"
#include "exception.h"

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

    void scope::insert_def(const std::string& name, const var_func_definition& def)
    {
        if (m_definitions.find(name) != m_definitions.end())
            throw invalid_declaration_error("Error: Func '" + name + "' already defined in this scope.");
        m_definitions[name] = def;
    }

    void scope::insert_def_to_global(const std::string& name, const var_func_definition& def)
    {
        if (m_definitions.find(name) != m_definitions.end())
            throw invalid_declaration_error("Error: Func '" + name + "' already defined in this scope.");

        if (m_parent && m_type != scope_type::global)
        {
            ref<scope> glob = m_parent;
            while (glob->get_type() != scope_type::global)
                glob = glob->m_parent;
            if (glob->lookup_def(name))
                throw invalid_declaration_error("Error: Func '" + name + "' already defined in this scope.");
            glob->insert_def(name, def);
        }
        else
        {
            m_definitions[name] = def;
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

        return nullptr;
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

        return nullptr;
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

        return nullptr;
    }

    var_func_definition* scope::lookup_def(const std::string& name)
    {
        auto it = m_definitions.find(name);
        if (it != m_definitions.end())
            return &it->second;
        if (m_parent)
        {
            if (m_type == scope_type::function_body)
            {
                ref<scope> parent = m_parent;
                while (parent->get_type() != scope_type::global)
                    parent = parent->m_parent;
                return parent->lookup_def(name);
            }
            return m_parent->lookup_def(name);
        }
        return nullptr;
    }
}