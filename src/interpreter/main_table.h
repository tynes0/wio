#pragma once

#include "../base/base.h"
#include "scope.h"

#include <map>
#include <set>

namespace wio
{
   	class main_table
	{
	public:
        main_table();

        static main_table& get();

        void add(id_t id);
        void add_imported_module(id_t id);

        void insert(id_t cur_id, const std::string& name, const symbol& symbol);
        void insert_to_global(id_t cur_id, const std::string& name, const symbol& symbol);

        symbol* lookup(id_t cur_id, const std::string& name, id_t pass_id = 0);
        symbol* lookup_only_global(id_t cur_id, const std::string& name, id_t pass_id = 0);
        symbol* lookup_current_and_global(id_t cur_id, const std::string& name, id_t pass_id = 0);
        symbol* lookup_builtin(const std::string& name);

        void set_scope(id_t cur_id, ref<scope> scpe) { m_table[cur_id] = scpe; }

        scope_type get_type(id_t cur_id) const { return m_table.at(cur_id)->m_type; }
        ref<scope>& get_parent(id_t cur_id) { return m_table.at(cur_id)->m_parent; }

        ref<scope>& get_scope(id_t cur_id) { return m_table[cur_id]; }
        ref<scope>& get_builtin_scope();

        void enter_scope(id_t cur_id, scope_type type);
        ref<scope> exit_scope(id_t cur_id);
	private:
        ref<scope> search(id_t id);
        ref<scope> search_checked(id_t id);
        bool is_imported(id_t id);

		std::map<id_t, ref<scope>> m_table;
        std::set<id_t> m_imported_modules;
	};
}