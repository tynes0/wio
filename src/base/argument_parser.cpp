#include "argument_parser.h"

#include "../utils/filesystem.h"
#include "../base/exception.h"

#include <algorithm>
#include <sstream>

namespace wio
{
    argument_parser::argument_parser() : m_program_name("<program_name>")
    {
    }

    void argument_parser::add_argument(const std::string& id, const std::vector<std::string>& aliases, const std::string& description, std::function<void(const std::string&)> action, bool is_help, bool takes_value)
    {
        argument arg;
        arg.id = id;
        arg.aliases = aliases;
        arg.description = description;
        arg.action = action;
        arg.value = "";
        arg.takes_value = takes_value;
        arg.is_set = false;
        arg.is_help = is_help;
        m_arguments[id] = arg;
    }

    void argument_parser::parse(int argc, char* argv[])
    {
        if (argc < 2 && !m_required_file_extension.empty())
            throw exception("At least one file argument is required.");

        static constexpr auto compare = [](char a, char b) { return std::tolower(a) == std::tolower(b); };

        m_positional_arguments.clear();
        bool positional_arg_mode = false;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg_str(argv[i]);

            if (!positional_arg_mode && arg_str[0] == '-')
            {
                bool found = false;
                for (auto& pair : m_arguments)
                {
                    argument& arg = pair.second;

                    for (const std::string& alias : arg.aliases)
                    {
                        if (alias.size() == arg_str.size() && std::equal(alias.begin(), alias.end(), arg_str.begin(), compare))
                        {
                            arg.is_set = true;
                            found = true;

                            if (arg.is_help)
                                m_help_called = true;

                            if (arg.takes_value)
                            {
                                if (i + 1 < argc)
                                {
                                    arg.value = argv[i + 1];
                                    i++;
                                    if (arg.action)
                                        arg.action(arg.value);
                                }
                                else
                                {
                                    throw exception(("argument '" + arg_str + "' requires a value.").c_str());
                                }
                            }
                            else
                            {
                                if (arg.action)
                                    arg.action("");
                            }
                            break;
                        }
                    }

                    if (found)
                        break;
                }

                if (!found)
                    throw exception(("Unknown argument: " + arg_str).c_str());
            }
            else
            {
                if (!positional_arg_mode && !m_required_file_extension.empty())
                {
                    if (!filesystem::check_extension(arg_str, m_required_file_extension))
                        throw exception("The file extensions do not match.");
                    m_file = arg_str;
                    positional_arg_mode = true;
                }
                else
                {
                    m_positional_arguments.push_back(arg_str);
                }
            }
        }
        if (!m_required_file_extension.empty() && m_file.empty() && !m_help_called)
            throw exception("At least one file argument is required.");
    }

    void argument_parser::set_program_name(const std::string& program_name)
    {
        m_program_name = program_name;
    }

    void argument_parser::enable_required_file(const std::string& extension)
    {
        m_required_file_extension = extension;
    }

    std::string argument_parser::help() const
    {
        std::stringstream ss;
        ss << ("Usage: " + m_program_name + " [options]");
        if (!m_required_file_extension.empty())
            ss << (" <" + m_required_file_extension + " file>");
        ss << " [positional_arguments]" << std::endl << "Options:" << std::endl;
        for (const auto& pair : m_arguments)
        {
            const argument& arg = pair.second;
            ss << "  ";

            bool first = true;
            for (const std::string& alias : arg.aliases)
            {
                if (!first)
                    ss << ", ";

                ss << alias;
                first = false;
            }

            if (arg.takes_value)
                ss << " <value>";

            ss << "\t" << arg.description << std::endl;
        }
        return ss.str();
    }

    bool argument_parser::help_called() const
    {
        return m_help_called;
    }

    bool argument_parser::is_set(const std::string& arg_id) const
    {
        if (m_arguments.count(arg_id))
            return m_arguments.at(arg_id).is_set;
        return false;
    }

    std::string argument_parser::get_value(const std::string& arg_id) const
    {
        if (m_arguments.count(arg_id))
            return m_arguments.at(arg_id).value;
        return "";
    }

    const std::string& argument_parser::get_file() const
    {
        return m_file;
    }

    const std::vector<std::string>& argument_parser::get_positional_arguments() const
    {
        return m_positional_arguments;
    }
}