#include "b_array.h"
#include "builtin_base.h"

#include "../variables/array.h"

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
                    throw exception("Array is null!");
                return array;
            }

            static ref<variable_base> b_array_length(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                return make_ref<variable>((long long)array->size(), variable_type::vt_integer);
            }
            
            static ref<variable_base> b_array_front(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                if(array->is_pf_return_ref())
                    return array->get_element(0);
                return array->get_element(0)->clone();
            }

            static ref<variable_base> b_array_back(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                return array->get_element(array->size() - 1);
                if (array->is_pf_return_ref())
                    return array->get_element(array->size() - 1);
                return array->get_element(array->size() - 1)->clone();
            }

            static ref<variable_base> b_array_at(ref<variable_base> base_array, ref<variable_base> idx)
            {
                ref<var_array> array = get_var_array(base_array);
                if (array->is_pf_return_ref())
                    return array->get_element(std::dynamic_pointer_cast<variable>(idx)->get_data_as<long long>());
                return array->get_element(std::dynamic_pointer_cast<variable>(idx)->get_data_as<long long>())->clone();
            }

            static  ref<variable_base> b_array_push(ref<variable_base> base_array, ref<variable_base> value)
            {
                ref<var_array> array = get_var_array(base_array);
                array->push(value);
                return create_null_variable();
            }

            static ref<variable_base> b_array_pop(ref<variable_base> base_array, ref<variable_base> idx)
            {
                ref<var_array> array = get_var_array(base_array);
                auto& data = array->get_data();
                long long ll_idx = std::dynamic_pointer_cast<variable>(idx)->get_data_as<long long>();
                ref<variable_base> result = array->get_element(ll_idx);
                array->erase(ll_idx);
                return result;
            }

            static ref<variable_base> b_array_pop_back(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                return array->pop();
            }

            static ref<variable_base> b_array_extend(ref<variable_base> base_array, ref<variable_base> right_array)
            {
                ref<var_array> array = get_var_array(base_array);
                ref<var_array> rarray = get_var_array(right_array);

                for (auto& item : rarray->get_data())
                    array->push(item);

                return create_null_variable();
            }

            static ref<variable_base> b_array_clear(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                array->clear();
                return create_null_variable();
            }

            static ref<variable_base> b_array_sub_array(ref<variable_base> base_array, ref<variable_base> base_offset, ref<variable_base> base_length)
            {
                ref<var_array> array = get_var_array(base_array);
                long long offset = std::dynamic_pointer_cast<variable>(base_offset)->get_data_as<long long>();
                long long length = std::dynamic_pointer_cast<variable>(base_length)->get_data_as<long long>();

                if (offset >= (long long)array->size())
                    throw builtin_error("Offset out of the range!");

                if (offset + length > (long long)array->size())
                    length = (long long)array->size() - offset;

                auto& array_data = array->get_data();
                std::vector<ref<variable_base>> new_data(array_data.begin() + offset, array_data.begin() + offset + length);
                
                return make_ref<var_array>(new_data);
            }

            static ref<variable_base> b_array_string(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                std::string result;

                for (size_t i = 0; i < array->size(); i++)
                {
                    ref<variable_base> elem_base = array->get_element((long long)i);
                    if (elem_base->get_type() == variable_type::vt_character)
                        result += std::dynamic_pointer_cast<variable>(elem_base)->get_data_as<char>();
                }
                return make_ref<variable>(result, variable_type::vt_string);
            }

            static ref<variable_base> b_array_fill(ref<variable_base> base_array, ref<variable_base> value)
            {
                ref<var_array> array = get_var_array(base_array);

                for (size_t i = 0; i < array->size(); ++i)
                    array->set_element(i, value);

                return array;
            }

            static ref<variable_base> b_array_fill2(ref<variable_base> base_array, ref<variable_base> base_offset, ref<variable_base> base_length, ref<variable_base> value)
            {
                ref<var_array> array = get_var_array(base_array);
                long long offset = std::dynamic_pointer_cast<variable>(base_offset)->get_data_as<long long>();
                long long length = std::dynamic_pointer_cast<variable>(base_length)->get_data_as<long long>();

                if (offset > (long long)array->size())
                    throw builtin_error("");

                if (offset + length > (long long)array->size())
                    length = array->size() - offset;

                for (long long i = offset; i < offset + length; ++i)
                    array->set_element(i, value);

                return array;
            }
        }

        ref<scope> b_array::load()
        {
            auto result = make_ref<scope>(scope_type::builtin);

            loader::load_function<1>(result, "Length",  detail::b_array_length,     { variable_type::vt_array });
            loader::load_function<1>(result, "Front",   detail::b_array_front,      { variable_type::vt_array });
            loader::load_function<1>(result, "Back",    detail::b_array_back,       { variable_type::vt_array });
            loader::load_function<2>(result, "At",      detail::b_array_at,         { variable_type::vt_array, variable_type::vt_integer });
            loader::load_function<2>(result, "Push",    detail::b_array_push,       { variable_type::vt_array, variable_type::vt_any });
            loader::load_function<2>(result, "Pop",     detail::b_array_pop,        { variable_type::vt_array, variable_type::vt_integer });
            loader::load_function<1>(result, "PopBack", detail::b_array_pop_back,   { variable_type::vt_array });
            loader::load_function<2>(result, "Extend",  detail::b_array_extend,     { variable_type::vt_array, variable_type::vt_array });
            loader::load_function<1>(result, "Clear",   detail::b_array_clear,      { variable_type::vt_array });
            loader::load_function<3>(result, "SubArray", detail::b_array_sub_array, { variable_type::vt_array, variable_type::vt_integer, variable_type::vt_integer });
            loader::load_function<1>(result, "String",  detail::b_array_string,     { variable_type::vt_array });

            auto fill_f = loader::load_function<2>(result, "Fill", detail::b_array_fill, { variable_type::vt_array, variable_type::vt_any });
            loader::load_overload<4>(fill_f, detail::b_array_fill2, { variable_type::vt_array, variable_type::vt_integer, variable_type::vt_integer, variable_type::vt_any });

            return result;
        }
    }
}