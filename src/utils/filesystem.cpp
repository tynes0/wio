#define _CRT_SECURE_NO_WARNINGS
#include "filesystem.h"

#include <fstream>
#include <filesystem>

#include "../interpreter/exception.h"

#define WIO_BUFSIZ 8192

namespace wio
{
	namespace filesystem
	{
		std::filesystem::path get_abs_path(const std::filesystem::path& filepath)
		{
			auto cur = std::filesystem::current_path();
			if (filepath.is_relative())
				return cur / filepath;
			return filepath;
		}

		bool check_extension(const std::string& filepath, const std::string& expected_extension)
		{
			size_t pos = filepath.rfind(expected_extension);
			return (pos == (filepath.size() - expected_extension.size()));
		}

		raw_buffer read_file(const std::string& filepath)
		{
			std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
			if (!stream)
				return {};

			std::streampos end = stream.tellg();
			stream.seekg(0, std::ios::beg);
			uint64_t size = end - stream.tellg();

			if (size == 0)
				return {};

			raw_buffer buffer(size);
			stream.read(buffer.as<char>(), size);
			buffer.data[size] = '\0';
			stream.close();
			return buffer;
		}

		raw_buffer read_file(FILE* file)
		{
			if (!file)
				return {};

			fseek(file, 0, SEEK_END);
			long end = ftell(file);
			fseek(file, 0, SEEK_SET);
			long size = end - ftell(file);
			
			if (size == 0)
				return {};

			raw_buffer buffer(size);
			fread(buffer.as<char>(), 1, size, file);
			buffer.data[size] = '\0';
			return buffer;
		}

		raw_buffer read_stdout()
		{
			raw_buffer buffer;
			stack_buffer<WIO_BUFSIZ> temp{};

			while (fgets(temp.as<char>(), temp.size, stdin))
			{
				size_t len = strlen(temp.as<char>());

				if (len > 0 && temp.data[len - 1] == '\n')
				{
					temp.data[len - 1] = '\0';
					buffer.append(temp.data, len);
					break;
				}
				else
				{
					buffer.append(temp.data, len);
				}
			}

			return buffer;
		}


		bool file_exists(const std::string& filepath)
		{
			return std::filesystem::exists(filepath);
		}

		bool has_prefix(const std::string& str, const std::string& prefix)
		{
			if (str.size() < prefix.size())
				return false;
			return (str.substr(0, prefix.size()) == prefix);
		}

		void write_file(FILE* file, const std::string& str)
		{
			if (!str.empty())
				 fwrite(&str[0], 1, str.size(), file);
		}

		void write_stdout(const std::string& str)
		{
			write_file(stdout, str);
		}

		void write_fp(const std::string& filepath, const std::string& str)
		{
			FILE* f = fopen(filepath.c_str(), "wb");
			if (f)
			{
				write_file(f, str);
				fclose(f);
			}
			else
				throw builtin_error("File'" + filepath + "' cannot open!");
		}
	}
}

