#pragma once

#include <cstdio> 
#include <string>

namespace wio 
{
    class file_wrapper 
    {
    public:
        file_wrapper(FILE* file, const std::string& filename, long long open_mode);
        FILE* get_file() const;
        const std::string& get_filename() const;
        long long get_open_mode() const;
    private:
        std::string m_filename;
        FILE* m_file;
        long long m_open_mode;
    };

    static const file_wrapper wrapped_stdin = file_wrapper(stdin, "", 0);
    static const file_wrapper wrapped_stdout = file_wrapper(stdout, "", 0);
    static const file_wrapper wrapped_stderr = file_wrapper(stderr, "", 0);

} // namespace wio