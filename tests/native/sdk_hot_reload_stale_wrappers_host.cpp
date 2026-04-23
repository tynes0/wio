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
    if (argc < 3)
    {
        std::cerr << "SDK stale wrapper host expected two library paths." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::HotReloadModule::load(argv[1]);

        auto counterType = module.load_object("Counter");
        auto counter = counterType.create();
        auto valueField = counter.field("value");
        auto add = counter.method<std::int32_t(std::int32_t)>("Add");

        auto statsType = module.load_component("Stats");
        auto stats = statsType.create();
        auto hpField = stats.field("hp");

        const std::int32_t start = valueField.get_as<std::int32_t>();
        const std::int32_t afterAdd = add(5);
        const std::int32_t hpBefore = hpField.get_as<std::int32_t>();

        module.reload_from(argv[2]);

        std::int32_t staleCount = 0;
        expect_stale("Counter type", [&] { (void)counterType.create(); }, staleCount);
        expect_stale("Counter object", [&] { (void)counter.get<std::int32_t>("value"); }, staleCount);
        expect_stale("Counter field", [&] { (void)valueField.get_as<std::int32_t>(); }, staleCount);
        expect_stale("Counter method", [&] { (void)add(3); }, staleCount);
        expect_stale("Stats type", [&] { (void)statsType.create(); }, staleCount);
        expect_stale("Stats field", [&] { (void)hpField.get_as<std::int32_t>(); }, staleCount);

        auto freshCounter = module.load_object("Counter").create();
        auto freshAdd = freshCounter.method<std::int32_t(std::int32_t)>("Add");
        auto freshStats = module.load_component("Stats").create();

        const std::int32_t fresh = freshAdd(10);
        const std::int32_t freshHp = freshStats.get<std::int32_t>("hp");

        std::cout << "SDK reload stale wrappers: start=" << start
                  << " afterAdd=" << afterAdd
                  << " hp=" << hpBefore
                  << " stale=" << staleCount
                  << " fresh=" << fresh
                  << " freshHp=" << freshHp
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
