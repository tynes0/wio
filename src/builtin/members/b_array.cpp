#include "b_array.h"

#include "../builtin_base.h"
#include "../../variables/array.h"
#include "../../interpreter/eval_base.h"

#include <array>

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<var_array> get_var_array(ref<variable_base> base_array)
            {
                if (!base_array)
                    throw exception("Array is null!");

                ref<var_array> array = std::dynamic_pointer_cast<var_array>(base_array);

                if (!array)
                    throw type_mismatch_error("Mismatch parameter type!");
                return array;
            }

            static ref<variable_base> b_array_length(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                return make_ref<variable>((integer_t)array->size(), variable_type::vt_integer);
            }
            
            static ref<variable_base> b_array_front(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);

                if (array->size() == 0)
                    throw builtin_error("Front() called on empty array!");

                if (array->is_pf_return_ref())
                {
                    auto& result = array->get_element(0);
                    eval_base::get().set_last_left_value(&result);
                    return result;
                }
                return array->get_element(0)->clone();
            }

            static ref<variable_base> b_array_back(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);

                if (array->size() == 0)
                    throw builtin_error("Back() called on empty array!");

                if (array->is_pf_return_ref())
                {
                    auto& result = array->get_element(array->size() - 1);
                    eval_base::get().set_last_left_value(&result);
                    return result;
                }
                return array->get_element(array->size() - 1)->clone();
            }

            static ref<variable_base> b_array_at(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                if (array->is_pf_return_ref())
                {
                    auto& result = array->get_element(std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>());
                    eval_base::get().set_last_left_value(&result);
                    return result;
                }
                return array->get_element(std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>())->clone();
            }

            static  ref<variable_base> b_array_insert(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                integer_t ll_idx = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>();
                array->insert(ll_idx, args[2]);
                return create_null_variable();
            }

            static ref<variable_base> b_erase(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                auto& data = array->get_data();
                integer_t ll_idx = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>();
                ref<variable_base> result = array->get_element(ll_idx);
                array->erase(ll_idx);
                return result;
            }

            static  ref<variable_base> b_array_push_back(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                array->push(args[1]);
                return create_null_variable();
            }

            static ref<variable_base> b_array_pop_back(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                return array->pop();
            }

            static ref<variable_base> b_array_extend(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                ref<var_array> rarray = get_var_array(args[1]);

                for (auto& item : rarray->get_data())
                    array->push(item);

                return create_null_variable();
            }

            static ref<variable_base> b_array_clear(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                array->clear();
                return create_null_variable();
            }

            static ref<variable_base> b_array_resize(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                integer_t new_size = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>();

                if (new_size <= 0)
                {
                    array->clear();
                }
                else if (new_size > (integer_t)array->size())
                {
                    while (new_size != (integer_t)array->size())
                        array->push(create_null_variable());
                }
                else if (new_size < (integer_t)array->size())
                {
                    while (new_size != (integer_t)array->size())
                        array->pop();
                }

                return array->clone();
            }

            static ref<variable_base> b_array_sub_array(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                integer_t offset = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>();
                integer_t length = std::dynamic_pointer_cast<variable>(args[2])->get_data_as<integer_t>();

                if (offset >= (integer_t)array->size())
                    throw builtin_error("Offset out of the range!");

                if (offset + length > (integer_t)array->size())
                    length = (integer_t)array->size() - offset;

                auto result = make_ref<var_array>();

                for (integer_t i = offset; i < length + offset; ++i)
                    result->push(array->get_element(i));

                return result;
            }

            static ref<variable_base> b_array_string(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                string_t result;

                for (size_t i = 0; i < array->size(); i++)
                {
                    ref<variable_base> elem_base = array->get_element((integer_t)i);
                    if (elem_base->get_type() == variable_type::vt_character)
                        result += std::dynamic_pointer_cast<variable>(elem_base)->get_data_as<character_t>();
                }
                return make_ref<variable>(result, variable_type::vt_string);
            }

            static ref<variable_base> b_array_reverse(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                std::vector<ref<variable_base>> array_data = array->get_data();
                for (size_t i = 0; i < array->size() / 2; ++i)
                {
                    const auto& temp = array->get_element(i);
                    array->set_element(i, array->get_element(array->size() - 1 - i));
                    array->set_element(array->size() - 1 - i, temp);
                }
                return array->clone();
            }

            static ref<variable_base> b_array_reversed(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]->clone());
                std::vector<ref<variable_base>> array_data = array->get_data();
                for (size_t i = 0; i < array->size() / 2; ++i)
                {
                    const auto& temp = array->get_element(i);
                    array->set_element(i, array->get_element(array->size() - 1 - i));
                    array->set_element(array->size() - 1 - i, temp);
                }
                return array;
            }

            static ref<variable_base> b_array_filter(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                ref<variable> type = std::dynamic_pointer_cast<variable>(args[1]);
                variable_type type_data = type->get_data_as<variable_type>();
                
                auto filtered_result = make_ref<var_array>();

                for (size_t i = 0; i < array->size(); ++i)
                    if (array->get_element(i)->get_type() == type_data)
                        filtered_result->push(array->get_element(i));

                return filtered_result;
            }

            static ref<variable_base> b_array_fill(std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);

                if (args.size() == 2)
                {
                    for (size_t i = 0; i < array->size(); ++i)
                        array->set_element(i, args[1]);
                }
                else if (args.size() == 4)
                {
                    integer_t offset = std::dynamic_pointer_cast<variable>(args[1])->get_data_as<integer_t>();
                    integer_t length = std::dynamic_pointer_cast<variable>(args[2])->get_data_as<integer_t>();

                    if (offset > (integer_t)array->size())
                        throw out_of_bounds_error("Offset is out of bounds!");

                    if (offset + length > (integer_t)array->size())
                        length = array->size() - offset;

                    for (integer_t i = offset; i < offset + length; ++i)
                        array->set_element(i, args[3]);
                }
                return array->clone();
            }

            static ref<variable_base> b_array_swap(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> left_array = get_var_array(args[0]);
                ref<var_array> right_array = get_var_array(args[1]);

                std::vector<ref<variable_base>> temp = left_array->get_data();
                left_array->set_data(right_array->get_data());
                right_array->set_data(temp);

                return create_null_variable();
            }

            static ref<variable_base> b_array_empty(const std::vector<ref<variable_base>>& args)
            {
                ref<var_array> array = get_var_array(args[0]);
                return make_ref<variable>(any(array->size() == 0), variable_type::vt_bool);
            }
        }

        ref<symbol_table> b_array::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "Length",  detail::b_array_length,     pa<1>{ variable_base_type::array });
            loader::load_function(table, "Size",    detail::b_array_length,     pa<1>{ variable_base_type::array });
            loader::load_function(table, "Front",   detail::b_array_front,      pa<1>{ variable_base_type::array });
            loader::load_function(table, "Back",    detail::b_array_back,       pa<1>{ variable_base_type::array });
            loader::load_function(table, "At",      detail::b_array_at,         pa<2>{ variable_base_type::array, variable_base_type::variable });
            loader::load_function(table, "Insert",  detail::b_array_insert,     pa<3>{ variable_base_type::array, variable_base_type::variable, variable_base_type::omni });
            loader::load_function(table, "Erase",   detail::b_erase,            pa<2>{ variable_base_type::array, variable_base_type::variable });
            loader::load_function(table, "Push",    detail::b_array_push_back,  pa<2>{ variable_base_type::array, variable_base_type::omni });
            loader::load_function(table, "Pop",     detail::b_array_pop_back,   pa<1>{ variable_base_type::array });
            loader::load_function(table, "Extend",  detail::b_array_extend,     pa<2>{ variable_base_type::array, variable_base_type::array });
            loader::load_function(table, "Clear",   detail::b_array_clear,      pa<1>{ variable_base_type::array });
            loader::load_function(table, "Resize",  detail::b_array_resize,     pa<2>{ variable_base_type::array, variable_base_type::variable });
            loader::load_function(table, "SubArray",detail::b_array_sub_array,  pa<3>{ variable_base_type::array, variable_base_type::variable, variable_base_type::variable });
            loader::load_function(table, "String",  detail::b_array_string,     pa<1>{ variable_base_type::array });
            loader::load_function(table, "Reverse", detail::b_array_reverse,    pa<1>{ variable_base_type::array });
            loader::load_function(table, "Reversed",detail::b_array_reversed,   pa<1>{ variable_base_type::array });
            loader::load_function(table, "Filter",  detail::b_array_filter,     pa<2>{ variable_base_type::array, variable_base_type::variable });
            auto fill_f = loader::load_function(table, "Fill", detail::b_array_fill, pa<2>{ variable_base_type::array, variable_base_type::omni });
            loader::load_overload(fill_f, detail::b_array_fill, pa<4>{ variable_base_type::array, variable_base_type::variable, variable_base_type::variable, variable_base_type::omni });
            loader::load_function(table, "Swap",     detail::b_array_swap,       pa<2>{ variable_base_type::array, variable_base_type::array }, std::bitset<2>("11"));
            loader::load_function(table, "Empty",    detail::b_array_empty,      pa<1>{ variable_base_type::array });

            return result;
        }
    }
}