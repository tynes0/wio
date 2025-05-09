#include "b_dictionary.h"

#include "../builtin_base.h"
#include "../../variables/dictionary.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<var_dictionary> get_dict(ref<variable_base> base_dictionary)
            {
                if (!base_dictionary)
                    throw exception("Dictionary is null!");
                ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(base_dictionary);
                if (!dict)
                    throw type_mismatch_error("Mismatch parameter type!");

                return dict;
            }

            static ref<variable_base> b_dict_exists(std::vector<ref<variable_base>>& args)
            {
                ref<var_dictionary> dict = get_dict(args[0]);

                string_t key = var_dictionary::as_key(args[1]);

                return make_ref<variable>(any(dict->check_existance(key)), variable_type::vt_bool);
            }

            static ref<variable_base> b_dict_size(std::vector<ref<variable_base>>& args)
            {
                ref<var_dictionary> dict = get_dict(args[0]);

                return make_ref<variable>(any((integer_t)dict->size()), variable_type::vt_integer);
            }
        }

        ref<symbol_table> b_dictionary::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "Exists", detail::b_dict_exists, pa<2>{ variable_base_type::dictionary, variable_base_type::omni });
            loader::load_function(table, "Size", detail::b_dict_size, pa<1>{ variable_base_type::dictionary });

            return result;
        }
    }
}
