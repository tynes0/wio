#include "interpreter.h"

#include <functional>
#include <array>
#include <vector>
#include <filesystem>

#include "utils/filesystem.h"
#include "interpreter/exception.h"
#include "interpreter/lexer.h"
#include "interpreter/parser.h"
#include "interpreter/evaluator.h"
#include "interpreter/module_tracker.h"

namespace wio
{
	namespace detail
	{
		struct arg_data
		{
			bool show_tokens = false;
			bool show_parse_tree = false;
			bool run = true;
		};

		static arg_data s_arg_data;
		static std::filesystem::path base_path;
		static symbol_table_t temp_table;
		using arg_pr = std::pair<const char*, void(*)()>;

		static void show_tokens()
		{
			s_arg_data.show_tokens = true;
		}

		static void show_parse_tree()
		{
			s_arg_data.show_parse_tree = true;
		}

		static void no_run()
		{
			s_arg_data.run = false;
		}


		static constexpr std::array<arg_pr, 8> s_argument_array
		{
			arg_pr{ "-T", show_tokens },
			arg_pr{ "-P", show_parse_tree },
			arg_pr{ "-NR", no_run },
			arg_pr{ "", nullptr },
			arg_pr{ "", nullptr },
			arg_pr{ "", nullptr },
			arg_pr{ "", nullptr },
			arg_pr{ "", nullptr },
		};

		static bool is_starts_with(const std::string& str, char ch)
		{
			if (str.empty())
				return false;
			return str[0] == ch;
		}

		static void to_upper(std::string& str)
		{
			for (char& ch : str)
				if (islower(ch))
					ch = toupper(ch);
		}

		static arg_pr find_arg(const std::string& arg)
		{
			for (auto& item : s_argument_array)
				if (arg == item.first)
					return item;
			return arg_pr{ "", nullptr };
		}
	}

	struct app_data
	{
		std::vector<std::string> args;
		std::vector<std::string> wio_args;
	};

	static app_data s_app_data;

	interpreter::interpreter() {}

	void interpreter::load_args(int argc, const char** argv)
	{
		if (argc <= 1)
			return;
		s_app_data.args.reserve(argc - 1);
		for (int i = 1; i < argc; ++i)
			s_app_data.args.emplace_back(argv[i]);
	}

	void interpreter::run()
	{
		try
		{
			bool file_passed = false;
			raw_buffer content;

			if (s_app_data.args.empty())
				throw file_error("No argument provided!");

			for (int i = 0; i < (int)s_app_data.args.size(); ++i)
			{
				if (file_passed)
				{
					s_app_data.wio_args.push_back(s_app_data.args[i]);
				}
				else if (detail::is_starts_with(s_app_data.args[i], '-'))
				{
					detail::to_upper(s_app_data.args[i]);
					auto arg_pair = detail::find_arg(s_app_data.args[i]);
					if (arg_pair.second == nullptr)
						throw undefined_argument_error("Undefined argument! (" + s_app_data.args[i] + ")");
					(arg_pair.second)();
				}
				else
				{
					content = run_f_p1(s_app_data.args[i].c_str());
					file_passed = true;
				}
			}

			if (!file_passed)
				throw file_error("No wio file provided!");

			run_f_p2(content, false);
			// TODO
		}
		catch (const exception& e)
		{
			filesystem::write_file(stderr, e.what()); // TODO
			exit(EXIT_FAILURE);
		}
	}

	interpreter& interpreter::get()
	{
		static interpreter main_interpreter;
		return main_interpreter;
	}

	raw_buffer interpreter::run_f(const char* fp, bool sf)
	{
		raw_buffer content = run_f_p1(fp);
		raw_buffer result = run_f_p2(content, sf);
		run_f_p3();
		return result;
	}

	raw_buffer interpreter::run_f_p1(const char* fp)
	{
		raw_buffer content;

		std::string filepath = fp;
		if (!filesystem::check_extension(filepath, ".wio"))
		{
			if (std::filesystem::path(filepath).has_extension())
				throw file_error("\'" + filepath + "\' is not a wio file!");

			if (filesystem::has_prefix(filepath, "wio."))
			{
				// BUILTINS import "wio	"
			}
			else
			{
				filepath += ".wio";
			}
		}

		if (!filesystem::file_exists(filepath))
			throw file_error("\'" + filepath + "\' not exists!");

		if (!module_tracker::get().add_module(filepath))
			return raw_buffer{ nullptr };
		
		content = filesystem::read_file(filepath);
		detail::base_path = std::filesystem::current_path();

		std::filesystem::path abs_path = filesystem::get_abs_path(filepath);
		std::filesystem::path rd = abs_path.parent_path();
		std::filesystem::current_path(rd);

		return content;
	}

	raw_buffer interpreter::run_f_p2(raw_buffer content, bool sf)
	{
		if (!content)
			return raw_buffer(nullptr);

		lexer l_lexer(content.as<const char>());
		auto& tokens = l_lexer.get_tokens();

		if (detail::s_arg_data.show_tokens)
			filesystem::write_stdout(l_lexer.to_string());

		parser l_parser(tokens);
		ref<program> tree = l_parser.parse();

		if (detail::s_arg_data.show_parse_tree)
			filesystem::write_stdout(tree->to_string());

		evaluator eval;
		eval.evaluate_program(tree);

		detail::temp_table = eval.get_symbols();

		raw_buffer result;
		result.store(detail::temp_table);

		return result;
	}

	void interpreter::run_f_p3()
	{
		std::filesystem::current_path(detail::base_path);
	}
}