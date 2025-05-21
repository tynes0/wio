#pragma once

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <unordered_map>

#include "../base/base.h"
#include "../variables/variable_type.h"
#include "../variables/function_param.h"

#include "symbol.h"

namespace wio
{
    enum class scope_type 
    {
        builtin,
        global,
        local,
        function,
        function_body,
        block
    };

    class scope 
    {
    public:
        friend class main_table;

        using constant_member_table_map = std::unordered_map<std::string, ref<symbol_table>>;

        scope(scope_type type, ref<scope> parent = nullptr) : m_type(type), m_parent(parent) {}

        void insert(const std::string& name, const symbol& symbol);
        void insert_to_global(const std::string& name, const symbol& symbol);

        void add_member_table(const std::string& id, ref<symbol_table> table);
        void add_member_table_to_global(const std::string& id, ref<symbol_table> table);
        ref<symbol_table>& lookup_member_table(const std::string& id);
        bool is_null_member_table(ref<symbol_table> table);

        symbol* lookup(const std::string& name);
        symbol* lookup_current(const std::string& name);
        symbol* lookup_function(const std::string& name, const std::vector<function_param>& parameters);

        scope_type get_type() const { return m_type; }
        ref<scope> get_parent() { return m_parent; }

        symbol_map& get_symbols() { return m_symbols; }
        void set_symbols(const symbol_map& table) { m_symbols = table; }

        constant_member_table_map& get_member_table_map() { return m_member_table_map; }
    private:
        symbol_map m_symbols;
        constant_member_table_map m_member_table_map;
        ref<scope> m_parent;
        scope_type m_type;
    };
} // namespace wio