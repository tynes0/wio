#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "wio_sdk.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "SDK fixed application host expected one module path.\n";
        return 2;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        auto application = module.application();
        application.start();

        std::int32_t frames = 0;
        while (application.update(0.05) == wio::sdk::ApplicationFrameStatus::Running)
        {
            ++frames;
            if (frames > 10)
                throw std::runtime_error("fixed application did not request exit");
        }
        ++frames;
        if (frames != 6 || application.exit_code() != 23)
            throw std::runtime_error("fixed-step accumulation contract failed");
        application.close();

        std::cout << "sdk-fixed-application-ok frames=" << frames
                  << " ticks=3 exit=" << application.exit_code() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
