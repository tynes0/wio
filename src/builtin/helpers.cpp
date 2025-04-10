#include "helpers.h"

#include "builtin_base.h"

#include "../variables/function.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../types/vec2.h"
#include "../types/file_wrapper.h"

#include "../utils/util.h"

namespace wio
{
    namespace builtin
    {
        namespace helper
        {            
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
                ref<variable> x_value = std::dynamic_pointer_cast<variable>(xvb);
                ref<variable> y_value = std::dynamic_pointer_cast<variable>(yvb);

                vec2 result_vec{ 0, 0 };

                if (x_value->get_type() == variable_type::vt_float)
                    result_vec.x = x_value->get_data_as<double>();
                else if (x_value->get_type() == variable_type::vt_float_ref)
                    result_vec.x = *x_value->get_data_as<double*>();
                else if (x_value->get_type() == variable_type::vt_integer)
                    result_vec.x = (double)x_value->get_data_as<long long>();

                if (y_value->get_type() == variable_type::vt_float)
                    result_vec.y = y_value->get_data_as<double>();
                else if (y_value->get_type() == variable_type::vt_float_ref)
                    result_vec.y = *y_value->get_data_as<double*>();
                else if (y_value->get_type() == variable_type::vt_integer)
                    result_vec.y = (double)y_value->get_data_as<long long>();
                
                ref<variable> result = make_ref<variable>(result_vec, variable_type::vt_vec2);
                result->init_members();
                ref<scope> members = result->get_members();
                
                symbol x_sym(make_ref<variable>(&result->get_data_as<vec2>().x, variable_type::vt_float_ref));
                symbol y_sym(make_ref<variable>(&result->get_data_as<vec2>().y, variable_type::vt_float_ref));

                members->insert("X", x_sym);
                members->insert("Y", y_sym);
                members->insert("R", x_sym);
                members->insert("G", y_sym);

                return result;
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

