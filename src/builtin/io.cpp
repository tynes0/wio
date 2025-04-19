#define _CRT_SECURE_NO_WARNINGS
#include "io.h"

#include <iostream>

#include "builtin_base.h"
#include "helpers.h"

#include "../types/file_wrapper.h"
#include "../utils/filesystem.h"
#include "../utils/util.h"

#include "../variables/function.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"

#include "../base/exception.h"

namespace wio
{
    namespace builtin
    {
        constexpr long long FILE_MODE_READ      = BIT(0);
        constexpr long long FILE_MODE_WRITE     = BIT(1);
        constexpr long long FILE_MODE_APPEND    = BIT(2);
        constexpr long long FILE_MODE_BINARY    = BIT(3);
        constexpr long long FILE_MODE_TRUNC     = BIT(4);
        constexpr long long FILE_MODE_ATE       = BIT(5);

        using namespace std::string_literals;

        namespace detail
        {
            static ref<variable_base> b_open_file(const std::vector<ref<variable_base>>& args)
            {
                auto filename_var = std::dynamic_pointer_cast<variable>(args[0]);
                if (!filename_var || filename_var->get_type() != variable_type::vt_string) 
                    throw builtin_error("OpenFile(): Filename must be a string.");

                std::string filename = filename_var->get_data_as<std::string>();

                auto mode_var = std::dynamic_pointer_cast<variable>(args[1]);
                if (!mode_var || mode_var->get_type() != variable_type::vt_integer)
                    throw builtin_error("OpenFile(): Mode must be an integer.");

                long long open_mode = mode_var->get_data_as<long long>();

                FILE* file = nullptr;

                std::string mode_str;
                bool is_read = open_mode & FILE_MODE_READ;
                bool is_write = open_mode & FILE_MODE_WRITE;
                bool is_append = open_mode & FILE_MODE_APPEND;
                bool is_binary = open_mode & FILE_MODE_BINARY;
                bool is_trunc = open_mode & FILE_MODE_TRUNC;
                bool is_ate = open_mode & FILE_MODE_ATE;

                if (is_read && !is_write && !is_append)
                {
                    mode_str = "r";
                    if (is_binary)
                        mode_str.push_back('b');
                    if (is_trunc)
                        throw file_error("OPEN_MODE_TRUNC not works with read-only files!");
                }
                else if (!is_read && is_write && !is_append)
                {
                    if (is_ate)
                    {
                        if(is_trunc)
                            mode_str = "w";
                        else
                            mode_str = "a";
                    }
                    else
                    {
                        mode_str = "w";
                    }

                    if (is_binary)
                        mode_str.push_back('b');
                }
                else if (is_read && is_write && !is_append)
                {
                    mode_str = "w+";
                    if (is_binary)
                        mode_str.push_back('b');
                }
                else if (is_append)
                {
                    if(is_trunc)
                        mode_str = "w";
                    else
                        mode_str = "a";
                    if (is_read)
                        mode_str.push_back('+');
                    if (is_binary)
                        mode_str.push_back('b');
                }


                file = fopen(filename.c_str(), mode_str.c_str());

                if (!file) 
                    return create_null_variable();

                if (is_ate)
                    fseek(file, 0, SEEK_END);

                return make_ref<variable>(any(file_wrapper(file, filename, open_mode)), variable_type::vt_file);
            }

            static ref<variable_base> b_close_file(const std::vector<ref<variable_base>>& args)
            {
                auto file_var = std::dynamic_pointer_cast<variable>(args[0]);
                if (!file_var || file_var->get_type() != variable_type::vt_file)
                    throw builtin_error("OpenFile(): File must be a file.");

                fclose(file_var->get_data_as<file_wrapper>().get_file());
                return create_null_variable();
            }

            static ref<variable_base> b_write(const std::vector<ref<variable_base>>& args)
            {
                auto file_var = std::dynamic_pointer_cast<variable>(args[0]);
                if (!file_var || file_var->get_type() != variable_type::vt_file && file_var->get_type() != variable_type::vt_string)
                    throw builtin_error("Write(): File must be a file or filepath (string).");

                auto value_var = std::dynamic_pointer_cast<variable>(args[1]);
                if (!value_var || value_var->get_type() != variable_type::vt_string)
                    throw builtin_error("Write(): Value must be an string.");

                if(file_var->get_type() == variable_type::vt_file)
                    filesystem::write_file(file_var->get_data_as<file_wrapper>().get_file(), value_var->get_data_as<std::string>());
                else
                    filesystem::write_fp(file_var->get_data_as<std::string>(), value_var->get_data_as<std::string>());

                return create_null_variable();
            }

            static ref<variable_base> b_read(const std::vector<ref<variable_base>>& args)
            {
                auto file_var = std::dynamic_pointer_cast<variable>(args[0]);
                if (!file_var || file_var->get_type() != variable_type::vt_file && file_var->get_type() != variable_type::vt_string)
                    throw builtin_error("OpenFile(): File must be a file or filepath (string).");

                file_wrapper wrapped_file = file_var->get_data_as<file_wrapper>();
                raw_buffer buf;

                if(wrapped_file != wrapped_stdin)
                    buf = filesystem::read_file(wrapped_file.get_file());
                else
                    buf = filesystem::read_stdout();

                auto result = make_ref<variable>(any(std::string(buf.as<char>(), buf.size)), variable_type::vt_string);
                return result;
            }
        }

        static void load_all(symbol_map& table)
        {
            using namespace wio::builtin::detail;

            loader::load_function(table, "OpenFile", detail::b_open_file, pa<2>{ variable_type::vt_string, variable_type::vt_integer });
            loader::load_function(table, "CloseFile", detail::b_close_file, pa<1>{ variable_type::vt_file });
            loader::load_function(table, "Write", detail::b_write, pa<2>{ variable_type::vt_any, variable_type::vt_string });
            loader::load_function(table, "Read", detail::b_read, pa<1>{ variable_type::vt_file });

            loader::load_constant(table, "OPEN_MODE_READ", variable_type::vt_integer, FILE_MODE_READ);
            loader::load_constant(table, "OPEN_MODE_WRITE", variable_type::vt_integer, FILE_MODE_WRITE);
            loader::load_constant(table, "OPEN_MODE_APPEND", variable_type::vt_integer, FILE_MODE_APPEND);
            loader::load_constant(table, "OPEN_MODE_BINARY", variable_type::vt_integer, FILE_MODE_BINARY);
            loader::load_constant(table, "OPEN_MODE_TRUNC", variable_type::vt_integer, FILE_MODE_TRUNC);
            loader::load_constant(table, "OPEN_MODE_ATE", variable_type::vt_integer, FILE_MODE_ATE);
            loader::load_constant(table, "STDOUT", variable_type::vt_file, wrapped_stdout);
            loader::load_constant(table, "STDERR", variable_type::vt_file, wrapped_stderr);
            loader::load_constant(table, "STDIN", variable_type::vt_file, wrapped_stdin);
        }

        void io::load(ref<scope> target_scope)
        {

            if (!target_scope)
                target_scope = main_table::get().get_builtin_scope();

            load_all(target_scope->get_symbols());
        }

        void io::load_table(ref<symbol_table> target_table)
        {
            if (target_table)
                load_all(target_table->get_symbols());
        }

        void io::load_symbol_map(symbol_map& target_map)
        {
            load_all(target_map);
        }
    }
}
