#include "file_wrapper.h"
#include "../base/exception.h"

namespace wio 
{
    file_wrapper::file_wrapper(FILE* file, const std::string& filename, long long open_mode)
        : m_file(file), m_filename(filename), m_open_mode(open_mode)
    {
        if (!m_file) 
            throw file_error("file_wrapper: file pointer is null!");
    }

    FILE* file_wrapper::get_file() const 
    {
        return m_file;
    }

    const std::string& file_wrapper::get_filename() const
    {
        return m_filename;
    }

    long long file_wrapper::get_open_mode() const
    {
        return m_open_mode;
    }

    bool file_wrapper::operator==(const file_wrapper& right) const
    {
        return (m_file == right.m_file);
    }
} // namespace wio