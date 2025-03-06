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

	class exception
	{
	public:
		exception() noexcept : m_data() {}

		explicit exception(const char* message) noexcept : m_data(message) {}

		exception(const exception& other) noexcept : m_data(other.m_data) {}

		exception& operator=(const exception& other) noexcept
		{
			m_data = other.m_data;
		}

		[[nodiscard]] virtual char const* what() const
		{
			return !m_data.empty() ? m_data.c_str() : "Unknown exception";
		}
	protected:
		std::string m_data;
	};

	class local_exception : public exception
	{
	public:
		explicit local_exception(const std::string& message, const location& loc) noexcept : exception()
		{
			m_data.append(message).append(" ").append(loc.to_string());
		}

		explicit local_exception(const char* message) noexcept : exception(message) {}

	};

	class syntax_error : public local_exception
	{
	public:
		explicit syntax_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit syntax_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_token_error : public local_exception
	{
	public:
		explicit invalid_token_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_token_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_number_error : public local_exception
	{
	public:
		explicit invalid_number_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_number_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_string_error : public local_exception
	{
	public:
		explicit invalid_string_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_string_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class unterminated_string_error : public local_exception
	{
	public:
		explicit unterminated_string_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit unterminated_string_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class unterminated_comment_error : public local_exception
	{
	public:
		explicit unterminated_comment_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit unterminated_comment_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class unexpected_character_error : public local_exception
	{
	public:
		explicit unexpected_character_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit unexpected_character_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class parse_error : public local_exception
	{
	public:
		explicit parse_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit parse_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

}