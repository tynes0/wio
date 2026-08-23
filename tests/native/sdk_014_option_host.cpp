#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 Option host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        auto state = module.load_object("Sdk14Options").create();
        auto selected = state.field("selected");
        auto nested = state.field("nested");
        auto wide = state.field("wide");

        using IntOption = wio::sdk::WioOption<std::int32_t>;
        using NestedOption = wio::sdk::WioOption<IntOption>;
        using WideOption = wio::sdk::WioOption<wio::sdk::WioU64>;
        if (!selected.can_access_as<IntOption>() ||
            !nested.can_access_as<NestedOption>() ||
            !wide.can_access_as<WideOption>() ||
            selected.supports_dynamic_value() ||
            selected.get_as<IntOption>().value_or(0) != 14 ||
            nested.get_as<NestedOption>().value().value_or(0) != 28 ||
            wide.get_as<WideOption>().value().value() != 14000000000ull)
        {
            std::cerr << "Initial Option values did not cross the SDK bridge.\n";
            return EXIT_FAILURE;
        }

        selected.set_as(IntOption::none());
        nested.set_as(NestedOption::some(IntOption::none()));
        wide.set_as(WideOption::none());
        if (state.method<std::int32_t(std::int32_t)>("SelectedOr")(41) != 41 ||
            state.method<std::int32_t(std::int32_t)>("NestedOr")(42) != 42 ||
            state.method<wio::sdk::WioU64(wio::sdk::WioU64)>("WideOr")(wio::sdk::WioU64{43}).value() != 43ull)
        {
            std::cerr << "Empty Option values were not reconstructed in Wio.\n";
            return EXIT_FAILURE;
        }

        selected.set_as(IntOption::some(7));
        nested.set_as(NestedOption::some(IntOption::some(9)));
        wide.set_as(WideOption::some(wio::sdk::WioU64{18000000000ull}));
        const auto selectedAfter = selected.get_as<IntOption>();
        const auto nestedAfter = nested.get_as<NestedOption>();
        const auto wideAfter = wide.get_as<WideOption>();
        if (state.method<std::int32_t(std::int32_t)>("SelectedOr")(0) != 7 ||
            state.method<std::int32_t(std::int32_t)>("NestedOr")(0) != 9 ||
            selectedAfter.value_or(0) != 7 ||
            nestedAfter.value().value_or(0) != 9 ||
            state.method<wio::sdk::WioU64(wio::sdk::WioU64)>("WideOr")(wio::sdk::WioU64{0}).value() != 18000000000ull ||
            wideAfter.is_none() || wideAfter.value().value() != 18000000000ull)
        {
            std::cerr << "Some Option values did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 Option: selected=" << selectedAfter.value()
                  << " nested=" << nestedAfter.value().value()
                  << " empty-roundtrip=ok\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
