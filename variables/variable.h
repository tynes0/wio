#pragma once

#include <string>

#include "../utils/any.h"
#include "../utils/frenum.h"
#include "../base.h"

namespace wio
{
    enum class variable_base_type
    {
        none = 0, variable, array, dictionary, function
    };

    MakeFrenumInNamespace(wio, variable_base_type, none, variable, array, dictionary, function)

    class variable_base
    {
    public:
        virtual variable_base_type get_base_type() const = 0;
    };

    enum class variable_type
    {
        vt_none, // nothing
        vt_integer, // long long
        vt_float, // float
        vt_string, // std::string -> would be change -> may be chi_string (ugly but faster)
        vt_character, // char
        vt_bool, // bool
    };

    MakeFrenumInNamespace(wio, variable_type, vt_none, vt_integer, vt_float, vt_string, vt_character, vt_bool)

    class variable : public variable_base
    {
    public:
        variable() {}

        variable(const std::string& id, any data, variable_type type, bool is_constant = false) 
            : m_id(id), m_data(data), m_type(type), m_is_constant(is_constant) {}

        const std::string& get_id() const
        {
            return m_id;
        }

        any& get_data()
        {
            return m_data;
        }

        variable_type get_type() const
        {
            return m_type;
        }

        bool is_constant() const
        {
            return m_is_constant;
        }

        bool has_value() const
        {
            return !m_data.empty();
        }

        virtual variable_base_type get_base_type() const override
        {
            return variable_base_type::variable;
        }

    private:
        std::string m_id;
        any m_data;
        variable_type m_type = variable_type::vt_none;
        bool m_is_constant = false;
    };
}