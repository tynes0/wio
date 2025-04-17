#include "types.h"

#include "builtin_base.h"
#include "../variables/variable_type.h"

namespace wio
{
	namespace builtin
	{
		static void load_all(symbol_map& table)
		{
			loader::load_constant(table, "NULL", variable_type::vt_type, variable_type::vt_null);
			loader::load_constant(table, "INTEGER", variable_type::vt_type, variable_type::vt_integer);
			loader::load_constant(table, "FLOAT", variable_type::vt_type, variable_type::vt_float);
			loader::load_constant(table, "STRING", variable_type::vt_type, variable_type::vt_string);
			loader::load_constant(table, "CHARACTER", variable_type::vt_type, variable_type::vt_character);
			loader::load_constant(table, "BOOLEAN", variable_type::vt_type, variable_type::vt_bool);
			loader::load_constant(table, "TYPE", variable_type::vt_type, variable_type::vt_type);
			loader::load_constant(table, "PAIR", variable_type::vt_type, variable_type::vt_pair);
			loader::load_constant(table, "FILE", variable_type::vt_type, variable_type::vt_file);
			loader::load_constant(table, "ARRAY", variable_type::vt_type, variable_type::vt_array);
			loader::load_constant(table, "DICTIONARY", variable_type::vt_type, variable_type::vt_dictionary);
			loader::load_constant(table, "FUNCTION", variable_type::vt_type, variable_type::vt_function);
			loader::load_constant(table, "VEC2", variable_type::vt_type, variable_type::vt_vec2);
			loader::load_constant(table, "VEC3", variable_type::vt_type, variable_type::vt_vec3);
			loader::load_constant(table, "VEC4", variable_type::vt_type, variable_type::vt_vec4);
			loader::load_constant(table, "COMPARATOR", variable_type::vt_type, variable_type::vt_comparator);
		}

		void types::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = main_table::get().get_builtin_scope();

			load_all(target_scope->get_symbols());
		}

		void types::load_table(ref<symbol_table> target_table)
		{
			if (target_table)
				load_all(target_table->get_symbols());
		}

		void types::load_symbol_map(symbol_map& target_map)
		{
			load_all(target_map);
		}
	}
}

