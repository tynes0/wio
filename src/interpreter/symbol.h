#pragma once

#include "../base/base.h"
#include "../variables/variable_type.h"
#include "../variables/function_param.h"

#include <string>
#include <map>

namespace wio
{
    class variable_base;

    struct symbol
    {
        friend class scope;

        ref<variable_base> var_ref;
        packed_bool flags = {}; // 1-> is_local --- 2-> is_global --- 3-> original position passed

        symbol(ref<variable_base> var_ref = nullptr, packed_bool flags = {})
            : var_ref(var_ref), flags(flags) {
        }
    };

    using symbol_map = std::map<std::string, symbol>;

    class symbol_table
    {
    public:
        void insert(const std::string& name, const symbol& symbol);
        symbol* lookup(const std::string& name);

        symbol_map& get_symbols() { return m_symbols; }
        void set_symbols(const symbol_map& table) { m_symbols = table; }
    private:
        std::map<std::string, symbol> m_symbols;
    };
}