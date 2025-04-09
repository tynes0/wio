#include "util.h"

#include "../base/exception.h"

namespace wio
{
	namespace util
	{
		std::string double_to_string(double d)
		{
			std::string s = std::to_string(d);
			while (s.back() == '0' && s.size() != 1 && std::isdigit(s[s.size() - 2]))
				s.pop_back();
			return s;
		}

		std::string type_to_string(variable_type vt)
		{
			std::string_view view = frenum::to_string_view(vt);
			view.remove_prefix(3);
			return std::string(view);
		}

		char get_escape_seq(char ch)
		{
			if (ch == '\\' || ch == '\'' || ch == '\"')
				return ch;
			else if (ch == 'n') // newline
				return '\n';
			else if (ch == 'r') // return
				return '\r';
			else if (ch == 'a') // alert
				return '\a';
			else if (ch == 'b') // backspace
				return '\b';
			else if (ch == 'f') // formfeed page break
				return '\f';
			else if (ch == 't') // horizontal tab
				return '\t';
			else if (ch == 'v') // vertical tab
				return '\v';

			throw invalid_string_error("Invalid escape sequence: \\" + std::string(1, ch));
		}
	}
}