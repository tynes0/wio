#include "builtin.h"

#include "variables/function.h"
#include "variables/array.h"
#include "variables/dictionary.h"
#include <iostream>

namespace wio
{
    namespace detail
    {
        static void builtin_print(ref<variable_base> base)
        {
            static const auto write = [](const std::string& str) 
                {
                    if(!str.empty())
                        fwrite(&str[0], 1, str.size(), stdout);
                };

            std::string result;
            if (base->get_base_type() == variable_base_type::variable)
            {
                ref<variable> var = std::dynamic_pointer_cast<variable>(base);
                if (var->get_type() == variable_type::vt_string)
                    result = var->get_data_as<std::string>();
                else if (var->get_type() == variable_type::vt_integer)
                    result = std::to_string(var->get_data_as<long long>());
                else if (var->get_type() == variable_type::vt_float)
                    result = std::to_string(var->get_data_as<double>());
                else if (var->get_type() == variable_type::vt_bool)
                    result = var->get_data_as<bool>() ? "true" : "false";
                else if (var->get_type() == variable_type::vt_character)
                    result = std::string(1, var->get_data_as<char>());
                else if (var->get_type() == variable_type::vt_null)
                    result = "null";
                else
                    throw builtin_error("Invalid expression in print parameter!");
            }
            else if (base->get_base_type() == variable_base_type::array)
            {
                ref<var_array> arr = std::dynamic_pointer_cast<var_array>(base);
                write("[ ");
                for (size_t i = 0; i < arr->size(); ++i)
                {
                    builtin_print(arr->get_element(i));
                    if (i != arr->size() - 1)
                        write(", ");
                }
                write(" ]");

            }
            else if (base->get_base_type() == variable_base_type::dictionary)
            {
                ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(base);
                write("{ ");
                const auto& dict_map = dict->get_data();

                size_t i = 0;
                for (const auto& [key, value] : dict_map)
                {
                    write("[ ");
                    write(key);
                    write(", ");
                    builtin_print(value);
                    write(" ]");
                    if (i != dict->size() - 1)
                        write(", ");
                    i++;
                }

                write(" }");
            }

            if (!result.empty())
                fwrite(&result[0], 1, result.size(), stdout);
        }
    }

    ref<scope> builtin::load()
    {
        ref<scope> result_scope = make_ref<scope>(scope_type::builtin);
        std::vector<function_param> params;
        params.emplace_back("", variable_type::vt_any, false);
        var_function func([](const std::vector<function_param>&, std::vector<ref<variable_base>>& parameters)
            { 
                detail::builtin_print(parameters.front()); 
                return make_ref<variable>(any(), variable_type::vt_null); 
            }, variable_type::vt_null, params, false);
        symbol sym("print", variable_type::vt_function, scope_type::builtin, make_ref<var_function>(func), {false, true});
        result_scope->insert("print", sym);
        return result_scope;
    }
}