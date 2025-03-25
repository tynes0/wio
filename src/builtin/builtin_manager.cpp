#include "builtin_manager.h"

#include "io.h"
#include "math.h"
#include "utility.h"
#include "b_array.h"

namespace wio
{
    static packed_bool s_member_load_flags_1{}; // 1->array 2->string 3->dict 4->file
    static packed_bool s_lib_load_flags_1{}; // 1->io 2->math 3->util 4->range

    void builtin_manager::load(ref<scope> target_scope, std::map<variable_type, ref<scope>>& target_member_scope_map)
    {
        if (!s_lib_load_flags_1.b1)
        {
            builtin::io io_loader;
            io_loader.load(target_scope);
        }
        if (!s_lib_load_flags_1.b2)
        {
            builtin::math math_loader;
            math_loader.load(target_scope);
        }
        if (!s_lib_load_flags_1.b3)
        {
            builtin::utility utility_loader;
            utility_loader.load(target_scope);
        }

        if (!s_member_load_flags_1.b1)
        {
            builtin::b_array array_member_loader;
            target_member_scope_map[variable_type::vt_array] = array_member_loader.load();
        }

        if (!s_member_load_flags_1.b2)
        {
            target_member_scope_map[variable_type::vt_string] = nullptr;
        }

        if (!s_member_load_flags_1.b3)
        {
            target_member_scope_map[variable_type::vt_dictionary] = nullptr;
        }

        if (!s_member_load_flags_1.b4)
        {
            target_member_scope_map[variable_type::vt_file] = nullptr;
        }

        target_member_scope_map[variable_type::vt_null] = nullptr;
        target_member_scope_map[variable_type::vt_integer] = nullptr;
        target_member_scope_map[variable_type::vt_float] = nullptr;
        target_member_scope_map[variable_type::vt_character] = nullptr;
        target_member_scope_map[variable_type::vt_bool] = nullptr;
        target_member_scope_map[variable_type::vt_function] = nullptr;
        target_member_scope_map[variable_type::vt_var_param] = nullptr;
        target_member_scope_map[variable_type::vt_any] = nullptr;
    }
}