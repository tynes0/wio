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
        bool is_pf_return_ref() const { return m_flags.b5; }

        void set_const(bool flag) { m_flags.b1 = flag; }
        void set_omni(bool flag) { m_flags.b2 = flag; }
        // pf -> param flag
        void set_pf_return_ref(bool flag) { m_flags.b5 = flag; }

        ref<symbol_table> get_members() const { return m_members; }
        void init_members() { if (m_members) return; m_members = make_ref<symbol_table>(); }
        void load_members(ref<symbol_table> members) { m_members = members; }
        void load_members(ref<scope> members) { init_members();  m_members->get_symbols() = members->get_symbols(); }
        void load_members(const symbol_map& table) { if (!m_members) init_members(); m_members->set_symbols(table); }
    protected:
        variable_base(packed_bool flags) : m_flags(flags) { }
        ref<symbol_table> m_members;
        packed_bool m_flags = {}; // b1 -> const --- b2 -> omni type --- b5-> return ref 
    };

    using pair_t = pair<ref<variable_base>, ref<variable_base>>;
} // namespace wio
