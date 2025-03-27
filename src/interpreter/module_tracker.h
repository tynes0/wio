#pragma once

#include <string>
#include <filesystem>

namespace wio
{
	class module_tracker
	{
	public:
		bool add_module(const std::filesystem::path& filepath);
		bool add_module(const std::string& module);

		bool is_imported(const std::filesystem::path& filepath);
		bool is_imported(const std::string& module);

		static module_tracker& get();
	};
}