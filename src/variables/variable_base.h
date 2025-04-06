#pragma once

#include <string>
#include "variable_type.h"

#include "../utils/frenum.h"
#include "../utils/any.h"
#include "../types/pair.h"
#include "../utils/uuid.h"
#include "../base/base.h"

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
        bool is_ref() const { return m_flags.b2; }
        // pf -> param flag
        bool is_pf_return_ref() const { return m_flags.b5; }

        void set_const(bool flag) { m_flags.b1 = flag; }
        void set_ref(bool flag) { m_flags.b2 = flag; }
        // pf -> param flag
        void set_pf_return_ref(bool flag) { m_flags.b5 = flag; }

        void set_flags(packed_bool flags) { m_flags = flags; }
        ref<scope> get_members() const { return m_members; }
        void init_members() { if (m_members) return; m_members = make_ref<scope>(scope_type::builtin); }
        void load_members(ref<scope> members) { m_members = members; }
        void load_members(const symbol_table_t& table) { if (!m_members) init_members(); m_members->set_symbols(table); }
    protected:
        variable_base(packed_bool flags) : m_flags(flags) { }
    private:
        ref<scope> m_members;
        packed_bool m_flags = {}; // b1 -> const --- b2 -> ref --- b3-> * --- b4-> * --- b5-> return ref --- b6-> * --- b7-> * --- b8-> *  (b1-b2-b3-b4 default flags  ---  b5-b6-b7-b8 func parameter flags)
    };

    using pair_t = pair<ref<variable_base>, ref<variable_base>>;

    class null_var : public variable_base
    {
    public:
        null_var(variable_base_type vt = variable_base_type::variable) :variable_base({}), m_vbt(vt) {}
        null_var(packed_bool flags) : variable_base(flags) {}
        variable_base_type get_base_type() const override { return m_vbt; }
        variable_type get_type() const override { return variable_type::vt_null; }
        ref<variable_base> clone() const override { return make_ref<null_var>(*this); }
    private:
        variable_base_type m_vbt = variable_base_type::variable;
    };

} // namespace wio
