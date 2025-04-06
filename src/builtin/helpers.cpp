#include "helpers.h"

namespace wio
{
    namespace builtin
    {
        namespace helpers
        {

            using namespace std::string_literals;

            std::string var_to_string(ref<variable_base> base)
            {
                static int s_member_count = 0;

                if (base->get_type() == variable_type::vt_null)
                    return std::string("null");
                else if (base->get_base_type() == variable_base_type::variable)
                {
                    ref<variable> var = std::dynamic_pointer_cast<variable>(base);
                    if (var->get_type() == variable_type::vt_string)
                    {
                        return s_member_count ? ('\"' + var->get_data_as<std::string>() + '\"') : var->get_data_as<std::string>();
                    }
                    else if (var->get_type() == variable_type::vt_integer)
                    {
                        return std::to_string(var->get_data_as<long long>());
                    }
                    else if (var->get_type() == variable_type::vt_float)
                    {
                        return std::to_string(var->get_data_as<double>());
                    }
                    else if (var->get_type() == variable_type::vt_bool)
                    {
                        return var->get_data_as<bool>() ? std::string("true") : std::string("false");
                    }
                    else if (var->get_type() == variable_type::vt_character)
                    {
                        return s_member_count ? ("'"s + var->get_data_as<char>() + '\'') : std::string(1, var->get_data_as<char>());
                    }
                    else if (var->get_type() == variable_type::vt_type)
                    {
                        std::string_view view = frenum::to_string_view(var->get_data_as<variable_type>());
                        view.remove_prefix(3); // vt_
                        return std::string(view);
                    }
                    else if (var->get_type() == variable_type::vt_pair)
                    {
                        ref<variable> var_ref = std::dynamic_pointer_cast<variable>(base);
                        pair_t p = var_ref->get_data_as<pair_t>();
                        std::stringstream ss;
                        ss << ("[");
                        s_member_count++;
                        ss << var_to_string(p.first);
                        ss << (", ");
                        ss << var_to_string(p.second);
                        s_member_count--;
                        ss << ("]");
                        return ss.str();
                    }
                    else
                        throw builtin_error("Invalid expression in print parameter!");
                }
                else if (base->get_base_type() == variable_base_type::array)
                {
                    ref<var_array> arr = std::dynamic_pointer_cast<var_array>(base);
                    std::stringstream ss;
                    ss << ("[");
                    s_member_count++;
                    for (size_t i = 0; i < arr->size(); ++i)
                    {
                        ss << var_to_string(arr->get_element(i));
                        if (i != arr->size() - 1)
                            ss << (", ");
                    }
                    s_member_count--;
                    ss << ("]");

                    return ss.str();
                }
                else if (base->get_base_type() == variable_base_type::dictionary)
                {
                    ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(base);
                    std::stringstream ss;
                    ss << ("{");
                    const auto& dict_map = dict->get_data();

                    size_t i = 0;
                    s_member_count++;
                    for (const auto& [key, value] : dict_map)
                    {
                        ss << ("[");
                        ss << ('\"' + key + '\"');
                        ss << (", ");
                        ss << var_to_string(value);
                        ss << ("]");
                        if (i != dict->size() - 1)
                            ss << (", ");
                        i++;
                    }
                    s_member_count--;

                    ss << ("}");
                    return ss.str();
                }

                return std::string();
            }
            
            ref<variable_base> create_pair(ref<variable_base> first, ref<variable_base> second)
            {
                pair_t p(first, second);
                ref<variable> result = make_ref<variable>(p, variable_type::vt_pair);
                result->init_members();
                ref<scope> members = result->get_members();

                symbol f1_sym("First", first);
                symbol s1_sym("Second", second);
                symbol f2_sym("Key", first);
                symbol s2_sym("Value", second);

                members->insert("First", f1_sym);
                members->insert("Second", s1_sym);
                members->insert("Key", f2_sym);
                members->insert("Value", s2_sym);

                return result;
            }

            ref<variable_base> create_vec2(ref<variable_base> xvb, ref<variable_base> yvb)
            {
                return nullptr;
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

