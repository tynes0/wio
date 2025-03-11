#pragma once

#include <vector>
#include <string>

namespace wio
{
	class application
	{
	public:
		application(int argc, const char** argv);
		void run();
	};
}