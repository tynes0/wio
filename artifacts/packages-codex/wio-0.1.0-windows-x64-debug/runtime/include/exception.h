#pragma once

#include <exception>
#include <string>
#include <utility>

// NOLINTBEGIN(bugprone-macro-parentheses)
#define WIO_CREATE_RUNTIME_EXCEPTION(EXPN)                                      \
    class EXPN : public ::wio::runtime::RuntimeException                        \
    {                                                                           \
    public:                                                                     \
        explicit EXPN(std::string message)                                      \
            : RuntimeException(std::string(#EXPN) + ": " + std::move(message)) \
        {                                                                       \
        }                                                                       \
                                                                                \
        explicit EXPN(const char* message)                                      \
            : EXPN(std::string(message != nullptr ? message : ""))             \
        {                                                                       \
        }                                                                       \
    }
// NOLINTEND(bugprone-macro-parentheses)

namespace wio::runtime
{
    class RuntimeException : public std::exception
    {
    public:
        explicit RuntimeException(std::string message)
            : message_(std::move(message))
        {
        }

        [[nodiscard]] const char* what() const noexcept override
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
