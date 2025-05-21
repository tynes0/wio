#include "unit.h"

namespace wio
{
	unit::unit(packed_bool flags) : variable_base(flags)
	{

	}

	unit::unit(const std::string& identifier, packed_bool flags) : m_identifier(identifier), variable_base(flags)
	{
	}

	variable_base_type unit::get_base_type() const
	{
		return variable_base_type::unit;
	}

	variable_type unit::get_type() const
	{
		return variable_type::vt_unit;
	}

	ref<variable_base> unit::clone() const
	{
		return make_ref<unit>(*this);
	}

	void unit::set_identifier(const std::string& identifier)
	{
		m_identifier = identifier;
	}

	const std::string& unit::get_identifier()
	{
		return m_identifier;
	}
}
