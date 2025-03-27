#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>

namespace wio
{
    struct argument
    {
        std::string id;
        std::vector<std::string> aliases;
        std::string description;
        std::function<void(const std::string&)> action;
        std::string value;
        bool takes_value = false;
        bool is_set = false;
        bool is_help = false;
    };

    class argument_parser
    {
    public:
        argument_parser();

        void add_argument(const std::string& id, const std::vector<std::string>& aliases, const std::string& description,
            std::function<void(const std::string&)> action = nullptr, bool is_help = false, bool takes_value = false);

        void parse(int argc, char* argv[]);
        void set_program_name(const std::string& program_name);
        void enable_required_file(const std::string& extension);

        std::string help() const;
        bool help_called() const;
        bool is_set(const std::string& arg_id) const;
        std::string get_value(const std::string& arg_id) const;
        const std::string& get_file() const;
        const std::vector<std::string>& get_positional_arguments() const;
    private:
        std::map<std::string, argument> m_arguments;
        std::string m_program_name;
        std::string m_required_file_extension;
        std::string m_file;
        std::vector<std::string> m_positional_arguments;
        bool m_help_called = false;
    };

}