#include "scope.h"
#include "../base/exception.h"

#include "../variables/variable_base.h"
#include "../variables/function.h"

#include "main_table.h"

namespace wio
{
    static ref<symbol_table> s_null_member_table = nullptr;

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

    void scope::add_member_table(const std::string& id, ref<symbol_table> table)
    {
        if (m_member_table_map.find(id) != m_member_table_map.end())
            throw invalid_declaration_error("Error: Table '" + id + "' already defined in this scope.");
        m_member_table_map[id] = table;
    }

    void scope::add_member_table_to_global(const std::string& id, ref<symbol_table> table)
    {
        if (m_member_table_map.find(id) != m_member_table_map.end())
            throw invalid_declaration_error("Error: Symbol '" + id + "' already defined in this scope.");

        if (m_parent && m_type != scope_type::global)
        {
            ref<scope> glob = m_parent;
            while (glob->get_type() != scope_type::global)
                glob = glob->m_parent;
            if (glob->lookup(id))
                throw invalid_declaration_error("Error: Symbol '" + id + "' already defined in this scope.");
            glob->add_member_table(id, table);
        }
        else
        {
            m_member_table_map[id] = table;
        }
    }

    ref<symbol_table>& scope::lookup_member_table(const std::string& id)
    {
        auto it = m_member_table_map.find(id);
        if (it != m_member_table_map.end())
            return (it->second);

        if (m_parent)
        {
            if (m_type == scope_type::function_body)
            {
                ref<scope> parent = m_parent;
                while (parent && parent->get_type() != scope_type::global)
                    parent = parent->m_parent;
                if (parent)
                    return parent->lookup_member_table(id);
                return s_null_member_table;
            }
            return m_parent->lookup_member_table(id);
        }

        return s_null_member_table;
    }

    bool scope::is_null_member_table(ref<symbol_table> table)
    {
        return table == s_null_member_table;
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
                while (parent && parent->get_type() != scope_type::global)
                    parent = parent->m_parent;
                if(parent)
                    return parent->lookup(name);
                return nullptr;
            }
            return m_parent->lookup(name);
        }

        return main_table::get().search_builtin(name);
    }

    symbol* scope::lookup_current(const std::string& name)
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end())
            return &(it->second);

        return nullptr;
    }

    symbol* scope::lookup_function(const std::string& name, const std::vector<function_param>& parameters)
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end() && it->second.var_ref->get_base_type() == variable_base_type::function)
        {
            if (auto f = std::dynamic_pointer_cast<var_function>(it->second.var_ref))
            {
                if(f->compare_parameters(parameters))
                    return &(it->second);
            }
            else if (auto ol = std::dynamic_pointer_cast<overload_list>(it->second.var_ref))
            {
                for (size_t i = 0; i < ol->count(); ++i)
                {
                    if (auto fun = std::dynamic_pointer_cast<var_function>(ol->get(i)->var_ref))
                    {
                        if (fun->compare_parameters(parameters))
                            return &(it->second);
                    }
                }
            }
        }

        if (m_parent)
        {
            if (m_type == scope_type::function_body)
            {
                ref<scope> parent = m_parent;
                while (parent && parent->get_type() != scope_type::global)
                    parent = parent->m_parent;
                if (parent)
                    return parent->lookup_function(name, parameters);
                return nullptr;
            }
            return m_parent->lookup_function(name, parameters);
        }

        return main_table::get().search_builtin_function(name, parameters);
    }
}