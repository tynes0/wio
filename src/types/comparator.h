#pragma once

#include "../variables/variable_base.h"

namespace wio
{
	struct comparator
	{
		ref<variable_base> equal;
		ref<variable_base> not_equal;
		ref<variable_base> less;
		ref<variable_base> greater;
		ref<variable_base> less_or_equal;
		ref<variable_base> greater_or_equal;
		ref<variable_base> type_equal;
	};
}