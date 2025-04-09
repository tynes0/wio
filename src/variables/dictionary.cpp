#include "dictionary.h"
#include "variable.h"

#include "../utils/util.h"
#include "../types/file_wrapper.h"

namespace wio
{
    var_dictionary::var_dictionary(const map_t& data, packed_bool flags) : m_data(data), variable_base(flags)
    {
    }

    variable_base_type wio::var_dictionary::get_base_type() const
    {
        return variable_base_type::dictionary;
    }

    variable_type var_dictionary::get_type() const
    {
        return variable_type::vt_dictionary;
    }

    ref<variable_base> var_dictionary::clone() const
    {
        return make_ref<var_dictionary>(*this);
    }

    var_dictionary::map_t& var_dictionary::get_data()
    {
        return m_data;
    }

    const var_dictionary::map_t& var_dictionary::get_data() const
    {
        return m_data;
    }

    ref<variable_base> var_dictionary::get_element(ref<variable_base> key)
    {
        std::string skey = as_key(key);
        if (check_existance(skey))
            return m_data[skey];
        else
            throw invalid_key_error("Key '" + skey + "' is not exists!");
    }

    void var_dictionary::set_data(const map_t& data)
    {
        m_data.clear();
        for (auto& [key, value] : data)
            m_data[key] = value->clone();
    }

    void var_dictionary::set_element(ref<variable_base> key, ref<variable_base> value)
    {
        std::string skey = as_key(key);
        if (check_existance(skey))
            throw invalid_key_error("Key '" + skey + "'' is already exists!");
        else
            m_data[skey] = value->clone();
    }

    size_t var_dictionary::size() const
    {
        return m_data.size();
    }

    bool var_dictionary::check_existance(const std::string& key)
    {
        return (m_data.find(key) != m_data.end());
    }

    std::string var_dictionary::as_key(ref<variable_base> value)
    {
        ref<variable> var = std::dynamic_pointer_cast<variable>(value);

        if(var->get_type() == variable_type::vt_integer)
            return std::to_string(var->get_data_as<long long>());
        else if(var->get_type() == variable_type::vt_string)
            return var->get_data_as<std::string>();
        else if(var->get_type() == variable_type::vt_float)
            return std::to_string(var->get_data_as<double>());
        else if(var->get_type() == variable_type::vt_float_ref)
            return std::to_string(*var->get_data_as<double*>());
        else if(var->get_type() == variable_type::vt_file)
            return var->get_data_as<file_wrapper>().get_filename();
        else if(var->get_type() == variable_type::vt_character)
            return std::to_string(var->get_data_as<char>());
        else if(var->get_type() == variable_type::vt_character_ref)
            return std::to_string(*var->get_data_as<char*>());

        throw invalid_key_error("Type '" + util::type_to_string(var->get_type()) + "' can not be used as key!");
    }
}
