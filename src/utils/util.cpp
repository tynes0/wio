#include "util.h"

#include "../base/exception.h"

#include "../variables/variable.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../variables/function.h"
#include "../variables/overload_list.h"
#include "../variables/realm.h"

#include "../types/file_wrapper.h"
#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../types/comparator.h"

#include <string>

namespace wio
{
	namespace util
	{
        static string_t scientific_to_string(double d)
        {
            if (d == 0.0 || (std::signbit(d) && d == 0.0))
                return "0.0e+00";

            constexpr int precision = 15;
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*e", precision - 1, d);
            return string_t(buffer);
        }

        string_t float_t_to_string(float_t d)
        {
            if (d == 0.0 || (std::signbit(d) && d == 0.0))
                return "0.0";

            double abs_val = std::fabs(d);

            if (abs_val < 1e-6 || abs_val > 1e+9)
                return scientific_to_string(d);

            constexpr int max_significant_digits = 15;
            constexpr int max_precision = 10;

            int magnitude = (abs_val > 0.0) ? static_cast<int>(std::log10(abs_val)) : 0;
            int precision = max_significant_digits - magnitude;
            if (precision > max_precision) precision = max_precision;
            if (precision < 1) precision = 1;

            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*f", precision, d);

            char* end = buffer + std::strlen(buffer) - 1;
            while (*end == '0' && *(end - 1) != '.') 
            {
                *end = '\0';
                --end;
            }
            if (*end == '.') 
            {
                *(end + 1) = '0';
                *(end + 2) = '\0';
            }

            return string_t(buffer);
        }

        string_t type_to_string(variable_type vt)
		{
			std::string_view view = frenum::to_string_view(vt);
			view.remove_prefix(3);
			return string_t(view);
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

			throw invalid_string_error("Invalid escape sequence: \\" + string_t(1, ch));
		}

