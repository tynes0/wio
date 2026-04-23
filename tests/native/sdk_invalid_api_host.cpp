#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string_view>

#include <module_api.h>
#include <wio_sdk.h>

namespace
{
    std::int32_t gCounterValue = 0;

    std::int32_t InvokeCreate(const WioValue* args, std::uint32_t argCount, WioValue* outResult)
    {
        if (args != nullptr || argCount != 0u || outResult == nullptr)
            return WIO_INVOKE_BAD_ARGUMENTS;

        gCounterValue = 0;
        outResult->type = WIO_ABI_USIZE;
        outResult->value.v_usize = 123u;
        return WIO_INVOKE_OK;
    }

    std::int32_t InvokeDestroy(const WioValue* args, std::uint32_t argCount, WioValue* outResult)
    {
        if (args == nullptr || argCount != 1u || args[0].type != WIO_ABI_USIZE)
            return WIO_INVOKE_BAD_ARGUMENTS;

        if (outResult != nullptr)
            outResult->type = WIO_ABI_VOID;
        return WIO_INVOKE_OK;
    }

    std::int32_t InvokeFieldGetI32(const WioValue* args, std::uint32_t argCount, WioValue* outResult)
    {
        if (args == nullptr || argCount != 1u || args[0].type != WIO_ABI_USIZE || outResult == nullptr)
            return WIO_INVOKE_BAD_ARGUMENTS;

        outResult->type = WIO_ABI_I32;
        outResult->value.v_i32 = gCounterValue;
        return WIO_INVOKE_OK;
    }

    std::int32_t InvokeFieldSetI32(const WioValue* args, std::uint32_t argCount, WioValue* outResult)
    {
        if (args == nullptr || argCount != 2u || args[0].type != WIO_ABI_USIZE || args[1].type != WIO_ABI_I32)
            return WIO_INVOKE_BAD_ARGUMENTS;

        gCounterValue = args[1].value.v_i32;
        if (outResult != nullptr)
            outResult->type = WIO_ABI_VOID;
        return WIO_INVOKE_OK;
    }

    std::int32_t InvokeMethodAdd(const WioValue* args, std::uint32_t argCount, WioValue* outResult)
    {
        if (args == nullptr || argCount != 2u || args[0].type != WIO_ABI_USIZE || args[1].type != WIO_ABI_I32 || outResult == nullptr)
            return WIO_INVOKE_BAD_ARGUMENTS;

        outResult->type = WIO_ABI_I32;
        outResult->value.v_i32 = args[1].value.v_i32 + 9;
        return WIO_INVOKE_OK;
    }

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

    constexpr WioModuleTypeDescriptor kI32TypeDescriptor = {
        "i32",
        nullptr,
        WIO_MODULE_TYPE_DESC_PRIMITIVE,
        WIO_ABI_I32,
        0u,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0u,
        nullptr
    };

    constexpr WioAbiType kHandleParamTypes[] = { WIO_ABI_USIZE };
    constexpr WioAbiType kFieldSetterParamTypes[] = { WIO_ABI_USIZE, WIO_ABI_I32 };
    constexpr WioAbiType kMethodParamTypes[] = { WIO_ABI_USIZE, WIO_ABI_I32 };
    constexpr WioAbiType kBrokenMethodParamTypes[] = { WIO_ABI_I32, WIO_ABI_I32 };

    constexpr WioModuleExport kValidTypeExports[] = {
        { "Counter.__create", "WioCreateCounter", WIO_ABI_USIZE, 0u, nullptr, &InvokeCreate, nullptr },
        { "Counter.__destroy", "WioDestroyCounter", WIO_ABI_VOID, 1u, kHandleParamTypes, &InvokeDestroy, nullptr },
        { "Counter.value.get", "WioGetCounterValue", WIO_ABI_I32, 1u, kHandleParamTypes, &InvokeFieldGetI32, nullptr },
        { "Counter.value.set", "WioSetCounterValue", WIO_ABI_VOID, 2u, kFieldSetterParamTypes, &InvokeFieldSetI32, nullptr },
        { "Counter.Add", "WioCounterAddMethod", WIO_ABI_I32, 2u, kMethodParamTypes, &InvokeMethodAdd, nullptr }
    };

    constexpr WioModuleConstructor kValidTypeConstructors[] = {
        { &kValidTypeExports[0] }
    };

