#include "b_string.h"
#include "builtin_base.h"

#include "../variables/variable.h"
#include "../variables/array.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<variable> get_var_str(ref<variable_base> base_str)
            {
                if (!base_str)
                    throw exception("String is null!");
                ref<variable> str = std::dynamic_pointer_cast<variable>(base_str);
                if (!str)
                    throw exception("String is null!");

                return str;
            }

            static ref<variable_base> b_string_int(ref<variable_base> base_str)
            {
                ref<variable> str = get_var_str(base_str);
                long long res = 0;
                try
                {
                    res = std::stoll(str->get_data_as<std::string>());
                }
                catch (const std::exception&) {}

                return make_ref<variable>(res, variable_type::vt_integer);
            }

            static ref<variable_base> b_string_float(ref<variable_base> base_str)
            {
                ref<variable> str = get_var_str(base_str);
                double res = 0;
                try
                {
                    res = std::stod(str->get_data_as<std::string>());
                }
                catch (const std::exception&) {}

                return make_ref<variable>(res, variable_type::vt_float);
            }

            static ref<variable_base> b_string_array(ref<variable_base> base_str)
            {
                ref<variable> str = get_var_str(base_str);
                
                std::vector<ref<variable_base>> data;
                std::string s = str->get_data_as<std::string>();
                for (char ch : s)
                    data.push_back(make_ref<variable>(ch, variable_type::vt_character));

                return make_ref<var_array>(data);
            }

            static ref<variable_base> b_string_length(ref<variable_base> base_str)
            {
                ref<variable> str = get_var_str(base_str);

                return make_ref<variable>((long long)str->get_data_as<std::string>().length(), variable_type::vt_integer);
            }
        }

        ref<scope> b_string::load()
        {
            auto result = make_ref<scope>(scope_type::builtin);

            loader::load_function<1>(result, "Int", detail::b_string_int, variable_type::vt_integer, { variable_type::vt_string });
            loader::load_function<1>(result, "Float", detail::b_string_float, variable_type::vt_float, { variable_type::vt_string });
            loader::load_function<1>(result, "Array", detail::b_string_array, variable_type::vt_array, { variable_type::vt_string });
            loader::load_function<1>(result, "Length", detail::b_string_length, variable_type::vt_integer, { variable_type::vt_string });

            return result;
        }
    }

}

