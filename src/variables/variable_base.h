#pragma once

#include <string>
#include "variable_type.h"

#include "../utils/frenum.h"
#include "../utils/any.h"
#include "../types/pair.h"
#include "../base/base.h"

#include "../interpreter/symbol.h"
#include "../interpreter/scope.h"

namespace wio 
{
    class variable_base
    {
    public:
        virtual ~variable_base() = default;

        virtual variable_base_type get_base_type() const = 0;
        virtual variable_type get_type() const = 0;
        virtual ref<variable_base> clone() const = 0;

        bool is_constant() const { return m_flags.b1; }
        bool is_omni() const { return m_flags.b2; }
        // pf -> param flag
        bool is_pf_return_ref() const { return m_flags.b3; }

        void set_const(bool flag) { m_flags.b1 = flag; }
        void set_omni(bool flag) { m_flags.b2 = flag; }
        // pf -> param flag
        void set_pf_return_ref(bool flag) { m_flags.b3 = flag; }

        ref<symbol_table> get_members() const { return m_members; }
        void init_members() { if (m_members) return; m_members = make_ref<symbol_table>(); }
        void load_members(ref<symbol_table> members) { m_members = members; }
        void load_members(ref<scope> members) { init_members();  m_members->get_symbols() = members->get_symbols(); }
        void load_members(const symbol_map& table) { if (!m_members) init_members(); m_members->set_symbols(table); }

        bool is_outer() const { return m_unit_flags.b1; }
        bool is_hidden() const { return m_unit_flags.b2; }
        bool is_exposed() const { return m_unit_flags.b3; }
        bool is_shared() const { return m_unit_flags.b4; }

        void set_outer(bool flag) { m_unit_flags.b1 = flag; }
        void set_hidden(bool flag) { m_unit_flags.b2 = flag; }
        void set_exposed(bool flag) { m_unit_flags.b3 = flag; }
        void set_shared(bool flag) { m_unit_flags.b4 = flag; }
    protected:
        variable_base(packed_bool flags) : m_flags(flags) { }
        ref<symbol_table> m_members;
        packed_bool m_flags = {}; // b1 -> const --- b2 -> omni type --- b3-> return ref 
        packed_bool m_unit_flags = {}; // (if this variable is a unit member) 1- outer 2- hidden 3- exposed 4- shared
    };

    using pair_t = pair<ref<variable_base>, ref<variable_base>>;
} // namespace wio
