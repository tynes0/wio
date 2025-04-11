#include "helpers.h"

#include "builtin_base.h"

#include "../variables/function.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../types/file_wrapper.h"

#include "../utils/util.h"

namespace wio
{
    namespace builtin
    {
        namespace helper
        {
            static double as_double(ref<variable_base> var)
            {
                ref<variable> value = std::dynamic_pointer_cast<variable>(var);

                if (value->get_type() == variable_type::vt_float)
                    return value->get_data_as<double>();
                else if (value->get_type() == variable_type::vt_float_ref)
                    return *value->get_data_as<double*>();
                else if (value->get_type() == variable_type::vt_integer)
                    return (double)value->get_data_as<long long>();

                return 0.0;
            }

            ref<variable_base> create_pair(ref<variable_base> first, ref<variable_base> second)
            {
                pair_t p(first, second);
                ref<variable> result = make_ref<variable>(p, variable_type::vt_pair);
                result->init_members();
                ref<scope> members = result->get_members();

                symbol f_sym(first);
                symbol s_sym(second);

                members->insert("First", f_sym);
                members->insert("Second", s_sym);
                members->insert("Key", f_sym);
                members->insert("Value", s_sym);

                return result;
            }

            ref<variable_base> create_vec2(ref<variable_base> xvb, ref<variable_base> yvb)
            {
                vec2 result_vec;

                result_vec.x = as_double(xvb);
                result_vec.y = as_double(yvb);

                ref<variable> result = make_ref<variable>(result_vec, variable_type::vt_vec2);
                result->init_members();
                ref<scope> members = result->get_members();
                
                vec2& vec_data = result->get_data_as<vec2>();

                symbol x_sym(make_ref<variable>(&vec_data.x, variable_type::vt_float_ref));
                symbol y_sym(make_ref<variable>(&vec_data.y, variable_type::vt_float_ref));

                members->insert("X", x_sym);
                members->insert("Y", y_sym);
                members->insert("R", x_sym);
                members->insert("G", y_sym);

                return result;
            }

            ref<variable_base> create_vec3(ref<variable_base> xvb, ref<variable_base> yvb, ref<variable_base> zvb)
            {
                vec3 result_vec;

                result_vec.x = as_double(xvb);
                result_vec.y = as_double(yvb);
                result_vec.z = as_double(zvb);

                ref<variable> result = make_ref<variable>(result_vec, variable_type::vt_vec3);
                result->init_members();
                ref<scope> members = result->get_members();

                vec3& vec_data = result->get_data_as<vec3>();

                symbol x_sym(make_ref<variable>(&vec_data.x, variable_type::vt_float_ref));
                symbol y_sym(make_ref<variable>(&vec_data.y, variable_type::vt_float_ref));
                symbol z_sym(make_ref<variable>(&vec_data.z, variable_type::vt_float_ref));

                members->insert("X", x_sym);
                members->insert("Y", y_sym);
                members->insert("Z", z_sym);
                members->insert("R", x_sym);
                members->insert("G", y_sym);
                members->insert("B", z_sym);

                return result;
            }

            ref<variable_base> create_vec4(ref<variable_base> xvb, ref<variable_base> yvb, ref<variable_base> zvb, ref<variable_base> wvb)
            {
                return ref<variable_base>();
            }

            ref<variable_base> string_as_array(ref<variable_base> base_str)
            {
                if (!base_str)
                    throw exception("String is null!");
                ref<variable> str = std::dynamic_pointer_cast<variable>(base_str);
                if (!str)
                    throw exception("String is null!");

                std::vector<ref<variable_base>> data;
                std::string s = str->get_data_as<std::string>();
                for (char ch : s)
                    data.push_back(make_ref<variable>(ch, variable_type::vt_character));

                return make_ref<var_array>(data);
            }
        }
    }
}

