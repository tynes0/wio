// scope.h
#pragma once

#include <map>
#include <string>
#include <memory>
#include "base.h"
#include "variables/variable.h"

namespace wio
{
    enum class scope_type 
    {
        builtin,
        global,
        local,
        function,
        block
    };

    struct symbol 
    {
        std::string name;
        variable_type type = variable_type::vt_null;
        scope_type scope_t = scope_type::local;
        ref<variable_base> var_ref;
        packed_bool flags = {}; // 1-> is_local ---  2-> is_global

        symbol() {}

        symbol(const std::string& name, variable_type type, scope_type scope, ref<variable_base> var_ref = nullptr, packed_bool flags = {})
            : name(name), type(type), scope_t(scope), var_ref(var_ref), flags(flags) {}
    };

    class scope 
    {
    public:
        scope(scope_type type, ref<scope> parent = nullptr) : m_type(type), m_parent(parent) {}

        void insert(const std::string& name, const symbol& symbol);
        void insert_to_global(const std::string& name, const symbol& symbol);
        symbol* lookup(const std::string& name);
        symbol* lookup_current_and_global(const std::string& name);
        scope_type get_type() { return m_type; }
        ref<scope> get_parent() { return m_parent; }
        std::map<std::string, symbol>& get_symbols() { return m_symbols; }
    private:
        scope_type m_type;
        std::map<std::string, symbol> m_symbols;
        ref<scope> m_parent;
    };

} // namespace wio