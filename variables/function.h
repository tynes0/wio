#pragma once

#include <vector>
#include <memory>

#include "../base.h"
#include "variable.h"

namespace wio
{
	class function : public variable_base
	{
	public:
        function() {}

        function(const std::string& id, ref<void> reference, variable_type return_type, const std::vector<variable_base_type>& parameter_types)
            : m_id(id), m_reference(reference), m_return_type(return_type), m_parameter_types(parameter_types) {}

        const std::string& get_id() const
        {
            return m_id;
        }

        ref<void> get_ref()
        {
            return m_reference;
        }

        const std::vector<variable_base_type>& get_parameter_types() const
        {
            return m_parameter_types;
        }

        variable_type get_return_type() const
        {
            return m_return_type;
        }

        bool has_value() const
        {
            return (bool)m_reference;
        }

        virtual variable_base_type get_base_type() const override
        {
            return variable_base_type::function;
        }

    private:
        std::string m_id;
        ref<void> m_reference;
        variable_type m_return_type = variable_type::vt_none;
        std::vector<variable_base_type> m_parameter_types;
	};
}