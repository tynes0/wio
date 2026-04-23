#pragma once

#include <exception>
#include <string>

#include "location.h"

// NOLINTBEGIN(bugprone-macro-parentheses)
#define WIO_CREATE_EXCEPTION(EXPN)                                                  \
    class EXPN : public wio::common::Exception                                      \
    {                                                                               \
    public:                                                                         \
        explicit EXPN(const char* message)                                          \
            : Exception((std::string(#EXPN) + ": " + (message) + ' ').c_str()) {} \
    }

#define WIO_CREATE_EXCEPTION_WITH_LOC(EXPN)                                         \
    class EXPN : public wio::common::ExceptionWithLocation                          \
    {                                                                               \
    public:                                                                         \
        using ExceptionWithLocation::ExceptionWithLocation;                         \
        EXPN(const char* message, wio::common::Location loc)                        \
            : ExceptionWithLocation((std::string(#EXPN) + ": " + message + ' ').c_str(), loc) {} \
    }
// NOLINTEND(bugprone-macro-parentheses)

namespace wio::common
{
    class Exception : public std::exception
    {
    public:
        Exception() = default;

        explicit Exception(const char* message)
            : message_(message == nullptr ? "" : message) {}

        explicit Exception(std::string message)
            : message_(std::move(message)) {}

        [[nodiscard]] const char* what() const noexcept override
        {
            return message_.c_str();
        }

        [[nodiscard]] const std::string& message() const noexcept
        {
            return message_;
        }

    private:
        std::string message_;
    };

    class ExceptionWithLocation : public Exception
    {
    public:
        ExceptionWithLocation() = default;

        explicit ExceptionWithLocation(const char* message)
            : Exception(message) {}

        explicit ExceptionWithLocation(std::string message)
            : Exception(std::move(message)) {}
        
        ExceptionWithLocation(const char* message, Location loc)
            : Exception(message),
              location_(std::move(loc)) {}

        ExceptionWithLocation(std::string message, Location loc)
            : Exception(std::move(message)),
              location_(std::move(loc)) {}

        [[nodiscard]] const Location& location() const noexcept
        {
            return location_;
        }

        [[nodiscard]] bool hasLocation() const noexcept
        {
            return location_.line != 0 || location_.column != 0 || !location_.file.empty();
        }

    private:
        Location location_;
    };
}

namespace wio
{
    WIO_CREATE_EXCEPTION(FileError);
    WIO_CREATE_EXCEPTION(OutOfMemory);
    WIO_CREATE_EXCEPTION(CompilationError);
    
    WIO_CREATE_EXCEPTION_WITH_LOC(UnterminatedCommentError);
    WIO_CREATE_EXCEPTION_WITH_LOC(UnterminatedStringError);
    WIO_CREATE_EXCEPTION_WITH_LOC(UnterminatedCharError);
    WIO_CREATE_EXCEPTION_WITH_LOC(InvalidStringError);
    WIO_CREATE_EXCEPTION_WITH_LOC(InvalidNumberError);
    WIO_CREATE_EXCEPTION_WITH_LOC(InvalidCharError);
    WIO_CREATE_EXCEPTION_WITH_LOC(UnexpectedCharError);
    WIO_CREATE_EXCEPTION_WITH_LOC(UnexpectedTokenError);
    WIO_CREATE_EXCEPTION_WITH_LOC(UninitializedConstantError);
    WIO_CREATE_EXCEPTION_WITH_LOC(RedefinitionError);
    WIO_CREATE_EXCEPTION_WITH_LOC(InvalidEntryReturnType);
    WIO_CREATE_EXCEPTION_WITH_LOC(InvalidEntryParameter);
    WIO_CREATE_EXCEPTION_WITH_LOC(MissedEntryBody);
}

#undef WIO_CREATE_EXCEPTION
#undef WIO_CREATE_EXCEPTION_WITH_LOC
