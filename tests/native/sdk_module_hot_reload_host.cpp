#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "SDK hot reload host expected two library paths." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::HotReloadModule::load(argv[1]);
        auto getCounter = module.load_command<std::function<std::int32_t()>>("counter.get");
        auto addCounter = module.load_command<std::int32_t(std::int32_t)>("counter.add");
        auto broadcastTick = module.load_event<void(float)>("game.tick");

        const std::int32_t before = getCounter();
        const std::int32_t afterAdd = addCounter(3);

        module.reload_from(argv[2]);

        const std::int32_t afterReload = getCounter();
        broadcastTick(5.0f);
        const std::int32_t afterTick = getCounter();

        std::cout << "SDK reload: before=" << before
                  << " afterAdd=" << afterAdd
                  << " afterReload=" << afterReload
                  << " afterTick=" << afterTick
                  << " generation=" << module.generation()
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
