#include "main_table.h"

#include "../base/exception.h"

namespace wio
{
    static constexpr id_t s_builtin_scope_id = static_cast<id_t>(0);

    main_table::main_table()
    {
        m_table[s_builtin_scope_id] = make_ref<scope>(scope_type::builtin);
    }

    main_table& main_table::get()
    {
        static main_table s_main_table;
        return s_main_table;
    }

    void main_table::add(id_t id)
    {
        if (search(id))
            throw exception("Undefined behavior: This scope already exists!");
        m_table[id] = make_ref<scope>(scope_type::global);
    }

    void main_table::add_imported_module(id_t id)
    {
        m_imported_modules.insert(id);
    }

    void main_table::insert(id_t cur_id, const std::string& name, const symbol& symbol)
    {
        ref<scope> current = search(cur_id);
        current->insert(name, symbol);
    }

    void main_table::insert_to_global(id_t cur_id, const std::string& name, const symbol& symbol)
    {
        ref<scope> current = search(cur_id);
        current->insert_to_global(name, symbol);
    }

    symbol* main_table::lookup(id_t cur_id, const std::string& name, id_t pass_id)
    {
        ref<scope> current = search(cur_id);
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
                if ((!item->flags.b1 && is_imported(scp.first)) || scp.first == pass_id)
                    return item;
            }
        }

        return nullptr;
    }

    symbol* main_table::lookup_only_global(id_t cur_id, const std::string& name, id_t pass_id)
    {
        ref<scope> current = search(cur_id);
        symbol* sym = current->lookup_only_global(name);

        if (sym)
            return sym;

        for (auto& scp : m_table)
        {
            if (scp.first == cur_id)
                continue;

            symbol* item = scp.second->lookup_only_global(name);

            if (item)
            {
                if ((!item->flags.b1 && is_imported(scp.first)) || scp.first == pass_id)
                    return item;
            }
        }

        return nullptr;
    }

    symbol* main_table::lookup_current_and_global(id_t cur_id, const std::string& name, id_t pass_id)
    {
        ref<scope> current = search(cur_id);
        symbol* sym = current->lookup_current_and_global(name);

        if (sym)
            return sym;

        for (auto& scp : m_table)
        {
            if (scp.first == cur_id)
                continue;

            symbol* item = scp.second->lookup_current_and_global(name);

            if (item)
            {
                if ((!item->flags.b1 && is_imported(scp.first)) || scp.first == pass_id)
                    return item;
            }
        }

        return nullptr;
    }

    symbol* main_table::lookup_builtin(const std::string& name)
    {
        auto& symbols = m_table[s_builtin_scope_id]->get_symbols();

        auto it = symbols.find(name);
        if (it != symbols.end())
            return &(it->second);

        return nullptr;
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

    ref<scope> main_table::search(id_t id)
    {
        auto it = m_table.find(id);
        if (it != m_table.end())
            return it->second;
        return nullptr;
    }

    ref<scope> main_table::search_checked(id_t id)
    {
        ref<scope> cur = search(id);
        if(!cur)
            throw exception("Undefined behavior: This scope is NOT exists!");
        return cur;
    }

    bool main_table::is_imported(id_t id)
    {
        return m_imported_modules.find(id) != m_imported_modules.end();
    }
}

