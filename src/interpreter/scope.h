#pragma once

#include <map>
#include <string>
#include <memory>
#include <vector>
#include "base.h"
#include "ast.h"
#include "../variables/variable.h"
#include "../variables/function.h"

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

    struct symbol 
    {
        friend class scope;

        std::string name;
        variable_type type = variable_type::vt_null;
        scope_type scope_t = scope_type::local;
        ref<variable_base> var_ref;
        ref<scope> members;
        packed_bool flags = {}; // 1-> is_local --- 2-> is_global --- 3-> original position passed

        symbol() {}

        symbol(const std::string& name, variable_type type, scope_type scope, ref<variable_base> var_ref = nullptr, packed_bool flags = {})
            : name(name), type(type), scope_t(scope), var_ref(var_ref), flags(flags) {}
    };

    using symbol_table_t = std::map<std::string, symbol>;
    using definition_table_t = std::map<std::string, var_func_definition>;

    class scope 
    {
    public:
        scope(scope_type type, ref<scope> parent = nullptr) : m_type(type), m_parent(parent) {}

        void insert(const std::string& name, const symbol& symbol);
        void insert_to_global(const std::string& name, const symbol& symbol);
        void insert_def(const std::string& name, const var_func_definition& def);
        void insert_def_to_global(const std::string& name, const var_func_definition& def);

        symbol* lookup(const std::string& name);
        symbol* lookup_only_global(const std::string& name);
        symbol* lookup_current_and_global(const std::string& name);
        var_func_definition* lookup_def(const std::string& name);

        scope_type get_type() const { return m_type; }
        ref<scope> get_parent() { return m_parent; }
        symbol_table_t& get_symbols() { return m_symbols; }
        definition_table_t& get_definitions() { return m_definitions; }
    private:
        symbol_table_t m_symbols;
        definition_table_t m_definitions;
        ref<scope> m_parent;
        scope_type m_type;
    };

    struct statement_stack
    {
        std::vector<ref<statement>>* list = nullptr;
        ref<statement_stack> parent;
    };

} // namespace wio