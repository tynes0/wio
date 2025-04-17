#pragma once

#include <string>
#include <filesystem>

#include "../base/base.h"

namespace wio
{
	class module_tracker
	{
	public:
		id_t add_module(const std::filesystem::path& filepath);
		bool add_module(const std::string& module);

		bool is_imported(const std::filesystem::path& filepath);
		bool is_imported(const std::string& module);

		static module_tracker& get();

	private:
		id_t hash_path_to_id(const std::string& path);
	};
}