    constexpr WioModuleField kValidTypeFields[] = {
        { "value", WIO_ABI_I32, &kI32TypeDescriptor, WIO_MODULE_FIELD_READABLE | WIO_MODULE_FIELD_WRITABLE, WIO_MODULE_ACCESS_PUBLIC, &kValidTypeExports[2], &kValidTypeExports[3], nullptr, nullptr }
    };

    constexpr WioModuleMethod kValidTypeMethods[] = {
        { "Add", &kValidTypeExports[4] }
    };

    constexpr WioModuleType kValidCounterTypes[] = {
        { "Counter", "Counter", WIO_MODULE_TYPE_OBJECT, &kValidTypeExports[0], &kValidTypeExports[1], 1u, kValidTypeConstructors, 1u, kValidTypeFields, 1u, kValidTypeMethods }
    };

    constexpr WioModuleApi kValidTypeApi = {
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
        5u,
        kValidTypeExports,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        kValidCounterTypes
    };

    constexpr WioModuleExport kBrokenConstructorExports[] = {
        { "Counter.__create", "WioCreateCounter", WIO_ABI_I32, 0u, nullptr, &InvokeCreate, nullptr },
        { "Counter.__destroy", "WioDestroyCounter", WIO_ABI_VOID, 1u, kHandleParamTypes, &InvokeDestroy, nullptr },
        { "Counter.value.get", "WioGetCounterValue", WIO_ABI_I32, 1u, kHandleParamTypes, &InvokeFieldGetI32, nullptr },
        { "Counter.value.set", "WioSetCounterValue", WIO_ABI_VOID, 2u, kFieldSetterParamTypes, &InvokeFieldSetI32, nullptr },
        { "Counter.Add", "WioCounterAddMethod", WIO_ABI_I32, 2u, kMethodParamTypes, &InvokeMethodAdd, nullptr }
    };

    constexpr WioModuleConstructor kBrokenConstructorEntries[] = {
        { &kBrokenConstructorExports[0] }
    };

    constexpr WioModuleField kBrokenConstructorFields[] = {
        { "value", WIO_ABI_I32, &kI32TypeDescriptor, WIO_MODULE_FIELD_READABLE | WIO_MODULE_FIELD_WRITABLE, WIO_MODULE_ACCESS_PUBLIC, &kBrokenConstructorExports[2], &kBrokenConstructorExports[3], nullptr, nullptr }
    };

    constexpr WioModuleMethod kBrokenConstructorMethods[] = {
        { "Add", &kBrokenConstructorExports[4] }
    };

    constexpr WioModuleType kBrokenConstructorTypes[] = {
        { "Counter", "Counter", WIO_MODULE_TYPE_OBJECT, &kBrokenConstructorExports[0], &kBrokenConstructorExports[1], 1u, kBrokenConstructorEntries, 1u, kBrokenConstructorFields, 1u, kBrokenConstructorMethods }
    };

    constexpr WioModuleApi kBrokenConstructorApi = {
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
        5u,
        kBrokenConstructorExports,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        kBrokenConstructorTypes
    };

    constexpr WioModuleExport kBrokenFieldGetterExports[] = {
        { "Counter.__create", "WioCreateCounter", WIO_ABI_USIZE, 0u, nullptr, &InvokeCreate, nullptr },
        { "Counter.__destroy", "WioDestroyCounter", WIO_ABI_VOID, 1u, kHandleParamTypes, &InvokeDestroy, nullptr },
        { "Counter.value.get", "WioGetCounterValue", WIO_ABI_I32, 0u, nullptr, &InvokeFieldGetI32, nullptr },
        { "Counter.value.set", "WioSetCounterValue", WIO_ABI_VOID, 2u, kFieldSetterParamTypes, &InvokeFieldSetI32, nullptr },
        { "Counter.Add", "WioCounterAddMethod", WIO_ABI_I32, 2u, kMethodParamTypes, &InvokeMethodAdd, nullptr }
    };

    constexpr WioModuleConstructor kBrokenFieldGetterConstructors[] = {
        { &kBrokenFieldGetterExports[0] }
    };

    constexpr WioModuleField kBrokenFieldGetterFields[] = {
        { "value", WIO_ABI_I32, &kI32TypeDescriptor, WIO_MODULE_FIELD_READABLE | WIO_MODULE_FIELD_WRITABLE, WIO_MODULE_ACCESS_PUBLIC, &kBrokenFieldGetterExports[2], &kBrokenFieldGetterExports[3], nullptr, nullptr }
    };

