#include "b_float.h"
#include "../builtin_base.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<variable> get_float(ref<variable_base> base)
            {
                if (!base)
                    throw exception("Variable is null!");

                ref<variable> var = std::dynamic_pointer_cast<variable>(base);

                if (!var)
                    throw type_mismatch_error("Mismatch parameter type!");
                return var;
            }

            static ref<variable_base> b_float_int(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> character = get_float(args[0]);

                long long result = 0;

                if (character->get_type() == variable_type::vt_character)
                    result = static_cast<long long>(character->get_data_as<double>());
                else if (character->get_type() == variable_type::vt_character_ref)
                    result = static_cast<long long>(*character->get_data_as<double*>());

                return make_ref<variable>(any(result), variable_type::vt_integer);
            }

        }

        ref<symbol_table> b_float::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "Int", detail::b_float_int, pa<1>{ variable_type::vt_any });

            return result;
        }
    }
}