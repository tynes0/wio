#include <iostream>
#include <stdexcept>
#include <string>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        auto application = module.application();
        bool rejected = false;
        try
        {
            application.start();
        }
        catch (const wio::sdk::Error& error)
        {
            rejected = error.code() == wio::sdk::ErrorCode::LifecycleFailed &&
                application.last_error().find("intentional application start failure") != std::string::npos;
        }
        if (!rejected || !application.closed() || application.started())
            throw std::runtime_error("failed application start was not rolled back into a terminal state");

        bool restartRejected = false;
        try { application.start(); }
        catch (const wio::sdk::Error&) { restartRejected = true; }
        if (!restartRejected)
            throw std::runtime_error("rolled-back application accepted a second start");

        std::cout << "sdk-application-rollback-ok closed=1 restart=blocked\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
