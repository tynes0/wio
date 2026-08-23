#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 tuple host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        using Selected = wio::sdk::WioOption<std::int32_t>;
        using Status = wio::sdk::WioResult<std::string>;
        using Snapshot = wio::sdk::WioTuple<Selected, Status, wio::sdk::WioU64>;

        auto module = wio::sdk::Module::load(argv[1]);
        auto profile = module.load_object("Sdk14TupleProfile").create();
        auto field = profile.field("snapshot");
        if (!field.can_access_as<Snapshot>())
        {
            std::cerr << "Tuple descriptor does not match its SDK value.\n";
            return EXIT_FAILURE;
        }

        const auto initial = field.get_as<Snapshot>();
        if (std::get<0>(initial).value_or(0) != 7 ||
            std::get<1>(initial).is_error() || std::get<1>(initial).value() != "ready" ||
            std::get<2>(initial).value() != 42u)
        {
            std::cerr << "Initial tuple did not cross the SDK bridge.\n";
            return EXIT_FAILURE;
        }

        wio::sdk::WioResultError hostError{
            wio::sdk::WioResultDomain::Custom,
            730,
            991,
            "host tuple error"
        };
        field.set_as(Snapshot{
            Selected::none(),
            Status::error(hostError),
            wio::sdk::WioU64{9000000000ull}
        });

        const auto after = field.get_as<Snapshot>();
        if (std::get<0>(after).is_some() || !std::get<1>(after).is_error() ||
            std::get<1>(after).error_value().code != 730 ||
            std::get<1>(after).error_value().native_code != 991 ||
            std::get<2>(after).value() != 9000000000ull)
        {
            std::cerr << "Tuple did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 tuple: selected=none error="
                  << std::get<1>(after).error_value().code
                  << " revision=" << std::get<2>(after).value() << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
