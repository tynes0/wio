#include "utility.h"

#include "builtin_base.h"

namespace wio
{
	namespace builtin
	{
		namespace detail
		{
			static ref<variable_base> b_swap(ref<variable_base> lhs, ref<variable_base> rhs)
			{
				return ref<null_var>();
			}
		}

		void utility::load()
		{
			loader::load_function<2>("Swap", detail::b_swap, variable_type::vt_null, { variable_type::vt_any, variable_type::vt_any }, std::bitset<2>("11"));
		}
	}
}