#include "std_async.h"

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{
    void SetWorkerEnvironment(const char* value)
    {
#if defined(_WIN32)
        if (_putenv_s("WIO_ASYNC_WORKERS", value ? value : "") != 0)
            throw std::runtime_error("could not update WIO_ASYNC_WORKERS");
#else
        const int result = value
            ? setenv("WIO_ASYNC_WORKERS", value, 1)
            : unsetenv("WIO_ASYNC_WORKERS");
        if (result != 0)
            throw std::runtime_error("could not update WIO_ASYNC_WORKERS");
#endif
    }

    void Require(const bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main()
{
    const char* originalValue = std::getenv("WIO_ASYNC_WORKERS");
    const std::optional<std::string> original = originalValue
        ? std::optional<std::string>(originalValue)
        : std::nullopt;

    try
    {
        SetWorkerEnvironment(nullptr);
        const auto boundedDefault = wio::runtime::ResolveDefaultAsyncWorkerCount();
        Require(boundedDefault >= 2 && boundedDefault <= 8,
            "default worker count must respect the bounded host range");

        SetWorkerEnvironment("17");
        Require(wio::runtime::ResolveDefaultAsyncWorkerCount() == 17,
            "explicit worker count must override the bounded default");

        SetWorkerEnvironment("1");
        const auto invalidLow = wio::runtime::ResolveDefaultAsyncWorkerCount();
        Require(invalidLow >= 2 && invalidLow <= 8,
            "worker count below the supported range must use the default");

        SetWorkerEnvironment("257");
        const auto invalidHigh = wio::runtime::ResolveDefaultAsyncWorkerCount();
        Require(invalidHigh >= 2 && invalidHigh <= 8,
            "worker count above the supported range must use the default");

        SetWorkerEnvironment(original ? original->c_str() : nullptr);
        return 0;
    }
    catch (...)
    {
        try { SetWorkerEnvironment(original ? original->c_str() : nullptr); }
        catch (...) {}
        return 1;
    }
}
