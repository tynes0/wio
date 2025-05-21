#include "main_table.h"

#include "../base/exception.h"

namespace wio
{
    static constexpr id_t s_builtin_scope_id = static_cast<id_t>(0);
    static main_table s_main_table;

    main_table::main_table()
    {
        m_table[s_builtin_scope_id] = make_ref<scope>(scope_type::builtin);
    }

    main_table& main_table::get()
    {
        return s_main_table;
    }

    void main_table::add(id_t id)
    {
        if (find_scope(id))
            throw exception("Undefined behavior: This scope already exists!");
        m_table[id] = make_ref<scope>(scope_type::global);
    }

    void main_table::add_imported_module(id_t id)
    {
        m_imported_modules.insert(id);
    }

    void main_table::insert(id_t cur_id, const std::string& name, const symbol& symbol)
    {
        ref<scope> current = find_scope_checked(cur_id);
        current->insert(name, symbol);
    }

    void main_table::insert_to_global(id_t cur_id, const std::string& name, const symbol& symbol)
    {
        ref<scope> current = find_scope_checked(cur_id);
        current->insert_to_global(name, symbol);
    }

    symbol* main_table::search(id_t cur_id, const std::string& name, id_t pass_id)
    {
        ref<scope> current = find_scope_checked(cur_id);
        symbol* sym = current->lookup(name);

        if (sym)
            return sym;

        for (auto& scp : m_table)
        {
            if (scp.first == cur_id)
                continue;

            symbol* item = scp.second->lookup(name);

            if (item)
            {
                if ((!item->is_local() && is_imported(scp.first)) || scp.first == pass_id)
                    return item;
            }
        }

        return nullptr;
    }

    symbol* main_table::search_current(id_t cur_id, const std::string& name, id_t pass_id)
    {
        ref<scope> current = find_scope_checked(cur_id);
        symbol* sym = current->lookup_current(name);

        if (sym)
            return sym;

        for (auto& scp : m_table)
        {
            if (scp.first == cur_id)
                continue;

            symbol* item = scp.second->lookup_current(name);

            if (item)
            {
                if ((!item->is_local() && is_imported(scp.first)) || scp.first == pass_id)
                    return item;
            }
        }

        return nullptr;
    }

    symbol* main_table::search_builtin(const std::string& name)
    {
        auto& symbols = m_table[s_builtin_scope_id]->get_symbols();

        auto it = symbols.find(name);
        if (it != symbols.end())
            return &(it->second);

        return nullptr;
    }

    symbol* main_table::search_function(id_t cur_id, const std::string& name, const std::vector<function_param>& parameters, id_t pass_id)
    {
        ref<scope> current = find_scope_checked(cur_id);
        symbol* sym = current->lookup_function(name, parameters);

        if (sym)
            return sym;

        for (auto& scp : m_table)
        {
            if (scp.first == cur_id)
                continue;

            symbol* item = scp.second->lookup_function(name, parameters);

            if (item)
            {
                if ((!item->is_local() && is_imported(scp.first)) || scp.first == pass_id)
                    return item;
            }
        }

        return nullptr;
    }

    symbol* main_table::search_current_function(id_t cur_id, const std::string& name, const std::vector<function_param>& parameters)
    {
        ref<scope> current = find_scope_checked(cur_id);
        return current->lookup_function(name, parameters);
    }

    symbol* main_table::search_builtin_function(const std::string& name, const std::vector<function_param>& parameters)
    {
        auto& symbols = m_table[s_builtin_scope_id]->get_symbols();

        auto it = symbols.find(name);
        if (it != symbols.end() && it->second.var_ref->get_base_type() == variable_base_type::function)
        {
            if (auto f = std::dynamic_pointer_cast<var_function>(it->second.var_ref))
            {
                if (f->compare_parameters(parameters))
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
        return nullptr;
    }

    std::pair<bool, symbol*> main_table::is_function_valid(id_t cur_id, const std::string name, const std::vector<function_param>& parameters, id_t pass_id)
    {
        std::pair<bool, symbol*> result_pair = std::make_pair<bool, symbol*>(false, nullptr);

        ref<scope> current = find_scope_checked(cur_id);
        symbol* sym = current->lookup(name);

        symbol* result = nullptr;

        if (sym)
        {
            if (auto ol = std::dynamic_pointer_cast<overload_list>(sym->var_ref))
            {
                if (auto* ol_sym = ol->find(parameters))
                {
                    ref<var_function> fref = std::dynamic_pointer_cast<var_function>(ol_sym->var_ref);
                    if(fref->declared() && !fref->early_declared())
                        return result_pair;
                }

                result = sym;
            }
            else
            {
                return result_pair;
            }
        }

        for (auto& scp : m_table)
        {
            if (scp.first == cur_id)
                continue;

            symbol* item = scp.second->lookup(name);

            if (item)
            {
                if ((!item->is_local() && is_imported(scp.first)) || scp.first == pass_id)
                {
                    if (auto ol = std::dynamic_pointer_cast<overload_list>(item->var_ref))
                    {
                        if (auto* ol_sym = ol->find(parameters))
                        {
                            ref<var_function> fref = std::dynamic_pointer_cast<var_function>(ol_sym->var_ref);
                            if (fref->declared() && !fref->early_declared())
                                return result_pair;

                            if (!fref->declared())
                                result = item;
                        }
                    }
                    else
                    {
                        return result_pair;
                    }
                }
            }
        }

        result_pair.first = true;
        result_pair.second = result;

        return result_pair;
    }

    ref<scope>& main_table::get_builtin_scope()
    {
        return m_table[s_builtin_scope_id];
    }

    void main_table::enter_scope(id_t cur_id, scope_type type)
    {
        m_table[cur_id] = make_ref<scope>(type, m_table[cur_id]);
    }

    ref<scope> main_table::exit_scope(id_t cur_id)
    {
        ref<scope> child = m_table[cur_id];

        m_table[cur_id] = m_table[cur_id]->get_parent();
        return child;
    }

    ref<scope> main_table::find_scope(id_t id)
    {
        auto it = m_table.find(id);
        if (it != m_table.end())
            return it->second;
        return nullptr;
    }

    ref<scope> main_table::find_scope_checked(id_t id)
    {
        ref<scope> cur = find_scope(id);
        if(!cur)
            throw exception("Undefined behavior: This scope is NOT exists!");
        return cur;
    }

    bool main_table::is_imported(id_t id)
    {
        return m_imported_modules.find(id) != m_imported_modules.end();
    }
}

