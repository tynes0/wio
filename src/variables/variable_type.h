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
    };

    MakeFrenumInNamespace(wio, variable_type, vt_null, vt_integer, vt_float, vt_string, vt_character, vt_bool, vt_array, vt_dictionary, vt_function, vt_var_param, vt_var_param, vt_file, vt_type, vt_pair, vt_any)

}