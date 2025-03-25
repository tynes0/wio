#pragma once

#include "../interpreter/scope.h"

namespace wio
{
	class builtin_manager
	{
	public:
		static void load(ref<scope> target_scope, std::map<variable_type, ref<scope>>& target_member_scope_map);
	};
}