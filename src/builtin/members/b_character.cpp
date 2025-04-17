#include "b_character.h"
#include "../builtin_base.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<variable> get_character(ref<variable_base> base)
            {
                if (!base)
                    throw exception("Variable is null!");

                ref<variable> var = std::dynamic_pointer_cast<variable>(base);

                if (!var)
                    throw exception("Variable is null!");
                return var;
            }

            static ref<variable_base> b_character_int(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> character = get_character(args[0]);

                long long result = 0;

                if (character->get_type() == variable_type::vt_character)
                    result = static_cast<long long>(character->get_data_as<char>());
                else if (character->get_type() == variable_type::vt_character_ref)
                    result = static_cast<long long>(*character->get_data_as<char*>());

                return make_ref<variable>(any(result), variable_type::vt_integer);
            }

            static ref<variable_base> b_character_string(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> character = get_character(args[0]);

                std::string result;

                if (character->get_type() == variable_type::vt_character)
                    result = character->get_data_as<char>();
                else if (character->get_type() == variable_type::vt_character_ref)
                    result = *character->get_data_as<char*>();

                return make_ref<variable>(any(result), variable_type::vt_string);
            }
        }

        ref<symbol_table> b_character::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "Int", detail::b_character_int, pa<1>{ variable_type::vt_any });
            loader::load_function(table, "String", detail::b_character_string, pa<1>{ variable_type::vt_any });

            return result;
        }
    }
}