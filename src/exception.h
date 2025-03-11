#pragma once

#include <string>
#include <sstream>

#include "location.h"

namespace wio
{
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

	class out_of_bounds_error : public exception
	{
	public:
		explicit out_of_bounds_error(const std::string& message) noexcept : exception(message.c_str()) {}
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

	class parse_error : public local_exception
	{
	public:
		explicit parse_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit parse_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class unexpected_character_error : public parse_error
	{
	public:
		explicit unexpected_character_error(const std::string& message, const location& loc) noexcept : parse_error(message, loc) {}
		explicit unexpected_character_error(const std::string& message) noexcept : parse_error(message.c_str()) {}
	};

	class unexpected_token_error : public parse_error
	{
	public:
		explicit unexpected_token_error(const std::string& message, const location& loc) noexcept : parse_error(message, loc) {}
		explicit unexpected_token_error(const std::string& message) noexcept : parse_error(message.c_str()) {}
	};

	class no_argument_error : public exception
	{
	public:
		explicit no_argument_error(const std::string& message) noexcept : exception(message.c_str()) {}
	};

	class undefined_argument_error : public exception
	{
	public:
		explicit undefined_argument_error(const std::string& message) noexcept : exception(message.c_str()) {}
	};

	class file_error : public exception
	{
	public:
		explicit file_error(const std::string& message) noexcept : exception(message.c_str()) {}
	};

	class undefined_identifier_error : public local_exception
	{
	public:
		explicit undefined_identifier_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit undefined_identifier_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_ast_error : public exception
	{
	public:
		explicit invalid_ast_error(const std::string& message) noexcept : exception(message.c_str()) {}
	};

	class invalid_literal_error : public exception
	{
	public:
		explicit invalid_literal_error(const std::string& message) noexcept : exception(message.c_str()) {}
	};

	class invalid_type_error : public local_exception
	{
	public:
		explicit invalid_type_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_type_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_operator_error : public local_exception
	{
	public:
		explicit invalid_operator_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_operator_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_operation_error : public local_exception
	{
	public:
		explicit invalid_operation_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_operation_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_assignment_error : public local_exception 
	{
	public:
		explicit invalid_assignment_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
	};

	class invalid_member_error : public local_exception
	{
	public:
		explicit invalid_member_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_member_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_break_statement : public local_exception
	{
	public:
		explicit invalid_break_statement(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_break_statement(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_continue_statement : public local_exception
	{
	public:
		explicit invalid_continue_statement(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_continue_statement(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_return_statement : public local_exception
	{
	public:
		explicit invalid_return_statement(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_return_statement(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_return_type : public local_exception
	{
	public:
		explicit invalid_return_type(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_return_type(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_declaration_error: public local_exception
	{
	public:
		explicit invalid_declaration_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_declaration_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_argument_count_error : public local_exception
	{
	public:
		explicit invalid_argument_count_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_argument_count_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_import_error : public local_exception
	{
	public:
		explicit invalid_import_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit invalid_import_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class evaluation_error : public local_exception
	{
	public:
		explicit evaluation_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit evaluation_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class type_mismatch_error : public local_exception
	{
	public:
		explicit type_mismatch_error(const std::string& message, const location& loc) noexcept : local_exception(message, loc) {}
		explicit type_mismatch_error(const std::string& message) noexcept : local_exception(message.c_str()) {}
	};

	class invalid_key_error : public exception
	{
	public:
		explicit invalid_key_error(const std::string& message) noexcept : exception(message.c_str()) {}
	};
}