#pragma once

#include "../interpreter/scope.h"

namespace wio
{
	class builtin_manager
	{
	public:
		static void load(std::map<variable_type, ref<scope>>& target_member_scope_map);
	};
}