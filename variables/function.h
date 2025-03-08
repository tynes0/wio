#pragma once

#include <string>
#include <vector>
#include "variables/variable.h" // variable_type

namespace wio 
{

    class function : public variable_base 
    {
    public:
        function(const std::string& name, const std::vector<std::string>& param_names, variable_type return_type)
            : m_name(name), m_param_names(param_names), m_return_type(return_type) {}

        function() : m_return_type(variable_type::vt_none) {}

        const std::string& get_name() const { return m_name; }

        const std::vector<std::string>& get_param_names() const { return m_param_names; }


        variable_type get_return_type() const { return m_return_type; }

        void set_return_type(variable_type type) { m_return_type = type; }

        variable_base_type get_base_type() const override { return variable_base_type::function; }
        std::string to_string() const override { return "function " + m_name; }

    private:
        std::string m_name;
        std::vector<std::string> m_param_names;
        variable_type m_return_type;
    };

} // namespace wio