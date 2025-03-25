#pragma once

#include <filesystem>

namespace wio
{
	class module_tracker
	{
	public:
		bool add_module(const std::filesystem::path& filepath);
		bool is_imported(const std::filesystem::path& filepath);

		static module_tracker& get();
	};
}