#include "system.h"

#include "builtin_base.h"
#include "helpers.h"

#ifdef _WIN32
	#include <windows.h>
	#define popen _popen
	#define pclose _pclose
#else
	#include <unistd.h>
#endif

#include <sstream>

namespace wio
{
	namespace builtin
	{
		using namespace std::string_literals;

		namespace detail
		{
			typedef enum
			{
				Windows, Linux, MacOS
			} OS;
			
			static ref<variable_base> b_clear_screen(const std::vector<ref<variable_base>>& args)
			{
#ifdef _WIN32
				std::system("cls");
#elif __unix__ || __APPLE__ || __linux__
				std::system("clear");
#else
#endif
				return create_null_variable();
			}
			static ref<variable_base> b_get_os(const std::vector<ref<variable_base>>& args) // TODO: EXTEND!
			{
#ifdef _WIN32
				return make_ref<variable>(any(integer_t(Windows)), variable_type::vt_integer);
#elif __linux__
				return make_ref<variable>(any(integer_t(Linux)), variable_type::vt_integer);
#elif __APPLE__ 
				return make_ref<variable>(any(integer_t(MacOS))), variable_type::vt_integer);
#else
				return create_null_variable();
#endif
			}

			static ref<variable_base> b_run_command(const std::vector<ref<variable_base>>& args)
			{
				if (!(args[0]->get_type() == variable_type::vt_string))
					return create_null_variable();

				string_t command = std::dynamic_pointer_cast<variable>(args[0])->get_data_as<string_t>();
				
				if (command.empty())
					throw undefined_argument_error("Empty command string!");

				FILE* pipe = popen(command.c_str(), "r");
				if (!pipe)
					throw builtin_error("Failed to open pipe for command.");

				string_t output;
				char buffer[256];
				while (fgets(buffer, sizeof(buffer), pipe))
					output += buffer;

				pclose(pipe);
				
				return make_ref<variable>(output, variable_type::vt_string);
			}
		}

		static void load_all(symbol_map& table)
		{
			using namespace wio::builtin::detail;

			loader::load_function(table, "ClearScreen", detail::b_clear_screen, pa<0>{});
			loader::load_function(table, "GetOS", detail::b_get_os, pa<0>{});
			loader::load_function(table, "RunCommand", detail::b_run_command, pa<1>{ variable_base_type::variable });

			loader::load_constant(table, "OS_Windows", variable_type::vt_integer, static_cast<integer_t>(OS::Windows));
			loader::load_constant(table, "OS_Linux", variable_type::vt_integer, static_cast<integer_t>(OS::Linux));
			loader::load_constant(table, "OS_MacOS", variable_type::vt_integer, static_cast<integer_t>(OS::MacOS));
		}

		void system::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = main_table::get().get_builtin_scope();

			load_all(target_scope->get_symbols());
		}

		void system::load_table(ref<symbol_table> target_table)
		{
			if (target_table)
				load_all(target_table->get_symbols());
		}

		void system::load_symbol_map(symbol_map& target_map)
		{
			load_all(target_map);
		}
	}
}