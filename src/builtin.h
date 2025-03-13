#pragma once

#include "scope.h"

namespace wio
{
	class builtin
	{
	public:
		static ref<scope> load();
	};
}