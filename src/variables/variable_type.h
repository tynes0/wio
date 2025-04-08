#pragma once

#include "../utils/frenum.h"

namespace wio
{

    enum class variable_base_type
    {
        variable,
        array,
        dictionary,
        function,
        realm // 
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
        vt_file,
        vt_type,
        vt_pair,
        vt_vec2,
        vt_vec3,
        vt_vec4,
        vt_realm, // 
        vt_any,

        vt_character_ref, // for optimized access
        vt_float_ref      // for optimized access
    };

    MakeFrenumInNamespace(wio, variable_type, vt_null, vt_integer, vt_float, vt_string, vt_character, vt_bool, vt_array, vt_dictionary, vt_function, vt_var_param, vt_var_param, vt_file, vt_type, vt_pair, vt_any)

    namespace helper
    {
        inline bool is_var_t(variable_type vt)
        {
            return (vt != variable_type::vt_array && vt != variable_type::vt_dictionary && vt != variable_type::vt_function && vt != variable_type::vt_realm);
        }
    }
}