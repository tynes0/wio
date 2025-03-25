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
                return array->get_element(0);
            }

            static ref<variable_base> b_array_back(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                return array->get_element(array->size() - 1);
            }

            static ref<variable_base> b_array_at(ref<variable_base> base_array, ref<variable_base> idx)
            {
                ref<var_array> array = get_var_array(base_array);
                return array->get_element(std::dynamic_pointer_cast<variable>(idx)->get_data_as<long long>());
            }

            static  ref<variable_base> b_array_append(ref<variable_base> base_array, ref<variable_base> value)
            {
                ref<var_array> array = get_var_array(base_array);
                array->get_data().push_back(value);
                return make_ref<null_var>();
            }

            static ref<variable_base> b_array_pop(ref<variable_base> base_array, ref<variable_base> idx)
            {
                ref<var_array> array = get_var_array(base_array);
                auto& data = array->get_data();
                long long ll_idx = std::dynamic_pointer_cast<variable>(idx)->get_data_as<long long>();
                ref<variable_base> result = array->get_element(ll_idx);
                data.erase(data.begin() + ll_idx, data.begin() + ll_idx + 1);
                return result;
            }

            static ref<variable_base> b_array_pop_back(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                auto& result = array->get_data().back();
                array->get_data().pop_back();
                return result;
            }

            static ref<variable_base> b_array_extend(ref<variable_base> base_array, ref<variable_base> right_array)
            {
                ref<var_array> array = get_var_array(base_array);
                ref<var_array> rarray = get_var_array(right_array);

                for (auto& item : rarray->get_data())
                    array->get_data().push_back(item);

                return make_ref<null_var>();
            }

            static ref<variable_base> b_array_clear(ref<variable_base> base_array)
            {
                ref<var_array> array = get_var_array(base_array);
                array->get_data().clear();
                return make_ref<null_var>();
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
        }

        ref<scope> b_array::load()
        {
            auto result = make_ref<scope>(scope_type::builtin);

            loader::load_function<1>(result, "Length", detail::b_array_length, variable_type::vt_integer, { variable_type::vt_array });
            loader::load_function<1>(result, "Front", detail::b_array_front, variable_type::vt_any, { variable_type::vt_array });
            loader::load_function<1>(result, "Back", detail::b_array_back, variable_type::vt_any, { variable_type::vt_array });
            loader::load_function<2>(result, "At", detail::b_array_at, variable_type::vt_any, { variable_type::vt_array, variable_type::vt_integer });
            loader::load_function<2>(result, "Append", detail::b_array_append, variable_type::vt_null, { variable_type::vt_array, variable_type::vt_any });
            loader::load_function<2>(result, "Pop", detail::b_array_pop, variable_type::vt_any, { variable_type::vt_array, variable_type::vt_integer });
            loader::load_function<1>(result, "PopBack", detail::b_array_pop_back, variable_type::vt_any, { variable_type::vt_array });
            loader::load_function<2>(result, "Extend", detail::b_array_extend, variable_type::vt_null, { variable_type::vt_array, variable_type::vt_array });
            loader::load_function<1>(result, "Clear", detail::b_array_clear, variable_type::vt_null, { variable_type::vt_array });
            loader::load_function<3>(result, "SubArray", detail::b_array_sub_array, variable_type::vt_array, { variable_type::vt_array, variable_type::vt_integer, variable_type::vt_integer });

            return result;
        }
    }
}