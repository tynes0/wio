#pragma once

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <utility>

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

        scope(scope_type type, ref<scope> parent = nullptr) : m_type(type), m_parent(parent) {}

        void insert(const std::string& name, const symbol& symbol);
        void insert_to_global(const std::string& name, const symbol& symbol);

        symbol* lookup(const std::string& name);
        symbol* lookup_only_global(const std::string& name);
        symbol* lookup_current_and_global(const std::string& name);

        scope_type get_type() const { return m_type; }
        ref<scope> get_parent() { return m_parent; }

        symbol_map& get_symbols() { return m_symbols; }
        void set_symbols(const symbol_map& table) { m_symbols = table; }
    private:
        symbol_map m_symbols;
        ref<scope> m_parent;
        scope_type m_type;
    };
} // namespace wio