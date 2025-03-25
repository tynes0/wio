#pragma once

#include <string>
#include <filesystem>

#include "buffer.h"

namespace wio
{
	namespace filesystem
	{
		std::filesystem::path get_abs_path(const std::filesystem::path& filepath);
		bool check_extension(const std::string& filepath, const std::string& expected_extension);
		raw_buffer read_file(const std::string& filepath);
		raw_buffer read_file(FILE* file);
		raw_buffer read_stdout();
		bool file_exists(const std::string& filepath);
		bool has_prefix(const std::string& str, const std::string& prefix);
		void write_file(FILE* file, const std::string& str);
		void write_stdout(const std::string& str);
		void write_fp(const std::string& filepath, const std::string& str);
	}
}