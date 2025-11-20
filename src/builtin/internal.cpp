#include "internal.h"

#include "../interpreter/main_table.h"

#include "../types/file_wrapper.h"
#include "../utils/filesystem.h"
#include "../utils/util.h"
#include "../variables/array.h"
#include "../variables/variable.h"

#include "builtin_base.h"
#include "helpers.h"

namespace wio
{
	namespace builtin
	{
		namespace detail
		{
			static ref<variable_base> b_print(const std::vector<ref<variable_base>>& args)
			{
				filesystem::write_stdout(util::var_to_string(args[0]));
				return create_null_variable();
			}

			static ref<variable_base> b_println(const std::vector<ref<variable_base>>& args)
			{
				b_print(args);
				filesystem::write_stdout("\n");
				return create_null_variable();
			}

			static ref<variable_base> b_println_2(const std::vector<ref<variable_base>>& args)
			{
				filesystem::write_stdout("\n");
				return create_null_variable();
			}

			static ref<variable_base> b_input(const std::vector<ref<variable_base>>& args)
			{
				raw_buffer buf = filesystem::read_stdout();
				return make_ref<variable>(any(string_t(buf.as<character_t>())), variable_type::vt_string);
			}

			static ref<variable_base> b_input_2(const std::vector<ref<variable_base>>& args)
			{
				filesystem::write_stdout(util::var_to_string(args[0]));
				raw_buffer buf = filesystem::read_stdout();
				return make_ref<variable>(any(string_t(buf.as<character_t>())), variable_type::vt_string);
			}

			static ref<variable_base> b_range(const std::vector<ref<variable_base>>& args)
			{
				if (args[0]->get_base_type() != variable_base_type::variable || args[1]->get_base_type() != variable_base_type::variable)
					return create_null_variable();

				any left_value = std::dynamic_pointer_cast<variable>(args[0])->get_data();
				any right_value = std::dynamic_pointer_cast<variable>(args[1])->get_data();

				if (args[0]->get_type() == variable_type::vt_integer)
				{
					integer_t ll_left = any_cast<integer_t>(left_value);
					integer_t ll_right = 0;

					if (args[1]->get_type() == variable_type::vt_integer)
						ll_right = any_cast<integer_t>(right_value);
					else if (args[1]->get_type() == variable_type::vt_float)
						ll_right = static_cast<integer_t>(any_cast<float_t>(right_value));
					else if (args[1]->get_type() == variable_type::vt_float_ref)
						ll_right = static_cast<integer_t>(*any_cast<float_ref_t>(right_value));

					std::vector<ref<variable_base>> result_vec;

					if (ll_left < ll_right)
					{
						for (integer_t i = ll_left; i < ll_right; ++i)
							result_vec.push_back(make_ref<variable>(any(i), variable_type::vt_integer));
					}
					else
					{
						for (integer_t i = ll_left - 1; i > ll_right - 1; --i)
							result_vec.push_back(make_ref<variable>(any(i), variable_type::vt_integer));
					}

					return make_ref<var_array>(result_vec);
				}
				else if (args[0]->get_type() == variable_type::vt_float)
				{
					float_t d_left = any_cast<float_t>(left_value);
					float_t d_right = 0.0;

					if (args[1]->get_type() == variable_type::vt_float)
						d_right = any_cast<float_t>(right_value);
					else if (args[1]->get_type() == variable_type::vt_float_ref)
						d_right = *any_cast<float_ref_t>(right_value);
					else if (args[1]->get_type() == variable_type::vt_integer)
						d_right = (float_t)any_cast<integer_t>(right_value);

					std::vector<ref<variable_base>> result_vec;

					if (d_left < d_right)
					{
						for (float_t i = d_left; i < d_right; ++i)
							result_vec.push_back(make_ref<variable>(any(i), variable_type::vt_float));
					}
					else
					{
						for (float_t i = d_left - 1; i > d_right - 1; --i)
							result_vec.push_back(make_ref<variable>(any(i), variable_type::vt_float));
					}

					return make_ref<var_array>(result_vec);
				}

				return create_null_variable();
			}

			static ref<variable_base> b_range_2(const std::vector<ref<variable_base>>& args)
			{
				std::vector<ref<variable_base>> new_args;
				new_args.push_back(make_ref<variable>(any(integer_t(0)), variable_type::vt_integer));
				new_args.push_back(args[0]);
				return b_range(new_args);
			}
		}

		static void load_all(symbol_map& table)
		{
			using namespace wio::builtin::detail;

			loader::load_function(table, "Print", detail::b_print, pa<1>{ variable_base_type::omni });

			auto println_func = loader::load_function(table, "Println", detail::b_println, pa<1>{ variable_base_type::omni });
			loader::load_overload(println_func, detail::b_println_2, pa<0>{});

			auto input_func = loader::load_function(table, "Input", detail::b_input, pa<0>{});
			loader::load_overload(input_func, detail::b_input_2, pa<1>{ variable_base_type::omni });

			auto range_func = loader::load_function(table, "Range", detail::b_range, pa<2>{ variable_base_type::omni, variable_base_type::omni });
			loader::load_overload(range_func, detail::b_range_2, pa<1>{ variable_base_type::omni });

		}

		void internal::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = main_table::get().get_builtin_scope();

			load_all(target_scope->get_symbols());
		}

		void internal::load_table(ref<symbol_table> target_table)
		{
			if (target_table)
				load_all(target_table->get_symbols());
		}

		void internal::load_symbol_map(symbol_map& target_map)
		{
			load_all(target_map);
		}

	}
}

