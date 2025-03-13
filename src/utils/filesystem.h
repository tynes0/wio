#pragma once

#include <string>
#include <fstream>
#include <filesystem>

namespace wio
{
	inline bool check_extension(const std::string& filepath, const std::string& expected_extension)
	{
		size_t pos = filepath.rfind(expected_extension);
		return (pos == (filepath.size() - expected_extension.size()));
	}

	inline std::string read_file(const std::string& filepath)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream)
			return std::string{};

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint64_t size = end - stream.tellg();

		if (size == 0)
			return std::string{};

		std::string result;
		result.resize(size);
		stream.read(result.data(), size);
		stream.close();
		return result;
	}

	inline bool file_exists(const std::string& filepath)
	{
		return std::filesystem::exists(filepath);
	}

	inline bool has_prefix(const std::string& str, const std::string& prefix)
	{
		return (str.substr(0, prefix.size()) == prefix);
	}
}