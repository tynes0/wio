#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 Result host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        using IntResult = wio::sdk::WioResult<std::int32_t>;
        using UnitResult = wio::sdk::WioUnitResult;

        auto module = wio::sdk::Module::load(argv[1]);
        auto state = module.load_object("Sdk14Results").create();
        auto calculation = state.field("calculation");
        auto completion = state.field("completion");
        auto marker = state.field("marker");

        if (!calculation.can_access_as<IntResult>() ||
            !completion.can_access_as<UnitResult>() ||
            !marker.can_access_as<wio::sdk::WioUnit>() ||
            calculation.get_as<IntResult>().value_or(0) != 21 ||
            completion.get_as<UnitResult>().is_error() ||
            !(marker.get_as<wio::sdk::WioUnit>() == wio::sdk::WioUnit{}))
        {
            std::cerr << "Initial Result values did not cross the SDK bridge.\n";
            return EXIT_FAILURE;
        }

        const wio::sdk::WioResultError calculationError{
            wio::sdk::WioResultDomain::Custom,
            701,
            -9001,
            "host calculation failed"
        };
        calculation.set_as(IntResult::error(calculationError));

        const wio::sdk::WioResultError completionError{
            wio::sdk::WioResultDomain::Io,
            702,
            12345,
            "host completion failed"
        };
        completion.set_as(UnitResult::error(completionError));
        marker.set_as(wio::sdk::WioUnit{});

        const auto calculationAfter = calculation.get_as<IntResult>();
        const auto completionAfter = completion.get_as<UnitResult>();
        if (calculationAfter.is_ok() || completionAfter.is_ok() ||
            calculationAfter.error_value().domain != wio::sdk::WioResultDomain::Custom ||
            calculationAfter.error_value().code != 701 ||
            calculationAfter.error_value().native_code != -9001 ||
            calculationAfter.error_value().message != "host calculation failed" ||
            completionAfter.error_value().domain != wio::sdk::WioResultDomain::Io ||
            completionAfter.error_value().code != 702 ||
            completionAfter.error_value().native_code != 12345 ||
            completionAfter.error_value().message != "host completion failed" ||
            state.method<std::int32_t(std::int32_t)>("CalculationOr")(99) != 99 ||
            state.method<std::int32_t()>("CalculationCode")() != 701 ||
            state.method<bool()>("CompletionOk")() ||
            state.method<std::int32_t()>("CompletionCode")() != 702)
        {
            std::cerr << "Result error data did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        calculation.set_as(IntResult::ok(84));
        completion.set_as(UnitResult::ok(wio::sdk::WioUnit{}));
        if (state.method<std::int32_t(std::int32_t)>("CalculationOr")(0) != 84 ||
            !state.method<bool()>("CompletionOk")())
        {
            std::cerr << "Result success data did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 Result: value=84 error-code=" << calculationError.code
                  << " unit=ok native-error=" << completionError.native_code << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