    constexpr WioModuleMethod kBrokenFieldGetterMethods[] = {
        { "Add", &kBrokenFieldGetterExports[4] }
    };

    constexpr WioModuleType kBrokenFieldGetterTypes[] = {
        { "Counter", "Counter", WIO_MODULE_TYPE_OBJECT, &kBrokenFieldGetterExports[0], &kBrokenFieldGetterExports[1], 1u, kBrokenFieldGetterConstructors, 1u, kBrokenFieldGetterFields, 1u, kBrokenFieldGetterMethods }
    };

    constexpr WioModuleApi kBrokenFieldGetterApi = {
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
        5u,
        kBrokenFieldGetterExports,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        kBrokenFieldGetterTypes
    };

    constexpr WioModuleExport kBrokenMethodExports[] = {
        { "Counter.__create", "WioCreateCounter", WIO_ABI_USIZE, 0u, nullptr, &InvokeCreate, nullptr },
        { "Counter.__destroy", "WioDestroyCounter", WIO_ABI_VOID, 1u, kHandleParamTypes, &InvokeDestroy, nullptr },
        { "Counter.value.get", "WioGetCounterValue", WIO_ABI_I32, 1u, kHandleParamTypes, &InvokeFieldGetI32, nullptr },
        { "Counter.value.set", "WioSetCounterValue", WIO_ABI_VOID, 2u, kFieldSetterParamTypes, &InvokeFieldSetI32, nullptr },
        { "Counter.Add", "WioCounterAddMethod", WIO_ABI_I32, 2u, kBrokenMethodParamTypes, &InvokeMethodAdd, nullptr }
    };

    constexpr WioModuleConstructor kBrokenMethodConstructors[] = {
        { &kBrokenMethodExports[0] }
    };

    constexpr WioModuleField kBrokenMethodFields[] = {
        { "value", WIO_ABI_I32, &kI32TypeDescriptor, WIO_MODULE_FIELD_READABLE | WIO_MODULE_FIELD_WRITABLE, WIO_MODULE_ACCESS_PUBLIC, &kBrokenMethodExports[2], &kBrokenMethodExports[3], nullptr, nullptr }
    };

    constexpr WioModuleMethod kBrokenMethodMethods[] = {
        { "Add", &kBrokenMethodExports[4] }
    };

    constexpr WioModuleType kBrokenMethodTypes[] = {
        { "Counter", "Counter", WIO_MODULE_TYPE_OBJECT, &kBrokenMethodExports[0], &kBrokenMethodExports[1], 1u, kBrokenMethodConstructors, 1u, kBrokenMethodFields, 1u, kBrokenMethodMethods }
    };

    constexpr WioModuleApi kBrokenMethodApi = {
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
        5u,
        kBrokenMethodExports,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        kBrokenMethodTypes
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

        wio_validate_module_api(&kValidTypeApi);
        auto counterType = wio_load_object(&kValidTypeApi, "Counter");
        auto counter = counterType.create();
        counter.set("value", std::int32_t{ 11 });
        auto addMethod = counter.method<std::int32_t(std::int32_t)>("Add");
        const std::int32_t reflectedValue = counter.get<std::int32_t>("value");
        const std::int32_t methodValue = addMethod(5);
        if (reflectedValue != 11 || methodValue != 14)
        {
            std::cerr << "Valid exported type API binding did not invoke as expected."
                      << " value=" << reflectedValue
                      << " method=" << methodValue
                      << '\n';
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

        if (!expectInvalid("constructor signature", "must return 'usize'", []()
        {
            wio_validate_module_api(&kBrokenConstructorApi);
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("constructor signature through object loader", "must return 'usize'", []()
        {
            auto objectType = wio_load_object(&kBrokenConstructorApi, "Counter");
            (void)objectType;
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("field getter signature", "must declare 1 parameter", []()
        {
            wio_validate_module_api(&kBrokenFieldGetterApi);
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("field getter signature through object loader", "must declare 1 parameter", []()
        {
            auto objectType = wio_load_object(&kBrokenFieldGetterApi, "Counter");
            (void)objectType;
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("method signature", "parameter 0 must be 'usize'", []()
        {
            wio_validate_module_api(&kBrokenMethodApi);
        }))
        {
            return EXIT_FAILURE;
        }

        if (!expectInvalid("method signature through object loader", "parameter 0 must be 'usize'", []()
        {
            auto objectType = wio_load_object(&kBrokenMethodApi, "Counter");
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
