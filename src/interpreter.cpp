#include "interpreter.h"

#include <functional>
#include <array>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "base/exception.h"
#include "base/argument_parser.h"

#include "utils/filesystem.h"

#include "interpreter/lexer.h"
#include "interpreter/parser.h"
#include "interpreter/evaluator.h"
#include "interpreter/module_tracker.h"

#include "builtin/io.h"
#include "builtin/math.h"
#include "builtin/utility.h"
#include "builtin/types.h"

#include "variables/function.h"

namespace wio
{
	struct app_data
	{
		std::filesystem::path base_path;
		symbol_table_t temp_sym_table;
		argument_parser arg_parser;
		packed_bool flags{}; // 1- single file 2- show tokens 3- show ast 4- no run 5- no built-in
		packed_bool buitin_imports{}; // 1-io 2-math 3-util
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
			raw_buffer buf = run_f_p1(s_app_data.arg_parser.get_file().c_str(), flags);
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

	raw_buffer interpreter::run_f(const char* fp, packed_bool flags, ref<scope> target_scope)
	{
		raw_buffer content = run_f_p1(fp, flags, target_scope);
		if (!content)
			return raw_buffer(nullptr);
		raw_buffer result = run_f_p2(content, flags);
		run_f_p3();
		return result;
	}

	raw_buffer interpreter::run_f_p1(const char* fp, packed_bool flags, ref<scope> target_scope)
	{
		std::string filepath = fp;
		if (!filesystem::check_extension(filepath, ".wio"))
		{
			if (filesystem::has_prefix(filepath, "wio."))
			{
				if (flags.b4)
					return raw_buffer(nullptr);

				std::for_each(filepath.begin(), filepath.end(), [](char& ch) { ch = std::tolower(ch); });

				std::string_view view(filepath);
				view.remove_prefix(4);
				
				if (view == "io")
				{
					if (module_tracker::get().add_module(std::filesystem::path(filepath)))
						builtin::io::load(target_scope);
				}
				else if (view == "math")
				{
					if (module_tracker::get().add_module(std::filesystem::path(filepath)))
						builtin::math::load(target_scope);

				}
				else if (view == "util")
				{
					if (module_tracker::get().add_module(std::filesystem::path(filepath)))
						builtin::utility::load(target_scope);
				}
				else if (view == "types")
				{
					if (module_tracker::get().add_module(std::filesystem::path(filepath)))
						builtin::types::load(target_scope);
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

		if (!module_tracker::get().add_module(std::filesystem::path(filepath)))
			return raw_buffer(nullptr);
		
		raw_buffer content = filesystem::read_file(filepath);
		s_app_data.base_path = std::filesystem::current_path();

		std::filesystem::path abs_path = filesystem::get_abs_path(filepath);
		std::filesystem::path rd = abs_path.parent_path();
		std::filesystem::current_path(rd);

		return content;
	}

	raw_buffer interpreter::run_f_p2(raw_buffer content, packed_bool flags)
	{
		if (!content)
			return raw_buffer(nullptr);

		lexer l_lexer(content.as<const char>());
		auto& tokens = l_lexer.get_tokens();

		if (s_app_data.flags.b2)
			filesystem::write_stdout(l_lexer.to_string());

		parser l_parser(tokens);
		ref<program> tree = l_parser.parse();

		if (s_app_data.flags.b3)
			filesystem::write_stdout(tree->to_string());

		if (s_app_data.flags.b4)
			return raw_buffer(nullptr);

		evaluator eval(flags);
		eval.evaluate_program(tree);

		s_app_data.temp_sym_table = eval.get_symbols();

		raw_buffer result;
		result.store(s_app_data.temp_sym_table);

		return result;
	}

	void interpreter::run_f_p3()
	{
		std::filesystem::current_path(s_app_data.base_path);
	}
}