#define _CRT_SECURE_NO_WARNINGS
#include "io.h"

#include <iostream>

#include "builtin_base.h"

#include "../utils/file_wrapper.h"
#include "../utils/filesystem.h"

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
            static int s_member_count = 0;

            static ref<variable_base> b_print(ref<variable_base> base)
            {
                if(base->get_type() == variable_type::vt_null)
                    filesystem::write_stdout("null");
                else if (base->get_base_type() == variable_base_type::variable)
                {
                    ref<variable> var = std::dynamic_pointer_cast<variable>(base);
                    if (var->get_type() == variable_type::vt_string)
                    {
                        filesystem::write_stdout(s_member_count ? ('\"' + var->get_data_as<std::string>() + '\"') : var->get_data_as<std::string>());
                    }
                    else if (var->get_type() == variable_type::vt_integer)
                    {
                        filesystem::write_stdout(std::to_string(var->get_data_as<long long>()));
                    }
                    else if (var->get_type() == variable_type::vt_float)
                    {
                        filesystem::write_stdout(std::to_string(var->get_data_as<double>()));
                    }
                    else if (var->get_type() == variable_type::vt_bool)
                    {
                        filesystem::write_stdout(var->get_data_as<bool>() ? "true" : "false");
                    }
                    else if (var->get_type() == variable_type::vt_character)
                    {
                        filesystem::write_stdout(s_member_count ? ("'"s + var->get_data_as<char>() + '\'') : std::string(1, var->get_data_as<char>()));
                    }
                    else if (var->get_type() == variable_type::vt_null)
                    {
                        filesystem::write_stdout("null");
                    }
                    else if (var->get_type() == variable_type::vt_type)
                    {
                        std::string_view view = frenum::to_string_view(var->get_data_as<variable_type>());
                        view.remove_prefix(3); // vt_
                        filesystem::write_stdout(view.data());
                    }
                    else
                        throw builtin_error("Invalid expression in print parameter!");
                }
                else if (base->get_base_type() == variable_base_type::array)
                {
                    ref<var_array> arr = std::dynamic_pointer_cast<var_array>(base);
                    filesystem::write_stdout("[");
                    s_member_count++;
                    for (size_t i = 0; i < arr->size(); ++i)
                    {
                        b_print(arr->get_element(i));
                        if (i != arr->size() - 1)
                            filesystem::write_stdout(", ");
                    }
                    s_member_count--;
                    filesystem::write_stdout("]");

                }
                else if (base->get_base_type() == variable_base_type::dictionary)
                {
                    ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(base);
                    filesystem::write_stdout("{");
                    const auto& dict_map = dict->get_data();

                    size_t i = 0;
                    s_member_count++;
                    for (const auto& [key, value] : dict_map)
                    {
                        filesystem::write_stdout("[");
                        filesystem::write_stdout('\"' + key + '\"');
                        filesystem::write_stdout(", ");
                        b_print(value);
                        filesystem::write_stdout("]");
                        if (i != dict->size() - 1)
                            filesystem::write_stdout(", ");
                        i++;
                    }
                    s_member_count--;

                    filesystem::write_stdout("}");
                }

                return make_ref<null_var>();
            }
            static ref<variable_base> b_println(ref<variable_base> base)
            {
                b_print(base);
                filesystem::write_stdout("\n");
                return make_ref<null_var>();
            }

            static ref<variable_base> b_input()
            {
                raw_buffer buf = filesystem::read_stdout();
                return make_ref<variable>(any(std::string(buf.as<char>())), variable_type::vt_string);
            }

            static ref<variable_base> b_open_file(ref<variable_base> filepath, ref<variable_base> mode)
            {
                auto filename_var = std::dynamic_pointer_cast<variable>(filepath);
                if (!filename_var || filename_var->get_type() != variable_type::vt_string) 
                    throw builtin_error("OpenFile(): Filename must be a string.");

                std::string filename = filename_var->get_data_as<std::string>();

                auto mode_var = std::dynamic_pointer_cast<variable>(mode);
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
                    return make_ref<null_var>();

                if (is_ate)
                    fseek(file, 0, SEEK_END);

                return make_ref<variable>(any(file_wrapper(file, filename, open_mode)), variable_type::vt_file);
            }

            static ref<variable_base> b_close_file(ref<variable_base> file)
            {
                auto file_var = std::dynamic_pointer_cast<variable>(file);
                if (!file_var || file_var->get_type() != variable_type::vt_file)
                    throw builtin_error("OpenFile(): File must be a file.");

                fclose(file_var->get_data_as<file_wrapper>().get_file());
                return make_ref<null_var>();
            }

            static ref<variable_base> b_write(ref<variable_base> file, ref<variable_base> value)
            {
                auto file_var = std::dynamic_pointer_cast<variable>(file);
                if (!file_var || file_var->get_type() != variable_type::vt_file && file_var->get_type() != variable_type::vt_string)
                    throw builtin_error("Write(): File must be a file or filepath (string).");

                auto value_var = std::dynamic_pointer_cast<variable>(value);
                if (!value_var || value_var->get_type() != variable_type::vt_string)
                    throw builtin_error("Write(): Value must be an string.");

                if(file_var->get_type() == variable_type::vt_file)
                    filesystem::write_file(file_var->get_data_as<file_wrapper>().get_file(), value_var->get_data_as<std::string>());
                else
                    filesystem::write_fp(file_var->get_data_as<std::string>(), value_var->get_data_as<std::string>());

                return make_ref<null_var>();
            }

            static ref<variable_base> b_read(ref<variable_base> file)
            {
                auto file_var = std::dynamic_pointer_cast<variable>(file);
                if (!file_var || file_var->get_type() != variable_type::vt_file && file_var->get_type() != variable_type::vt_string)
                    throw builtin_error("OpenFile(): File must be a file or filepath (string).");

                raw_buffer buf = filesystem::read_file(file_var->get_data_as<file_wrapper>().get_file());
                auto result = make_ref<variable>(any(std::string(buf.as<char>(), buf.size)), variable_type::vt_string);
                return result;
            }
        }

        void io::load()
        {
            loader::load_function<1>("Print", detail::b_print, variable_type::vt_null, { variable_type::vt_any });
            loader::load_function<1>("Println", detail::b_println, variable_type::vt_null, { variable_type::vt_any });
            loader::load_function<0>("Input", detail::b_input, variable_type::vt_string, {});
            loader::load_function<2>("OpenFile", detail::b_open_file, variable_type::vt_file, { variable_type::vt_string, variable_type::vt_integer });
            loader::load_function<1>("CloseFile", detail::b_close_file, variable_type::vt_null, { variable_type::vt_file });
            loader::load_function<2>("Write", detail::b_write, variable_type::vt_null, { variable_type::vt_any, variable_type::vt_string });
            loader::load_function<1>("Read", detail::b_read, variable_type::vt_string, { variable_type::vt_file });

            loader::load_constant("OPEN_MODE_READ", variable_type::vt_integer, FILE_MODE_READ);
            loader::load_constant("OPEN_MODE_WRITE", variable_type::vt_integer, FILE_MODE_WRITE);
            loader::load_constant("OPEN_MODE_APPEND", variable_type::vt_integer, FILE_MODE_APPEND);
            loader::load_constant("OPEN_MODE_BINARY", variable_type::vt_integer, FILE_MODE_BINARY);
            loader::load_constant("OPEN_MODE_TRUNC", variable_type::vt_integer, FILE_MODE_TRUNC);
            loader::load_constant("OPEN_MODE_ATE", variable_type::vt_integer, FILE_MODE_ATE);
            loader::load_constant("STDOUT", variable_type::vt_file, wrapped_stdout);
            loader::load_constant("STDERR", variable_type::vt_file, wrapped_stderr);
            loader::load_constant("STDIN", variable_type::vt_file, wrapped_stdin);
        }
    }
}
