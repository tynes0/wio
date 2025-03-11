#pragma once

#include <string>
#include "../utils/frenum.h"
#include "../utils/any.h"
#include "../base.h"

namespace wio 
{

    enum class variable_base_type 
    {
        variable,
        array,
        dictionary,
        function
    };

    MakeFrenumInNamespace(wio, variable_base_type, variable, array, dictionary, function)

    enum class variable_type
    {
        vt_null,
        vt_integer,
        vt_float,
        vt_string,
        vt_character,
        vt_bool,
        vt_array,
        vt_dictionary,
        vt_function,
        vt_any
    };

    MakeFrenumInNamespace(wio, variable_type, vt_null, vt_integer, vt_float, vt_string, vt_character, vt_bool, vt_array, vt_dictionary, vt_function, vt_any)

    class variable_base
    {
    public:
        virtual ~variable_base() = default;

        virtual variable_base_type get_base_type() const = 0;
        virtual variable_type get_type() const = 0;
        virtual ref<variable_base> clone() const = 0;

        bool is_constant() const { return m_flags.b1; }
        bool is_local() const { return m_flags.b2; }
        bool is_global() const { return m_flags.b3; }

        void set_const(bool flag) { m_flags.b1 = flag; }
        void set_local(bool flag) { m_flags.b2 = flag; }
        void set_global(bool flag) { m_flags.b3 = flag; }
    protected:
        void init_flags(bool cf, bool lf, bool gf) { set_const(cf); set_local(lf); set_global(gf); }
    private:
        packed_bool m_flags = {}; // b1 -> const --- b2 -> local --- b3 -> global
    };

} // namespace wio
