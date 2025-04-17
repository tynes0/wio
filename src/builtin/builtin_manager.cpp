#include "builtin_manager.h"

#include "builtin_base.h"

#include "io.h"
#include "math.h"
#include "utility.h"
#include "members/b_array.h"
#include "members/b_string.h"
#include "members/b_dictionary.h"
#include "members/b_vec2.h"
#include "members/b_character.h"

namespace wio
{
    void builtin_manager::load(std::map<variable_type, ref<symbol_table>>& target_member_scope_map)
    {
        builtin::b_array array_member_loader;
        target_member_scope_map[variable_type::vt_array] = array_member_loader.load();
        
        builtin::b_string string_member_loader;
        target_member_scope_map[variable_type::vt_string] = string_member_loader.load();
        
        builtin::b_dictionary dictionary_member_loader;
        target_member_scope_map[variable_type::vt_dictionary] = dictionary_member_loader.load();
        
        builtin::b_character character_member_loader;
        target_member_scope_map[variable_type::vt_character] = character_member_loader.load();
        target_member_scope_map[variable_type::vt_character_ref] = target_member_scope_map[variable_type::vt_character];
        
        builtin::b_vec2 vec2_member_loader;
        target_member_scope_map[variable_type::vt_vec2] = vec2_member_loader.load();
        
        target_member_scope_map[variable_type::vt_file] = nullptr;
        target_member_scope_map[variable_type::vt_pair] = nullptr;
        target_member_scope_map[variable_type::vt_vec3] = nullptr;
        target_member_scope_map[variable_type::vt_vec4] = nullptr;
        target_member_scope_map[variable_type::vt_null] = nullptr;
        target_member_scope_map[variable_type::vt_integer] = nullptr;
        target_member_scope_map[variable_type::vt_float] = nullptr;
        target_member_scope_map[variable_type::vt_bool] = nullptr;
        target_member_scope_map[variable_type::vt_function] = nullptr;
        target_member_scope_map[variable_type::vt_any] = nullptr;
        target_member_scope_map[variable_type::vt_type] = nullptr;
        target_member_scope_map[variable_type::vt_any] = nullptr;
    }
}