#include "types.h"

#include "builtin_base.h"
#include "../variables/variable_type.h"
#include "../variables/realm.h"

namespace wio
{
	namespace builtin
	{
		static void load_all(symbol_map& table)
		{
			loader::load_constant(table, "Null", variable_type::vt_type, variable_type::vt_null);
			loader::load_constant(table, "Integer", variable_type::vt_type, variable_type::vt_integer);
			loader::load_constant(table, "Float", variable_type::vt_type, variable_type::vt_float);
			loader::load_constant(table, "String", variable_type::vt_type, variable_type::vt_string);
			loader::load_constant(table, "Character", variable_type::vt_type, variable_type::vt_character);
			loader::load_constant(table, "Boolean", variable_type::vt_type, variable_type::vt_bool);
			loader::load_constant(table, "Type", variable_type::vt_type, variable_type::vt_type);
			loader::load_constant(table, "Pair", variable_type::vt_type, variable_type::vt_pair);
			loader::load_constant(table, "File", variable_type::vt_type, variable_type::vt_file);
			loader::load_constant(table, "Array", variable_type::vt_type, variable_type::vt_array);
			loader::load_constant(table, "Dictionary", variable_type::vt_type, variable_type::vt_dictionary);
			loader::load_constant(table, "Function", variable_type::vt_type, variable_type::vt_function);
			loader::load_constant(table, "Vec2", variable_type::vt_type, variable_type::vt_vec2);
			loader::load_constant(table, "Vec3", variable_type::vt_type, variable_type::vt_vec3);
			loader::load_constant(table, "Vec4", variable_type::vt_type, variable_type::vt_vec4);
			loader::load_constant(table, "Comparator", variable_type::vt_type, variable_type::vt_comparator);
		}

		void types::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = main_table::get().get_builtin_scope();

			ref<realm> type_realm = make_ref<realm>();
			type_realm->init_members();
			load_all(type_realm->get_members()->get_symbols());

			loader::load_variable(target_scope->get_symbols(), "Type", type_realm);
		}

		void types::load_table(ref<symbol_table> target_table)
		{
			if (target_table)
				load_all(target_table->get_symbols());
		}

		void types::load_symbol_map(symbol_map& target_map)
		{
			ref<realm> type_realm = make_ref<realm>();
			type_realm->init_members();
			load_all(type_realm->get_members()->get_symbols());

			loader::load_variable(target_map, "Type", type_realm);
		}
	}
}

