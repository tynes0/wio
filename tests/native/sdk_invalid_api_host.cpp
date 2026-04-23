#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>

#include <module_api.h>
#include <wio_sdk.h>

namespace
{
    std::int32_t InvokeAdd(const WioValue* args, std::uint32_t argCount, WioValue* outResult)
    {
        if (args == nullptr || argCount != 1u || outResult == nullptr)
            return WIO_INVOKE_BAD_ARGUMENTS;

        if (args[0].type != WIO_ABI_I32)
            return WIO_INVOKE_TYPE_MISMATCH;

        outResult->type = WIO_ABI_I32;
        outResult->value.v_i32 = args[0].value.v_i32 + 1;
        return WIO_INVOKE_OK;
    }

    constexpr WioAbiType kAddParamTypes[] = { WIO_ABI_I32 };
    constexpr WioModuleExport kValidExports[] = {
        { "counter.add", "WioCounterAdd", WIO_ABI_I32, 1u, kAddParamTypes, &InvokeAdd, nullptr }
    };

    constexpr WioModuleCommand kValidCommands[] = {
        { "counter.add", &kValidExports[0] }
    };

    constexpr WioModuleApi kValidApi = {
        WIO_MODULE_API_DESCRIPTOR_VERSION,
        0u,
        0u,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        1u,
        kValidExports,
        1u,
        kValidCommands,
        0u,
        nullptr,
        0u,
        nullptr
    };

    constexpr WioModuleExport kMissingParamTypeExports[] = {
        { "counter.add", "WioCounterAdd", WIO_ABI_I32, 1u, nullptr, &InvokeAdd, nullptr }
    };

    constexpr WioModuleApi kMissingParamTypeApi = {
        WIO_MODULE_API_DESCRIPTOR_VERSION,
        0u,
        0u,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        1u,
        kMissingParamTypeExports,
        0u,
        nullptr,
        0u,
        nullptr,
        0u,
        nullptr
    };

    constexpr WioModuleExport kDetachedExport = {
        "counter.orphan",
        "WioCounterOrphan",
        WIO_ABI_I32,
        1u,
        kAddParamTypes,
        &InvokeAdd,
        nullptr
    };

    constexpr WioModuleCommand kBrokenCommands[] = {
        { "counter.orphan", &kDetachedExport }
    };

    constexpr WioModuleApi kBrokenCommandApi = {
        WIO_MODULE_API_DESCRIPTOR_VERSION,
        0u,
        0u,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        1u,
        kValidExports,
        1u,
        kBrokenCommands,
        0u,
        nullptr,
        0u,
        nullptr
    };

    constexpr WioModuleField kBrokenFields[] = {
        { "value", WIO_ABI_I32, nullptr, WIO_MODULE_FIELD_READABLE, WIO_MODULE_ACCESS_PUBLIC, &kValidExports[0], nullptr, nullptr, nullptr }
    };

    constexpr WioModuleType kBrokenTypes[] = {
        { "BrokenCounter", "BrokenCounter", WIO_MODULE_TYPE_OBJECT, nullptr, &kValidExports[0], 0u, nullptr, 1u, kBrokenFields, 0u, nullptr }
    };

    constexpr WioModuleApi kBrokenTypeApi = {
        WIO_MODULE_API_DESCRIPTOR_VERSION,
        0u,
        0u,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        1u,
        kValidExports,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        kBrokenTypes
    };

    template <typename TProbe>
    bool expectInvalid(std::string_view label, std::string_view fragment, TProbe&& probe)
    {
        try
        {
            probe();
            std::cerr << "Expected invalid API rejection for " << label << '\n';
            return false;
        }
        catch (const wio::sdk::Error& error)
        {
            if (error.code() != wio::sdk::ErrorCode::InvalidApiDescriptor)
            {
                std::cerr << "Wrong error code for " << label << ": " << error.what() << '\n';
                return false;
            }

            if (std::string_view(error.what()).find(fragment) == std::string_view::npos)
            {
                std::cerr << "Missing diagnostic fragment for " << label << ": " << error.what() << '\n';
                return false;
            }

            return true;
        }
    }
}

int main()
{
    try
    {
        wio_validate_module_api(&kValidApi);
        auto addCounter = wio_load_function<std::function<std::int32_t(std::int32_t)>>(&kValidApi, "counter.add");
        if (addCounter(41) != 42)
        {
            std::cerr << "Valid API binding did not invoke as expected." << '\n';
            return EXIT_FAILURE;
        }

        if (!expectInvalid("missing export parameterTypes", "parameterTypes", []()
        {
            wio_validate_module_api(&kMissingParamTypeApi);
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("missing export parameterTypes through helper", "parameterTypes", []()
        {
            auto add = wio_load_function<std::function<std::int32_t(std::int32_t)>>(&kMissingParamTypeApi, "counter.add");
            (void)add;
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("command export ownership", "outside the module export table", []()
        {
            wio_validate_module_api(&kBrokenCommandApi);
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("command export ownership through helper", "outside the module export table", []()
        {
            auto command = wio_load_command<std::function<std::int32_t(std::int32_t)>>(&kBrokenCommandApi, "counter.orphan");
            (void)command;
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("field type descriptor", "missing a type descriptor", []()
        {
            wio_validate_module_api(&kBrokenTypeApi);
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("field type descriptor through object loader", "missing a type descriptor", []()
        {
            auto objectType = wio_load_object(&kBrokenTypeApi, "BrokenCounter");
            (void)objectType;
        }))
        {
            return EXIT_FAILURE;
        }

        std::cout << "SDK invalid API validation: ok";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
