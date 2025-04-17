#pragma once

#include "../interpreter/scope.h"

namespace wio
{
	namespace builtin
	{
		class scientific
		{
		public:
			static void load(ref<scope> target_scope = nullptr);
			static void load_table(ref<symbol_table> target_table);
			static void load_symbol_map(symbol_map& target_map);
		};
	}
}