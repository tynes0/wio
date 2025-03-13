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
        vt_var_param,
        vt_any
    };

    MakeFrenumInNamespace(wio, variable_type, vt_null, vt_integer, vt_float, vt_string, vt_character, vt_bool, vt_array, vt_dictionary, vt_function, vt_var_param)

    class variable_base
    {
    public:
        virtual ~variable_base() = default;

        virtual variable_base_type get_base_type() const = 0;
        virtual variable_type get_type() const = 0;
        virtual ref<variable_base> clone() const = 0;

        bool is_constant() const { return m_flags.b1; }
        bool is_ref() const { return m_flags.b2; }

        void set_const(bool flag) { m_flags.b1 = flag; }
        void set_ref(bool flag) { m_flags.b2 = flag; }

        void set_flags(packed_bool flags) { m_flags = flags; }
    protected:
        variable_base(packed_bool flags) : m_flags(flags) { }
    private:
        packed_bool m_flags = {}; // b1 -> const --- b2 ref
    };

} // namespace wio
