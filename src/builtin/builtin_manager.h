#pragma once

#include "../interpreter/scope.h"

namespace wio
{
	class builtin_manager
	{
	public:
		static void load(std::map<variable_type, ref<symbol_table>>& target_member_scope_map);
	};
}