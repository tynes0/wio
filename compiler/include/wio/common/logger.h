#pragma once

#include <dtlog.h>

#include "general/core.h"
#include "utility.h"

namespace wio
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class Logger
    {
    public:
        Logger();

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        static Logger& get();
        dtlog::logger<>& getLogger();

        template <class ...Args>
        void addWarning(const std::string& message, Args&&... args)
        {
            if (areTheWarningsBeingTreatedAsErrors())
            {
                addError(message, std::forward<Args>(args)...);
            }
            else
            {
                logger_.warning(message, std::forward<Args>(args)...);
                warningCount_++;
            }
        }

        template <class ...Args>
        void addError(const std::string& message, Args&&... args)
        {
            logger_.error(message, std::forward<Args>(args)...);
            errorCount_++;
        }

        template <class ExceptionClass>
        void processTheErrors() 
        {
            if (errorCount_ > 0)
            {
                int errCount = errorCount_;
                errorCount_ = 0;
                throw ExceptionClass((std::to_string(errCount) + " error(s) found during compilation.").c_str());
            }
        }

        void processTheWarnings();

        int32_t getErrorCount() const  { return errorCount_; }
        int32_t getWarningCount() const { return warningCount_; }
    
    private:
        static bool areTheWarningsBeingTreatedAsErrors();
        
        dtlog::logger<> logger_;
        int32_t warningCount_;
        int32_t errorCount_;
    };
}

#define WIO_LOG_TRACE(...) ::wio::Logger::get().getLogger().trace(__VA_ARGS__)
#define WIO_LOG_INFO(...) ::wio::Logger::get().getLogger().info(__VA_ARGS__)
#define WIO_LOG_DEBUG(...) ::wio::Logger::get().getLogger().debug(__VA_ARGS__)
#define WIO_LOG_WARN(...) ::wio::Logger::get().getLogger().warning(__VA_ARGS__)
#define WIO_LOG_ERROR(...) ::wio::Logger::get().getLogger().error(__VA_ARGS__)
#define WIO_LOG_FATAL(...) ::wio::Logger::get().getLogger().critical(__VA_ARGS__)

#define WIO_RUNTIME_LOG_TRACE(...) ::wio::Logger::get().getLogger().trace(__VA_ARGS__)
#define WIO_RUNTIME_LOG_INFO(...) ::wio::Logger::get().getLogger().info(__VA_ARGS__)
#define WIO_RUNTIME_LOG_DEBUG(...) ::wio::Logger::get().getLogger().debug(__VA_ARGS__)
#define WIO_RUNTIME_LOG_WARN(...) ::wio::Logger::get().getLogger().warning(__VA_ARGS__)
#define WIO_RUNTIME_LOG_ERROR(...) ::wio::Logger::get().getLogger().error(__VA_ARGS__)
#define WIO_RUNTIME_LOG_FATAL(...) ::wio::Logger::get().getLogger().critical(__VA_ARGS__)

#define WIO_LOG_ADD_WARN(loc, ...) ::wio::Logger::get().addWarning("Warning [Line {}, Col {}]: {}", loc.line, loc.column, common::formatString(__VA_ARGS__))
#define WIO_LOG_ADD_ERROR(loc, ...) ::wio::Logger::get().addError("Error [Line {}, Col {}]: {}", loc.line, loc.column, common::formatString(__VA_ARGS__))

#define WIO_LOG_PROCESS_WARNINGS() ::wio::Logger::get().processTheWarnings()
#define WIO_LOG_PROCESS_ERRORS(T) ::wio::Logger::get().processTheErrors<T>()

#ifdef _DEBUG
    #define WIO_ASSERT(condition, ...) do { if(!(condition)) { WIO_LOG_FATAL("Assertion failed. File: {0}, Line: {1}, Message: {2}", __FILE__, __LINE__, __VA_ARGS__); WIO_DEBUGBREAK(); } } while(false)
#else // _DEBUG
    #define WIO_ASSERT() (void)(0)
#endif // _DEBUG

#define WIO_CONDITIONAL_THROW(condition, throwClass, ...) do { if(!(condition)) { throw throwClass(__VA_ARGS__); } } while(false)
