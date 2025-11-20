#include "builtin_manager.h"

#include "builtin_base.h"

#include "members/b_array.h"
#include "members/b_string.h"
#include "members/b_dictionary.h"
#include "members/b_vec2.h"
#include "members/b_vec3.h"
#include "members/b_character.h"
#include "members/b_float.h"
#include "members/b_enum.h"

namespace wio
{
    void builtin_manager::load(std::map<variable_type, ref<symbol_table>>& target_member_scope_map)
    {
        target_member_scope_map[variable_type::vt_array] = builtin::b_array::load();
        target_member_scope_map[variable_type::vt_string] = builtin::b_string::load();
        target_member_scope_map[variable_type::vt_dictionary] = builtin::b_dictionary::load();
        target_member_scope_map[variable_type::vt_character] = builtin::b_character::load();
        target_member_scope_map[variable_type::vt_character_ref] = target_member_scope_map[variable_type::vt_character];
        target_member_scope_map[variable_type::vt_vec2] = builtin::b_vec2::load();
        target_member_scope_map[variable_type::vt_vec3] = builtin::b_vec3::load();
        target_member_scope_map[variable_type::vt_float] = builtin::b_float::load();
        target_member_scope_map[variable_type::vt_float_ref] = target_member_scope_map[variable_type::vt_float];
        target_member_scope_map[variable_type::vt_enumeration] = builtin::b_enum::load();
        target_member_scope_map[variable_type::vt_file] = nullptr;
        target_member_scope_map[variable_type::vt_pair] = nullptr;
        target_member_scope_map[variable_type::vt_vec4] = nullptr;
        target_member_scope_map[variable_type::vt_null] = nullptr;
        target_member_scope_map[variable_type::vt_integer] = nullptr;
        target_member_scope_map[variable_type::vt_bool] = nullptr;
        target_member_scope_map[variable_type::vt_function] = nullptr;
        target_member_scope_map[variable_type::vt_any] = nullptr;
        target_member_scope_map[variable_type::vt_type] = nullptr;
        target_member_scope_map[variable_type::vt_any] = nullptr;
    }
}