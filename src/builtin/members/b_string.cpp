#include "b_string.h"

#include "../builtin_base.h"
#include "../helpers.h"
#include "../../variables/variable.h"
#include "../../variables/array.h"

#include <algorithm>

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static constexpr integer_t s_end_position = (integer_t)(std::numeric_limits<integer_t>().max)();

            static ref<variable> get_var_str(ref<variable_base> base_str)
            {
                if (!base_str)
                    throw exception("String is null!");
                ref<variable> str = std::dynamic_pointer_cast<variable>(base_str);
                if (!str)
                    throw type_mismatch_error("Mismatch parameter type!");

                return str;
            }

            static ref<variable_base> b_string_front(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                string_t& data = str->get_data_as<string_t>();

                if (data.size() == 0)
                    throw builtin_error("Front() called on empty string!");

                if (str->is_pf_return_ref())
                    return make_ref<variable>(any(&data[0]), variable_type::vt_character_ref);

                return make_ref<variable>(any(data[0]), variable_type::vt_character);
            }

            static ref<variable_base> b_string_back(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                string_t& data = str->get_data_as<string_t>();

                if (data.size() == 0)
                    throw builtin_error("Back() called on empty string!");

                if (str->is_pf_return_ref())
                    return make_ref<variable>(any(&data[data.size() - 1]), variable_type::vt_character_ref);

                return make_ref<variable>(any(data[data.size() - 1]), variable_type::vt_character);
            }

            static ref<variable_base> b_string_at(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);
                integer_t index = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>();
                
                string_t& data = str->get_data_as<string_t>();

                if ((integer_t)data.size() <= index)
                    throw out_of_bounds_error("string index out of the bounds at At()!");

                if (str->is_pf_return_ref())
                    return make_ref<variable>(any(&data[index]), variable_type::vt_character_ref);

                return make_ref<variable>(any(data[index]), variable_type::vt_character);
            }

            static ref<variable_base> b_string_compare(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);
                ref<variable> right_var = get_var_str(args[1]);

                string_t left = str->get_data_as<string_t>();
                string_t right = right_var->get_data_as<string_t>();

                if (left.size() != right.size())
                    return make_ref<variable>(any(false), variable_type::vt_bool);

                for (size_t i = 0; i < left.size(); ++i)
                    if (tolower(left[i]) != tolower(right[i]))
                        return make_ref<variable>(any(false), variable_type::vt_bool);

                return make_ref<variable>(any(true), variable_type::vt_bool);
            }

            static ref<variable_base> b_string_int(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);
                integer_t res = 0;
                try
                {
                    res = std::stoll(str->get_data_as<string_t>());
                }
                catch (const std::exception&) {}

                return make_ref<variable>(res, variable_type::vt_integer);
            }

            static ref<variable_base> b_string_float(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);
                float_t res = 0;
                try
                {
                    res = std::stod(str->get_data_as<string_t>());
                }
                catch (const std::exception&) {}

                return make_ref<variable>(res, variable_type::vt_float);
            }

            static ref<variable_base> b_string_array(const std::vector<ref<variable_base>>& args)
            {
                return helper::string_as_array(args[0]);
            }

            static ref<variable_base> b_string_length(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                return make_ref<variable>((integer_t)str->get_data_as<string_t>().length(), variable_type::vt_integer);
            }

            static ref<variable_base> b_string_empty(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                return make_ref<variable>(str->get_data_as<string_t>().empty(), variable_type::vt_bool);
            }

            static ref<variable_base> b_string_clear(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                str->get_data_as<string_t>().clear();

                return create_null_variable();
            }

            static ref<variable_base> b_string_push(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                str->get_data_as<string_t>().push_back(helper::var_as_char(args[1]));
                return create_null_variable();
            }

            static ref<variable_base> b_string_pop(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);

                string_t& data = str->get_data_as<string_t>();

                if (data.size() == 0)
                    throw builtin_error("Pop() called on empty string!");

                character_t ch = data.back();
                data.pop_back();

                return make_ref<variable>(any(ch), variable_type::vt_character);
            }

            static ref<variable_base> b_string_split(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);
                string_t data = str->get_data_as<string_t>();

                std::vector<ref<variable_base>> result;
                size_t pos = 0;
                size_t last_location = 0;

                while (true)
                {
                    pos = data.find(' ', last_location);
                    if (pos == string_t::npos)
                        pos = data.size();

                    string_t temp = data.substr(last_location, pos - last_location);
                    if(!temp.empty())
                        result.push_back(make_ref<variable>(temp, variable_type::vt_string));

                    last_location = pos + 1;

                    if (pos == data.size())
                        break;
                }
                return make_ref<var_array>(result);
            }

            static ref<variable_base> b_string_split_2(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> str = get_var_str(args[0]);
                string_t data = str->get_data_as<string_t>();
                string_t token;

                if (args[1]->get_type() == variable_type::vt_string)
                    token = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<string_t>();
                else if (args[1]->get_type() == variable_type::vt_character)
                    token = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<character_t>();
                else if (args[1]->get_type() == variable_type::vt_character_ref)
                    token = *std::dynamic_pointer_cast<variable>(args[1])->get_data_as<character_t*>();
                else 
                    token = data;

                std::vector<ref<variable_base>> result;
                size_t pos = 0;
                size_t last_location = 0;

                while (true)
                {
                    pos = data.find(token, last_location);
                    if (pos == string_t::npos)
                        pos = data.size();

                    string_t temp = data.substr(last_location, pos - last_location);
                    if (!temp.empty())
                        result.push_back(make_ref<variable>(temp, variable_type::vt_string));

                    last_location = pos + token.size();

                    if (pos == data.size())
                        break;
                }
                return make_ref<var_array>(result);
            }
        }

        ref<symbol_table> b_string::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "Front", detail::b_string_front, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Back", detail::b_string_back, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "At", detail::b_string_at, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Compare", detail::b_string_compare, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Int", detail::b_string_int, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Float", detail::b_string_float, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Array", detail::b_string_array, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Length", detail::b_string_length, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Size", detail::b_string_length, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Empty", detail::b_string_empty, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Clear", detail::b_string_clear, pa<1>{ variable_base_type::variable });
            loader::load_function(table, "Push", detail::b_string_push, pa<2>{ variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "Pop", detail::b_string_pop, pa<1>{ variable_base_type::variable });

            auto split_func = loader::load_function(table, "Split", detail::b_string_split, pa<1>{ variable_base_type::variable });
            loader::load_overload(split_func, detail::b_string_split_2, pa<2>{ variable_base_type::variable, variable_base_type::variable });

            loader::load_constant(table, "End", variable_type::vt_integer, s_end_position);

            return result;
        }
    }

}