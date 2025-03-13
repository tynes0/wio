#include "application.h"

#include <functional>
#include <array>
#include <vector>

#include "exception.h"
#include "utils/filesystem.h"
#include "lexer.h"
#include "parser.h"
#include "evaluator.h"

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

	application::application(int argc, const char** argv)
	{
		if (argc <= 1)
			return;
		s_app_data.args.reserve(argc - 1);
		for (int i = 1; i < argc; ++i)
			s_app_data.args.emplace_back(argv[i]);
	}

	void application::run()
	{
		try
		{
			bool file_passed = false;
			std::string content;

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
					std::string filepath = s_app_data.args[i];
					if (!check_extension(filepath, ".wio"))
					{
						if (std::filesystem::path(filepath).has_extension())
							throw file_error("\'" + filepath + "\' is not a wio file!");
						if (has_prefix(filepath, "wio."))
						{
							// BUILTINS
						}
						else
						{
							filepath += ".wio";
						}
					}
					if (!file_exists(filepath))
					{
						throw file_error("\'" + filepath + "\' not exists!");
					}
					content = read_file(filepath);
					std::filesystem::path cp = std::filesystem::current_path();
					cp /= filepath;
					std::filesystem::path rd = cp.parent_path();
					std::filesystem::current_path(rd);

					file_passed = true;
				}
			}

			if (!file_passed)
				throw file_error("No wio file provided!");


			lexer l_lexer(content);
			auto& tokens = l_lexer.get_tokens();
			if (detail::s_arg_data.show_tokens)
				l_lexer.print_tokens();
			parser l_parser(tokens);
			ref<program> tree = l_parser.parse();
			if (detail::s_arg_data.show_parse_tree)
				std::cout << tree->to_string();
			evaluator eval;
			eval.evaluate_program(tree);

			// TODO
		}
		catch (const exception& e)
		{
			std::cout << e.what(); // TODO
			exit(EXIT_FAILURE);
		}
	}
}