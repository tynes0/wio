#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Expected a Wio module library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        module.update(1.0f);

        auto getCounter = module.load_command<std::int32_t()>("counter.get");
        auto addCounter = module.load_command<std::int32_t(std::int32_t)>("counter.add");
        auto broadcastTick = module.load_event<void(float)>("game.tick");

        const std::int32_t before = getCounter();
        const std::int32_t afterAdd = addCounter(5);
        broadcastTick(2.0f);
        const std::int32_t afterTick = getCounter();

        std::cout << "Hybrid project: before=" << before
                  << " afterAdd=" << afterAdd
                  << " afterTick=" << afterTick
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

