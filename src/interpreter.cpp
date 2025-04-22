#include "interpreter.h"

#include <functional>
#include <array>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "base/base.h"
#include "base/exception.h"
#include "base/argument_parser.h"

#include "utils/filesystem.h"

#include "interpreter/lexer.h"
#include "interpreter/parser.h"
#include "interpreter/evaluator.h"
#include "interpreter/module_tracker.h"
#include "interpreter/eval_base.h"
#include "interpreter/main_table.h"

#include "builtin/io.h"
#include "builtin/math.h"
#include "builtin/utility.h"
#include "builtin/types.h"
#include "builtin/scientific.h"
#include "builtin/system.h"

#include "variables/function.h"

namespace wio
{
	struct app_data
	{
		std::filesystem::path base_path;
		argument_parser arg_parser;
		packed_bool flags{}; // 1- single file 2- show tokens 3- show ast 4- no run 5- no built-in
		packed_bool buitin_imports{}; // 1-io 2-math 3-util
		id_t last_module_id = 0;
	};

	static app_data s_app_data;

	interpreter::interpreter() 
	{
		s_app_data.arg_parser.set_program_name("wio");
		s_app_data.arg_parser.enable_required_file(".wio");

		s_app_data.arg_parser.add_argument("help", { "-h", "--help" }, "Shows the help message.", [](const std::string&) {
			filesystem::write_stdout(s_app_data.arg_parser.help());
			}, true);

		s_app_data.arg_parser.add_argument("single-file", { "-sf", "--single-file" }, "imports are ignored (Built-in imports not). It is treated as a single file.", [](const std::string&) {
			s_app_data.flags.b1 = true;
			});

		s_app_data.arg_parser.add_argument("show-tokens", { "-st", "--show-tokens" }, "Shows tokens.", [](const std::string&) {
			s_app_data.flags.b2 = true;
			});

		s_app_data.arg_parser.add_argument("show-ast", { "-sa", "-ast", "--show-ast"}, "Shows ast.", [](const std::string&) {
			s_app_data.flags.b3 = true;
			});

		s_app_data.arg_parser.add_argument("no-run", { "-nr", "--no-run" }, "It just creates the ast without running the program.", [](const std::string&) {
			s_app_data.flags.b4 = true;
			});

		s_app_data.arg_parser.add_argument("no-builtin", { "-nb", "--no-builtin" }, "Wio built-in library imports are ignored.", [](const std::string&) {
			s_app_data.flags.b5 = true;
			});
	}

	void interpreter::load_args(int argc, char* argv[])
	{
		try
		{
			s_app_data.arg_parser.parse(argc, argv);
			if (s_app_data.arg_parser.help_called())
				exit(EXIT_SUCCESS);
		}
		catch (const exception& e)
		{
			filesystem::write_file(stderr, e.what());
			exit(EXIT_FAILURE);
		}
	}

	void interpreter::run()
	{
		try
		{
			packed_bool flags = { s_app_data.flags.b1, false, false, s_app_data.flags.b5 };
			eval_base::get().initialize(flags);
			raw_buffer buf = run_f_p1(s_app_data.arg_parser.get_file().c_str(), flags);
			main_table::get().add_imported_module(s_app_data.last_module_id);
			run_f_p2(buf, flags);
		}
		catch (const exception& e)
		{
			filesystem::write_file(stderr, e.what());
			exit(EXIT_FAILURE);
		}
	}

	interpreter& interpreter::get()
	{
		static interpreter main_interpreter;
		return main_interpreter;
	}

	id_t interpreter::run_f(const char* fp, packed_bool flags, symbol_map* target_map)
	{
		raw_buffer content = run_f_p1(fp, flags, target_map);
		if (!content)
			return 0;

		run_f_p2(content, flags, target_map);
		run_f_p3();

		return s_app_data.last_module_id;
	}

	raw_buffer interpreter::run_f_p1(const char* fp, packed_bool flags, symbol_map* target_map)
	{
		std::string filepath = fp;
		std::for_each(filepath.begin(), filepath.end(), [](char& ch) { ch = std::tolower(ch); });

		if (!filesystem::check_extension(filepath, ".wio"))
		{
			if (filesystem::has_prefix(filepath, "wio."))
			{
				if (flags.b4)
					return raw_buffer(nullptr);

				std::string_view view(filepath);
				view.remove_prefix(4);
				
				if (view == "io")
				{
					if (module_tracker::get().add_module(filepath))
					{
						if(target_map)
							builtin::io::load_symbol_map(*target_map);
						else
							builtin::io::load();
					}
				}
				else if (view == "math")
				{
					if (module_tracker::get().add_module(filepath))
					{
						if (target_map)
							builtin::math::load_symbol_map(*target_map);
						else
							builtin::math::load();
					}
				}
				else if (view == "util")
				{
					if (module_tracker::get().add_module(filepath))
					{
						if (target_map)
							builtin::utility::load_symbol_map(*target_map);
						else
							builtin::utility::load();
					}
				}
				else if (view == "types")
				{
					if (module_tracker::get().add_module(filepath))
					{
						if (target_map)
							builtin::types::load_symbol_map(*target_map);
						else
							builtin::types::load();
					}
				}
				else if (view == "scientific")
				{
					if (module_tracker::get().add_module(filepath))
					{
						if (target_map)
							builtin::scientific::load_symbol_map(*target_map);
						else
							builtin::scientific::load();
					}
				}
				else if (view == "system")
				{
					if (module_tracker::get().add_module(filepath))
					{
						if (target_map)
							builtin::system::load_symbol_map(*target_map);
						else
							builtin::system::load();
					}
				}
				else
				{
					throw invalid_import_error("wio does not have a library called '" + std::string(view) + "'.");
				}

				return raw_buffer(nullptr);
			}
			else
			{
				if (flags.b1)
					return raw_buffer(nullptr);
				filepath += ".wio";
			}
		}

		if (!filesystem::file_exists(filepath))
			throw file_error("\'" + filepath + "\' not exists!");

		s_app_data.last_module_id = module_tracker::get().add_module(std::filesystem::path(filepath));

		if(s_app_data.last_module_id == 0)
			return raw_buffer(nullptr);
		
		raw_buffer content = filesystem::read_file(filepath);
		s_app_data.base_path = std::filesystem::current_path();

		std::filesystem::path abs_path = filesystem::get_abs_path(filepath);
		std::filesystem::path rd = abs_path.parent_path();
		std::filesystem::current_path(rd);

		return content;
	}

	void interpreter::run_f_p2(raw_buffer content, packed_bool flags, symbol_map* target_map)
	{
		if (!content)
			return;

		lexer l_lexer(content.as<const char>());
		auto& tokens = l_lexer.get_tokens();

		if (s_app_data.flags.b2)
			filesystem::write_stdout(l_lexer.to_string());

		parser l_parser(tokens);
		ref<program> tree = l_parser.parse();

		if (s_app_data.flags.b3)
			filesystem::write_stdout(tree->to_string());

		if (s_app_data.flags.b4)
			return;

		evaluator::evaluate_program(tree, s_app_data.last_module_id, flags);

		if(target_map)
			(*target_map) = main_table::get().get_scope(s_app_data.last_module_id)->get_symbols();
	}

	void interpreter::run_f_p3()
	{
		std::filesystem::current_path(s_app_data.base_path);
	}
}