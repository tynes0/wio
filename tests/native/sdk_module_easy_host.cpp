#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "SDK host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        module.update(2.0f);

        auto getCounter = module.load_command<std::function<std::int32_t()>>("counter.get");
        auto addCounter = module.load_function<std::int32_t(std::int32_t)>("counter.add");
        auto broadcastTick = module.load_event<std::function<void(float)>>("game.tick");

        const std::int32_t before = getCounter();
        const std::int32_t afterAdd = addCounter(3);
        broadcastTick(5.0f);
        const std::int32_t afterTick = getCounter();

        std::cout << "SDK easy: before=" << before
                  << " afterAdd=" << afterAdd
                  << " afterTick=" << afterTick
                  << " exports=" << module.api().exportCount
                  << " commands=" << module.api().commandCount
                  << " events=" << module.api().eventHookCount
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