        string_t var_to_string(ref<variable_base> base)
        {
            using namespace std::string_literals;

            static int s_member_count = 0;

            if (base->get_type() == variable_type::vt_null)
                return string_t("null");
            else if (base->get_base_type() == variable_base_type::variable)
            {
                ref<variable> var = std::dynamic_pointer_cast<variable>(base);
                if (var->get_type() == variable_type::vt_string)
                {
                    return s_member_count ? ('\"' + var->get_data_as<string_t>() + '\"') : var->get_data_as<string_t>();
                }
                else if (var->get_type() == variable_type::vt_integer)
                {
                    return std::to_string(var->get_data_as<integer_t>());
                }
                else if (var->get_type() == variable_type::vt_float)
                {
                    return util::float_t_to_string(var->get_data_as<double>());
                }
                else if (var->get_type() == variable_type::vt_float_ref)
                {
                    return util::float_t_to_string(*var->get_data_as<double*>());
                }
                else if (var->get_type() == variable_type::vt_bool)
                {
                    return var->get_data_as<bool>() ? string_t("true") : string_t("false");
                }
                else if (var->get_type() == variable_type::vt_character)
                {
                    return s_member_count ? ("'"s + var->get_data_as<char>() + '\'') : string_t(1, var->get_data_as<char>());
                }
                else if (var->get_type() == variable_type::vt_character_ref)
                {
                    return s_member_count ? ("'"s + *var->get_data_as<char*>() + '\'') : string_t(1, *var->get_data_as<char*>());
                }
                else if (var->get_type() == variable_type::vt_type)
                {
                    return util::type_to_string(var->get_data_as<variable_type>());
                }
                else if (var->get_type() == variable_type::vt_file)
                {
                    return var->get_data_as<file_wrapper>().get_filename();
                }
                else if (var->get_type() == variable_type::vt_pair)
                {
                    ref<variable> var_ref = std::dynamic_pointer_cast<variable>(base);
                    pair_t p = var_ref->get_data_as<pair_t>();
                    std::stringstream ss;
                    ss << ("[");
                    s_member_count++;
                    ss << var_to_string(p.first);
                    ss << (", ");
                    ss << var_to_string(p.second);
                    s_member_count--;
                    ss << ("]");
                    return ss.str();
                }
                else if (var->get_type() == variable_type::vt_vec2)
                {
                    ref<variable> var_ref = std::dynamic_pointer_cast<variable>(base);
                    const vec2& v = var_ref->get_data_as<vec2>();
                    std::stringstream ss;
                    ss << ("(");
                    ss << util::float_t_to_string(v.x);
                    ss << (", ");
                    ss << util::float_t_to_string(v.y);
                    ss << (")");
                    return ss.str();
                }
                else if (var->get_type() == variable_type::vt_vec3)
                {
                    ref<variable> var_ref = std::dynamic_pointer_cast<variable>(base);
                    const vec3& v = var_ref->get_data_as<vec3>();
                    std::stringstream ss;
                    ss << ("(");
                    ss << util::float_t_to_string(v.x);
                    ss << (", ");
                    ss << util::float_t_to_string(v.y);
                    ss << (", ");
                    ss << util::float_t_to_string(v.z);
                    ss << (")");
                    return ss.str();
                }
                else if (var->get_type() == variable_type::vt_comparator)
                {
                    ref<variable> var_ref = std::dynamic_pointer_cast<variable>(base);
                    const comparator& v = var_ref->get_data_as<comparator>();
                    std::stringstream ss;

                    static auto add_to_stream = [&ss](const string_t& id, ref<variable_base> item) 
                        {
                            ss << ("[");
                            ss << id;
                            ss << (" : ");
                            ss << var_to_string(item);
                            ss << ("]\n");
                        };

                    s_member_count++;
                    add_to_stream("TypeEqual", v.type_equal);
                    add_to_stream("Equal", v.equal);
                    add_to_stream("NotEqual", v.not_equal);
                    add_to_stream("Less", v.less);
                    add_to_stream("LessOrEqual", v.less_or_equal);
                    add_to_stream("Greater", v.greater);
                    add_to_stream("GreaterOrEqual", v.greater_or_equal);
                    s_member_count--;
                    return ss.str();
                }
                else
                {
                    throw builtin_error("Invalid expression!");
                }
            }
            else if (base->get_base_type() == variable_base_type::array)
            {
                ref<var_array> arr = std::dynamic_pointer_cast<var_array>(base);
                std::stringstream ss;
                ss << ("[");
                s_member_count++;
                for (size_t i = 0; i < arr->size(); ++i)
                {
                    ss << var_to_string(arr->get_element(i));
                    if (i != arr->size() - 1)
                        ss << (", ");
                }
                s_member_count--;
                ss << ("]");

                return ss.str();
            }
            else if (base->get_base_type() == variable_base_type::dictionary)
            {
                ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(base);
                std::stringstream ss;
                ss << ("{");
                const auto& dict_map = dict->get_data();

                size_t i = 0;
                s_member_count++;
                for (const auto& [key, value] : dict_map)
                {
                    ss << ('\"' + key + '\"');
                    ss << (" : ");
                    ss << var_to_string(value);
                    if (i != dict->size() - 1)
                        ss << (", ");
                    i++;
                }
                s_member_count--;

                ss << ("}");
                return ss.str();
            }
            else if (base->get_base_type() == variable_base_type::function)
            {
                ref<overload_list> func = std::dynamic_pointer_cast<overload_list>(base);
                std::stringstream ss;

                const auto& id = func->get_symbol_id();

                ss << '@' << (!id.empty() ? id : "\'anonymous\'"s);

                return ss.str();
            }

            return string_t();
        }

        constexpr OS get_OS()
        {
#if defined(_WIN32)
            return OS::Windows;
#elif defined(__ANDROID__)
            return OS::Android;

#elif defined(__APPLE__) && defined(__MACH__)
#   include <TargetConditionals.h>
#   if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
            return OS::iOS;
#   else
            return OS::MacOS;
#   endif
#elif defined(__linux__)
            return OS::Linux;
#elif defined(__FreeBSD__)
            return OS::FreeBSD;
#elif defined(__OpenBSD__)
            return OS::OpenBSD;
#elif defined(__NetBSD__)
            return OS::NetBSD;
#elif defined(__DragonFly__)
            return OS::DragonFlyBSD;
#elif defined(__sun) && defined(__SVR4)
            return OS::Solaris;
#elif defined(_AIX)
            return OS::AIX;
#elif defined(__hpux)
            return OS::HP_UX;
#elif defined(__EMSCRIPTEN__)
            return OS::Emscripten;
#else
            return OS::Unknown;
#endif
        }
	}
}