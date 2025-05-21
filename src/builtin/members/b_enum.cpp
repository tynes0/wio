#include "b_enum.h"
#include "../builtin_base.h"
#include "../../variables/enum.h"

namespace wio
{
    namespace builtin
    {
        namespace detail
        {
            static ref<var_enum> get_enum(ref<variable_base> base)
            {
                if (!base)
                    throw exception("Variable is null!");

                ref<var_enum> var = std::dynamic_pointer_cast<var_enum>(base);

                if (!var)
                    throw type_mismatch_error("Mismatch parameter type!");
                return var;
            }

            static ref<variable_base> b_index(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> param = std::dynamic_pointer_cast<variable>(args[1]);

                if (!param)
                    throw type_mismatch_error("Mismatch parameter type!");

                if(param->get_type() == variable_type::vt_integer) // value
                    return make_ref<variable>(any(get_enum(args[0])->index_of(param->get_data_as<integer_t>())), variable_type::vt_integer);
                else if (param->get_type() == variable_type::vt_string) // item
                    return make_ref<variable>(any(get_enum(args[0])->index_of(param->get_data_as<string_t>())), variable_type::vt_integer);

                throw type_mismatch_error("Mismatch parameter type!");
                return create_null_variable();
            }

            static ref<variable_base> b_value(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> param = std::dynamic_pointer_cast<variable>(args[1]);

                if (!param)
                    throw type_mismatch_error("Mismatch parameter type!");

                if (param->get_type() == variable_type::vt_string) // item
                    return make_ref<variable>(any(get_enum(args[0])->value_of(param->get_data_as<string_t>())), variable_type::vt_integer);

                throw type_mismatch_error("Mismatch parameter type!");
                return create_null_variable();
            }

            static ref<variable_base> b_to_string(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> param = std::dynamic_pointer_cast<variable>(args[1]);

                if (!param)
                    throw type_mismatch_error("Mismatch parameter type!");

                if (param->get_type() == variable_type::vt_integer) // value
                    return make_ref<variable>(any(get_enum(args[0])->to_string(param->get_data_as<integer_t>())), variable_type::vt_string);

                throw type_mismatch_error("Mismatch parameter type!");
                return create_null_variable();
            }

            static ref<variable_base> b_index_to_string(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> param = std::dynamic_pointer_cast<variable>(args[1]);

                if (!param)
                    throw type_mismatch_error("Mismatch parameter type!");

                if (param->get_type() == variable_type::vt_integer) // index
                    return make_ref<variable>(any(get_enum(args[0])->to_string_n(param->get_data_as<integer_t>())), variable_type::vt_string);

                throw type_mismatch_error("Mismatch parameter type!");
                return create_null_variable();
            }

            static ref<variable_base> b_contains(const std::vector<ref<variable_base>>& args)
            {
                ref<variable> param = std::dynamic_pointer_cast<variable>(args[1]);

                if (!param)
                    throw type_mismatch_error("Mismatch parameter type!");

                if (param->get_type() == variable_type::vt_integer) // value
                    return make_ref<variable>(any(get_enum(args[0])->contains(param->get_data_as<integer_t>())), variable_type::vt_bool);
                else if (param->get_type() == variable_type::vt_string) // item
                    return make_ref<variable>(any(get_enum(args[0])->contains(param->get_data_as<string_t>())), variable_type::vt_bool);

                throw type_mismatch_error("Mismatch parameter type!");
                return create_null_variable();
            }

            static ref<variable_base> b_size(const std::vector<ref<variable_base>>& args)
            {
                return make_ref<variable>(any(get_enum(args[0])->size()), variable_type::vt_integer);
            }
        }

        ref<symbol_table> b_enum::load()
        {
            using namespace wio::builtin::detail;

            auto result = make_ref<symbol_table>();
            symbol_map& table = result->get_symbols();

            loader::load_function(table, "Index", detail::b_index, pa<2>{ variable_base_type::enumeration, variable_base_type::variable });
            loader::load_function(table, "Value", detail::b_value, pa<2>{ variable_base_type::enumeration, variable_base_type::variable });
            loader::load_function(table, "ToString", detail::b_to_string, pa<2>{ variable_base_type::enumeration, variable_base_type::variable });
            loader::load_function(table, "IndexToString", detail::b_index_to_string, pa<2>{ variable_base_type::enumeration, variable_base_type::variable });
            loader::load_function(table, "Contains", detail::b_contains, pa<2>{ variable_base_type::enumeration, variable_base_type::variable });
            loader::load_function(table, "Size", detail::b_size, pa<1>{ variable_base_type::enumeration });

            return result;
        }
    }
}