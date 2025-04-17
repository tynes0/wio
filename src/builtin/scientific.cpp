#include "scientific.h"

#include "builtin_base.h"
#include "helpers.h"

namespace wio
{
	namespace builtin
	{
        static void load_all(symbol_map& table)
        {
            loader::load_constant(table, "GRAVITY", variable_type::vt_float, 6.67430e-11);
            loader::load_constant(table, "STANDARD_GRAVITY", variable_type::vt_float, 9.80665);
            loader::load_constant(table, "SPEED_OF_SOUND", variable_type::vt_float, 340.29);
            loader::load_constant(table, "LIGHT_SPEED", variable_type::vt_float, 299792458.0);
            loader::load_constant(table, "PHI", variable_type::vt_float, 1.61803398874989484820);
            loader::load_constant(table, "GOLDEN_RATIO", variable_type::vt_float, 1.61803398874989484820);
            loader::load_constant(table, "GAMMA", variable_type::vt_float, 0.57721566490153286060);
            loader::load_constant(table, "FEIGENBAUM_DELTA", variable_type::vt_float, 4.66920160910299067185);
            loader::load_constant(table, "FEIGENBAUM_ALPHA", variable_type::vt_float, 2.50290787509589282228);
            loader::load_constant(table, "CATALAN", variable_type::vt_float, 0.91596559417721901505);
            loader::load_constant(table, "APERYS_CONSTANT", variable_type::vt_float, 1.20205690315959428540);
            loader::load_constant(table, "PLANCK", variable_type::vt_float, 6.62607015e-34);
            loader::load_constant(table, "BOLTZMANN", variable_type::vt_float, 1.380649e-23);
            loader::load_constant(table, "AVOGADRO", variable_type::vt_float, 6.02214076e23);
            loader::load_constant(table, "MU0", variable_type::vt_float, 1.25663706212e-6);
            loader::load_constant(table, "EPSILON0", variable_type::vt_float, 8.8541878128e-12);
            loader::load_constant(table, "RYDBERG", variable_type::vt_float, 10973731.568160);
            loader::load_constant(table, "BOHR_RADIUS", variable_type::vt_float, 5.29177210903e-11);
            loader::load_constant(table, "ELECTRON_MASS", variable_type::vt_float, 9.1093837015e-31);
            loader::load_constant(table, "PROTON_MASS", variable_type::vt_float, 1.67262192369e-27);
            loader::load_constant(table, "NEUTRON_MASS", variable_type::vt_float, 1.67492749804e-27);
        }

		void scientific::load(ref<scope> target_scope)
		{
            if (!target_scope)
                target_scope = main_table::get().get_builtin_scope();

            load_all(target_scope->get_symbols());
		}

        void scientific::load_table(ref<symbol_table> target_table)
        {
            if(target_table)
                load_all(target_table->get_symbols());
        }

        void scientific::load_symbol_map(symbol_map& target_map)
        {
            load_all(target_map);
        }
	}
}

