#include "wio/common/logger.h"

#include "compiler.h"
#include "wio/common/exception.h"

namespace wio
{
    Logger::Logger()
            : logger_("WIO LOG", "[%D-%T] [%L] %N: %V%n"), warningCount_(0), errorCount_(0)
    {
#if defined(_DEBUG) || defined(DEBUG)
        logger_.add_sink(std::make_shared<dtlog::debug_sink>());
#endif
    }

    Logger& Logger::get()
    {
        static Logger logger;
        return logger;
    }

    dtlog::logger<>& Logger::getLogger()
    {
        return logger_;
    }

    void Logger::processTheWarnings()
    {
        if (areTheWarningsBeingTreatedAsErrors())
        {
            processTheErrors<CompilationError>();
        }
        else
        {
            if (warningCount_)
            {
                logger_.warning("{}warning (s) found during compilation.", warningCount_);
                warningCount_ = 0;
            }
        }
    }

    void Logger::beginDiagnosticProbe()
    {
        diagnosticProbeErrorCounts_.push_back(0);
    }

    int32_t Logger::endDiagnosticProbe()
    {
        if (diagnosticProbeErrorCounts_.empty())
            return 0;

        const int32_t errorCount = diagnosticProbeErrorCounts_.back();
        diagnosticProbeErrorCounts_.pop_back();
        return errorCount;
    }

    bool Logger::areTheWarningsBeingTreatedAsErrors()
    {
        return Compiler::get().getFlags().get_WarnAsError();
    }
}
