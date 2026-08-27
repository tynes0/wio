#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "wio_sdk.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "SDK application host expected one module path.\n";
        return 2;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        const auto info = module.inspect();
        if (!module.supports_application() ||
            !info.has_capability(WIO_MODULE_CAP_APPLICATION_HOST_V1) ||
            !info.application.has_value() ||
            *info.application != "HostedApplication")
            throw std::runtime_error("application host metadata is incomplete");

        auto application = module.application();
        if (application.name() != "HostedApplication" || application.started() || application.closed())
            throw std::runtime_error("fresh application host state is invalid");

        application.start();
        if (!application.started() || application.exit_requested())
            throw std::runtime_error("application start state is invalid");

        std::int32_t frames = 0;
        while (application.update(1.0 / 60.0) == wio::sdk::ApplicationFrameStatus::Running)
        {
            ++frames;
            if (frames > 8)
                throw std::runtime_error("hosted application did not request exit");
        }
        ++frames;

        if (!application.exit_requested() || application.exit_code() != 17 || frames != 3)
            throw std::runtime_error("hosted application exit contract failed");

        std::atomic<bool> wrongThreadRejected{false};
        std::thread wrongThread([&]
        {
            try
            {
                (void)application.pump_main();
            }
            catch (const wio::sdk::Error& error)
            {
                wrongThreadRejected.store(error.code() == wio::sdk::ErrorCode::LifecycleFailed);
            }
        });
        wrongThread.join();
        if (!wrongThreadRejected.load())
            throw std::runtime_error("application host accepted a wrong-thread operation");

        application.close();
        application.close();
        if (!application.closed())
            throw std::runtime_error("application close was not idempotent");

        auto staleModule = wio::sdk::Module::load(argv[1]);
        auto staleApplication = staleModule.application();
        staleModule.close();
        bool staleRejected = false;
        try
        {
            staleApplication.start();
        }
        catch (const wio::sdk::Error& error)
        {
            staleRejected = error.code() == wio::sdk::ErrorCode::StaleBinding;
        }
        if (!staleRejected)
            throw std::runtime_error("application host remained callable after module close");

        std::cout << "sdk-application-host-ok frames=" << frames
                  << " exit=" << application.exit_code()
                  << " wrong-thread=1 stale=1\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
