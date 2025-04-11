#include "types.h"

#include "builtin_base.h"
#include "../variables/variable_type.h"

namespace wio
{
	namespace builtin
	{
		void types::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = builtin_scope;

			loader::load_constant(target_scope, "NULL", variable_type::vt_type, variable_type::vt_null);
			loader::load_constant(target_scope, "INTEGER", variable_type::vt_type, variable_type::vt_integer);
			loader::load_constant(target_scope, "FLOAT", variable_type::vt_type, variable_type::vt_float);
			loader::load_constant(target_scope, "STRING", variable_type::vt_type, variable_type::vt_string);
			loader::load_constant(target_scope, "CHARACTER", variable_type::vt_type, variable_type::vt_character);
			loader::load_constant(target_scope, "BOOLEAN", variable_type::vt_type, variable_type::vt_bool);
			loader::load_constant(target_scope, "TYPE", variable_type::vt_type, variable_type::vt_type);
			loader::load_constant(target_scope, "PAIR", variable_type::vt_type, variable_type::vt_pair);
			loader::load_constant(target_scope, "FILE", variable_type::vt_type, variable_type::vt_file);
			loader::load_constant(target_scope, "ARRAY", variable_type::vt_type, variable_type::vt_array);
			loader::load_constant(target_scope, "DICTIONARY", variable_type::vt_type, variable_type::vt_dictionary);
			loader::load_constant(target_scope, "FUNCTION", variable_type::vt_type, variable_type::vt_function);
			loader::load_constant(target_scope, "VEC2", variable_type::vt_type, variable_type::vt_vec2);
			loader::load_constant(target_scope, "VEC3", variable_type::vt_type, variable_type::vt_vec3);
			loader::load_constant(target_scope, "VEC4", variable_type::vt_type, variable_type::vt_vec4);
			loader::load_constant(target_scope, "COMPARATOR", variable_type::vt_type, variable_type::vt_comparator);
		}
	}
}

