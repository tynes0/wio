#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 span host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        using Values = std::vector<std::int32_t>;

        auto module = wio::sdk::Module::load(argv[1]);
        auto profile = module.load_object("Sdk14SpanProfile").create();
        auto valuesField = profile.field("values");
        auto windowField = profile.field("window");
        if (!valuesField.can_access_as<Values>() ||
            !windowField.can_access_as<wio::sdk::WioSpanRange>() ||
            windowField.can_access_as<wio::sdk::WioSpan<std::int32_t>>() ||
            !windowField.supports_dynamic_value())
        {
            std::cerr << "Span range descriptor does not match its SDK value.\n";
            return EXIT_FAILURE;
        }

        auto initialValues = valuesField.get_as<Values>();
        const auto initialRange = windowField.get_as<wio::sdk::WioSpanRange>();
        const wio::sdk::WioSpan<const std::int32_t> initialView(
            initialValues.data(), initialValues.size(), initialRange
        );
        if (initialRange.start() != 1u || initialRange.count() != 3u ||
            initialView.count() != 3u || initialView.first().value_or(0) != 6 ||
            initialView.last().value_or(0) != 12)
        {
            std::cerr << "Initial span range did not cross the SDK bridge.\n";
            return EXIT_FAILURE;
        }

        valuesField.set_as(Values{10, 20, 30, 40, 50});
        windowField.set_as(wio::sdk::WioSpanRange{2u, 2u});

        auto valuesAfter = valuesField.get_as<Values>();
        const auto rangeAfter = windowField.get_as<wio::sdk::WioSpanRange>();
        const wio::sdk::WioSpan<const std::int32_t> viewAfter(
            valuesAfter.data(), valuesAfter.size(), rangeAfter
        );
        const wio::sdk::WioSpan<const std::int32_t> rejected(
            valuesAfter.data(), valuesAfter.size(), wio::sdk::WioSpanRange{99u, 4u}
        );
        if (rangeAfter.start() != 2u || rangeAfter.count() != 2u ||
            viewAfter.first().value_or(0) != 30 || viewAfter.last().value_or(0) != 40 ||
            !rejected.empty())
        {
            std::cerr << "Span range did not round-trip safely through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 span: start=" << rangeAfter.start()
                  << " count=" << rangeAfter.count()
                  << " values=" << viewAfter.first().value_or(0)
                  << '-' << viewAfter.last().value_or(0) << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
