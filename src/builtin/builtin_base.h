#pragma once

#include "../interpreter/scope.h"
#include "../interpreter/main_table.h"
#include "../variables/function.h"
#include "../variables/overload_list.h"

#include <bitset>
#include <array>

namespace wio
{
	namespace builtin
	{
        namespace detail
        {
            template <size_t Size>
            using pa = std::array<variable_type, Size>;
        }
		namespace loader
		{
            template<size_t ArgCount, typename Func>
            ref<overload_list> load_function(symbol_map& target_table, const std::string& name, Func func, const std::array<variable_type, ArgCount>& param_types, const std::bitset<ArgCount>& is_ref = {})
            {
                std::vector<function_param> params;
                for (size_t i = 0; i < param_types.size(); ++i)
                    params.emplace_back("", param_types[i], is_ref.test(i));

                var_function varFunc([=](const std::vector<function_param>& real_parameters, std::vector<ref<variable_base>>& parameters) -> ref<variable_base>
                    {
                        return func(parameters);
                    }, params);

                ref<overload_list> olist = make_ref<overload_list>();
                olist->set_symbol_id(name);
                olist->add(symbol(make_ref<var_function>(varFunc), false, true));

                target_table[name] = symbol(olist, false, true);

                return olist;
            }

            template<int ArgCount, typename Func>
            void load_overload(ref<overload_list> olist, Func func, const std::array<variable_type, ArgCount>& param_types, const std::bitset<ArgCount>& is_ref = {})
            {
                std::vector<function_param> params;
                for (size_t i = 0; i < param_types.size(); ++i)
                    params.emplace_back("", param_types[i], is_ref.test(i));

                var_function varFunc([=](const std::vector<function_param>& real_parameters, std::vector<ref<variable_base>>& parameters) -> ref<variable_base>
                    {
                        return func(parameters);
                    }, params);

                olist->add(symbol(make_ref<var_function>(varFunc), false, true));
            }

            template <class T>
            void load_constant(symbol_map& target_table, const std::string& name, variable_type type, T value)
            {
                target_table[name] = symbol(make_ref<variable>(any(value), type, packed_bool{ true, false }), false, true);
            }

            template <class T>
            void load_variable(symbol_map& target_table, const std::string& name, ref<variable_base> var)
            {
                target_table[name] = symbol(var, false, true);
            }
		}
	}
}