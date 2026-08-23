#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 nested collections host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        using IntOption = wio::sdk::WioOption<std::int32_t>;
        using IntResult = wio::sdk::WioResult<std::int32_t>;
        using WideOption = wio::sdk::WioOption<wio::sdk::WioU64>;
        using Choices = std::vector<IntOption>;
        using Fixed = std::array<IntResult, 2>;
        using Lookup = std::unordered_map<std::string, WideOption>;
        using Ordered = std::map<std::string, IntResult>;

        auto module = wio::sdk::Module::load(argv[1]);
        auto state = module.load_object("Sdk14NestedCollections").create();

        auto choicesField = state.field("choices");
        auto fixedField = state.field("fixed");
        auto lookupField = state.field("lookup");
        auto orderedField = state.field("ordered");
        if (!choicesField.can_access_as<Choices>() || !fixedField.can_access_as<Fixed>() ||
            !lookupField.can_access_as<Lookup>() || !orderedField.can_access_as<Ordered>())
        {
            std::cerr << "Nested collection descriptors do not match their SDK values.\n";
            return EXIT_FAILURE;
        }

        const auto initialChoices = choicesField.get_as<Choices>();
        const auto initialFixed = fixedField.get_as<Fixed>();
        const auto initialLookup = lookupField.get_as<Lookup>();
        const auto initialOrdered = orderedField.get_as<Ordered>();
        if (initialChoices.size() != 3u || initialChoices[0].value_or(0) != 1 ||
            initialChoices[1].is_some() || initialChoices[2].value_or(0) != 3 ||
            initialFixed[0].value_or(0) != 4 || !initialFixed[1].is_error() ||
            initialFixed[1].error_value().code != 405 ||
            initialLookup.at("wide").value().value() != 14000000000ull ||
            initialLookup.at("missing").is_some() ||
            initialOrdered.at("first").value_or(0) != 7 ||
            initialOrdered.at("second").error_value().message != "ordered failure")
        {
            std::cerr << "Initial nested collections did not cross the SDK bridge.\n";
            return EXIT_FAILURE;
        }

        Choices choices{ IntOption::none(), IntOption::some(11) };
        Fixed fixed{
            IntResult::error({wio::sdk::WioResultDomain::Runtime, 501, -51, "host fixed"}),
            IntResult::ok(12)
        };
        Lookup lookup{
            {"host", WideOption::some(wio::sdk::WioU64{18000000000ull})},
            {"none", WideOption::none()}
        };
        Ordered ordered{
            {"alpha", IntResult::ok(13)},
            {"omega", IntResult::error({wio::sdk::WioResultDomain::Custom, 502, 52, "host ordered"})}
        };

        choicesField.set_as(std::move(choices));
        fixedField.set_as(std::move(fixed));
        lookupField.set_as(std::move(lookup));
        orderedField.set_as(std::move(ordered));

        const auto choicesAfter = choicesField.get_as<Choices>();
        const auto fixedAfter = fixedField.get_as<Fixed>();
        const auto lookupAfter = lookupField.get_as<Lookup>();
        const auto orderedAfter = orderedField.get_as<Ordered>();
        if (choicesAfter.size() != 2u || choicesAfter[0].is_some() || choicesAfter[1].value_or(0) != 11 ||
            !fixedAfter[0].is_error() || fixedAfter[0].error_value().native_code != -51 || fixedAfter[1].value_or(0) != 12 ||
            lookupAfter.at("host").value().value() != 18000000000ull || lookupAfter.at("none").is_some() ||
            orderedAfter.at("alpha").value_or(0) != 13 || orderedAfter.at("omega").error_value().code != 502)
        {
            std::cerr << "Nested collections did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 nested: choices=" << choicesAfter.size()
                  << " fixed-error=" << fixedAfter[0].error_value().code
                  << " wide=" << lookupAfter.at("host").value().value()
                  << " ordered-error=" << orderedAfter.at("omega").error_value().code << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
