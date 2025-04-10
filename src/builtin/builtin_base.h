#pragma once

#include "../interpreter/scope.h"
#include "../variables/function.h"
#include <bitset>

namespace wio
{
	namespace builtin
	{
		namespace loader
		{
            template<int ArgCount, typename Func>
            ref<var_function> load_function(ref<scope> target_scope, const std::string& name, Func func, const std::vector<variable_type>& param_types, const std::bitset<ArgCount>& is_ref = {})
            {
                std::vector<function_param> params;
                for (size_t i = 0; i < param_types.size(); ++i)
                    params.emplace_back("", param_types[i], is_ref.test(i));

                var_function varFunc([=](const std::vector<function_param>& real_parameters, std::vector<ref<variable_base>>& parameters) -> ref<variable_base>
                    {
                        if constexpr (ArgCount == 0)
                            return func();
                        else if constexpr (ArgCount == 1)
                            return func(parameters[0]);
                        else if constexpr (ArgCount == 2)
                            return func(parameters[0], parameters[1]);
                        else if constexpr (ArgCount == 3)
                            return func(parameters[0], parameters[1], parameters[2]);
                        else if constexpr (ArgCount == 4)
                            return func(parameters[0], parameters[1], parameters[2], parameters[3]);
                        else // We can add more, why i did it like that btw (it's sucks)
                            throw builtin_error("Unsupported number of parameters in load_function.");

                    }, params, false);

                ref<var_function> fun = make_ref<var_function>(varFunc);

                symbol sym(fun, { false, true });
                target_scope->insert(name, sym);
                return fun;
            }

            template<int ArgCount, typename Func>
            ref<var_function> load_function(const std::string& name, Func func, const std::vector<variable_type>& param_types, const std::bitset<ArgCount>& is_ref = {})
            {
                return load_function(builtin_scope, name, func, param_types, is_ref);
            }

            template<int ArgCount, typename Func>
            void load_overload(ref<var_function> fun, Func func, const std::vector<variable_type>& param_types, const std::bitset<ArgCount>& is_ref = {})
            {
                std::vector<function_param> params;
                for (size_t i = 0; i < param_types.size(); ++i)
                    params.emplace_back("", param_types[i], is_ref.test(i));

                var_function varFunc([=](const std::vector<function_param>& real_parameters, std::vector<ref<variable_base>>& parameters) -> ref<variable_base>
                    {
                        if constexpr (ArgCount == 0)
                            return func();
                        else if constexpr (ArgCount == 1)
                            return func(parameters[0]);
                        else if constexpr (ArgCount == 2)
                            return func(parameters[0], parameters[1]);
                        else if constexpr (ArgCount == 3)
                            return func(parameters[0], parameters[1], parameters[2]);
                        else if constexpr (ArgCount == 4)
                            return func(parameters[0], parameters[1], parameters[2], parameters[3]);
                        else // We can add more 
                            throw builtin_error("Unsupported number of parameters in load_function.");

                    }, params, false);

                fun->add_overload(symbol(make_ref<var_function>(varFunc)));
            }

            template <class T>
            void load_constant(ref<scope> target_scope, const std::string& name, variable_type type, T value)
            {
                target_scope->insert(name, symbol(make_ref<variable>(any(value), type, packed_bool{ true, false }), {false, true}));
            }

            template <class T>
            void load_constant(const std::string& name, variable_type type, T value)
            {
                builtin_scope->insert(name, symbol(make_ref<variable>(any(value), type, packed_bool{ true, false }), { false, true }));
            }

            template <class T>
            void load_variable(ref<scope> target_scope, const std::string& name, ref<variable_base> var)
            {
                target_scope->insert(name, symbol(var), {false, true});
            }

            template <class T>
            void load_variable(const std::string& name, ref<variable_base> var)
            {
                builtin_scope->insert(name, symbol(var), { false, true });
            }
		}
	}
}