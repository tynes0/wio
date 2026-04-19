#pragma once

#include <exception>
#include <string>

// NOLINTBEGIN(bugprone-macro-parentheses)
#define WIO_CREATE_RUNTIME_EXCEPTION(EXPN)                                                  \
    class EXPN : public wio::runtime::RuntimeException                                      \
    {                                                                               \
    public:                                                                         \
        explicit EXPN(const char* message)                                          \
            : RuntimeException((std::string(#EXPN) + ": " + (message) + ' ').c_str()) {} \
    }
// NOLINTEND(bugprone-macro-parentheses)

namespace wio::runtime
{
    class RuntimeException : public std::exception
    {
    public:
        explicit RuntimeException(std::string msg)
            : message_(std::move(msg)) {}

        const char* what() const noexcept override
        {
            return message_.c_str();
        }

    private:
        std::string message_;
    };
    
    WIO_CREATE_RUNTIME_EXCEPTION(FileError);
    WIO_CREATE_RUNTIME_EXCEPTION(OutOfMemory);
}

#undef WIO_CREATE_RUNTIME_EXCEPTION