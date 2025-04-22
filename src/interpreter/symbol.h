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

        bool is_local() const;
        bool is_global() const;
        bool is_ref() const;

        void set_local(bool flag);
        void set_global(bool flag);
        void set_ref(bool flag);

        symbol(ref<variable_base> var_ref = nullptr, bool is_local = false, bool is_global = false, bool is_ref = false)
            : var_ref(var_ref), flags({ is_local, is_global, is_ref }) {
        }

    private:
        packed_bool flags = {}; // 1-> is_local --- 2-> is_global --- 3-> original position passed
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