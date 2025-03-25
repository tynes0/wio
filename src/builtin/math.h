#pragma once

#include "../interpreter/scope.h"

namespace wio
{
	namespace builtin
	{
		class math
		{
		public:
			void load(ref<scope> target_scope);
		};
	}
}