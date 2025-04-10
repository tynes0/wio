#pragma once

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <utility>

#include "../base/base.h"
#include "../variables/variable_type.h"
#include "../variables/function_param.h"

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

    class variable_base;

    struct symbol 
    {
        friend class scope;

        ref<variable_base> var_ref;
        packed_bool flags = {}; // 1-> is_local --- 2-> is_global --- 3-> original position passed

        symbol(ref<variable_base> var_ref = nullptr, packed_bool flags = {})
            : var_ref(var_ref), flags(flags) {}
    };

    using symbol_table_t = std::map<std::string, symbol>;

    class scope 
    {
    public:
        scope(scope_type type, ref<scope> parent = nullptr) : m_type(type), m_parent(parent) {}

        void insert(const std::string& name, const symbol& symbol);
        void insert_to_global(const std::string& name, const symbol& symbol);

        symbol* lookup(const std::string& name);
        symbol* lookup_only_global(const std::string& name);
        symbol* lookup_current_and_global(const std::string& name);
        symbol* lookup_builtin(const std::string& name);

        scope_type get_type() const { return m_type; }
        ref<scope> get_parent() { return m_parent; }
        symbol_table_t& get_symbols() { return m_symbols; }

        void set_symbols(const symbol_table_t& table) { m_symbols = table; }

    private:
        symbol_table_t m_symbols;
        ref<scope> m_parent;
        scope_type m_type;
    };

    inline ref<scope> builtin_scope = make_ref<scope>(scope_type::builtin);

} // namespace wio