#pragma once

#include "../utils/frenum.h"
#include "../base/base.h"

#include <cstdint>

namespace wio
{

    enum class variable_base_type 
    {
        none, 
        variable,
        array,
        dictionary,
        function,
        enumeration,
        realm,
        unit,
        unit_instance,
        omni
    };

    MakeFrenumInNamespace(wio, variable_base_type, none,
        variable,
        array,
        dictionary,
        function,
        enumeration,
        realm,
        unit,
        unit_instance,
        omni)

    enum class variable_type : integer_t
    {
        vt_null,
        vt_integer,
        vt_float,
        vt_string,
        vt_character,
        vt_bool,
        vt_type,
        vt_pair,
        vt_file,

        vt_array,
        vt_dictionary,
        vt_function,
        vt_enumeration,
        vt_realm, 
        
        vt_vec2,
        vt_vec3,
        vt_vec4,
        
        vt_comparator,
        
        vt_unit,
        vt_unit_instance,
        vt_any,
        
        vt_character_ref,
        vt_float_ref
    };

    MakeFrenumInNamespace(wio, variable_type, 
        vt_null, vt_integer, vt_float, vt_string, vt_character, vt_bool, vt_type, vt_pair, vt_file, 
        vt_array, vt_dictionary, vt_function, vt_enumeration, vt_realm, 
        vt_vec2, vt_vec3, vt_vec4, 
        vt_comparator, 
        vt_unit, vt_unit_instance, vt_any,
        vt_character_ref, vt_float_ref)
}