#include "realm.h"

namespace wio
{
	realm::realm(packed_bool flags) : variable_base(flags)
	{
		
	}

	variable_base_type realm::get_base_type() const
	{
		return variable_base_type::realm;
	}

	variable_type realm::get_type() const
	{
		return variable_type::vt_realm;
	}

	ref<variable_base> realm::clone() const
	{
		return make_ref<realm>(*this);
	}
}