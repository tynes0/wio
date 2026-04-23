#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <wio_sdk.h>

namespace
{
    template <typename TAction>
    void expect_stale(std::string_view label, TAction&& action, std::int32_t& staleCount)
    {
        try
        {
            action();
        }
        catch (const wio::sdk::Error& error)
        {
            if (error.code() == wio::sdk::ErrorCode::StaleBinding)
            {
                ++staleCount;
                return;
            }

            throw;
        }

        throw std::runtime_error("Expected stale binding for " + std::string(label) + '.');
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "SDK module stale host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);

        auto getCounter = module.load_command<std::int32_t()>("counter.get");
        auto counterType = module.load_object("Counter");
        auto counter = counterType.create();
        auto valueField = counter.field("value");
        auto add = counter.method<std::int32_t(std::int32_t)>("Add");

        const std::int32_t before = getCounter();
        const std::int32_t afterAdd = add(3);

        module.unload();

        std::int32_t staleCount = 0;
        expect_stale("counter.get", [&] { (void)getCounter(); }, staleCount);
        expect_stale("Counter type", [&] { (void)counterType.create(); }, staleCount);
        expect_stale("Counter object", [&] { (void)counter.get<std::int32_t>("value"); }, staleCount);
        expect_stale("Counter field", [&] { (void)valueField.get_as<std::int32_t>(); }, staleCount);
        expect_stale("Counter method", [&] { (void)add(1); }, staleCount);

        module.start();
        auto freshGetCounter = module.load_command<std::int32_t()>("counter.get");
        const std::int32_t restarted = freshGetCounter();

        std::cout << "SDK module stale: before=" << before
                  << " afterAdd=" << afterAdd
                  << " stale=" << staleCount
                  << " restarted=" << restarted
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
