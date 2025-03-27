#pragma once

#include <string>
#include <sstream>

namespace wio
{
	struct location
	{
		size_t line = 0;
		size_t row = 0;

		std::string to_string() const
		{
			std::stringstream ss;
			ss << "Location: Line -> " << line << " Row -> " << row;
			return ss.str();
		}
	};

}