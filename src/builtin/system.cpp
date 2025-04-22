#include "system.h"

#include "builtin_base.h"
#include "helpers.h"

#include <sstream>

namespace wio
{
	namespace builtin
	{
		using namespace std::string_literals;

		namespace detail
		{

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
		}

		static void load_all(symbol_map& table)
		{
			using namespace wio::builtin::detail;

			loader::load_function(table, "ClearScreen", detail::b_clear_screen, pa<0>{});
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