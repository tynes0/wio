#include <wio_native_sdk.h>

#include <iostream>
#include <thread>

extern "C" const WioSdkModuleContract* WioStaticTestModule() noexcept;
extern "C" const WioSdkModuleContract* WioStaticSecondModule() noexcept;

namespace
{
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void qualify(const wio::sdk::NativeModule& module)
    {
        using wio::sdk::NativeValue;
        std::vector<NativeValue> args;
        args.push_back(NativeValue::integer(17));
        args.push_back(NativeValue::integer(25));
        expect(module.invoke("Add", args).asInteger() == 42, "module addition failed");
        const auto* contract = module.contract();
        bool generic = false;
        for (std::uint32_t i = 0; i < contract->exportCount; ++i)
        {
            const auto& entry = contract->exports[i];
            if (entry.kind == WIO_SDK_EXPORT_GENERIC_FUNCTION_SPECIALIZATION)
            {
                args.resize(1);
                expect(module.invoke(entry.stableId, args).asInteger() == 17, "generic specialization failed");
                generic = true;
            }
        }
        expect(generic, "concrete generic export identity missing");
        args.clear();
        args.push_back(NativeValue::integer(1));
        expect(module.invoke("EnumIdentity", args).asInteger() == 1, "enum wire identity failed");
        args.clear();
        args.push_back(NativeValue::unsignedInteger(3));
        expect(module.invoke("FlagIdentity", args).raw().payload.unsignedInteger == 3, "flagset wire identity failed");
        expect(module.invoke("Unit").raw().kind == WIO_NATIVE_ABI_VOID, "void export failed");
        args.clear();
        args.push_back(NativeValue::text(U"\U0001f680"));
        expect(module.invoke("Unicode", args).asText() == U"\u27e6\U0001f680\u27e7", "Unicode module export failed");
        bool failed = false;
        try
        {
            module.invoke("Fail");
        }
        catch (const std::runtime_error&)
        {
            failed = true;
        }
        expect(failed, "VM panic crossed SDK as success");
        args.clear();
        args.push_back(NativeValue::integer(1));
        args.push_back(NativeValue::integer(2));
        expect(module.invoke("Add", args).asInteger() == 3, "module did not recover after failed export");
        std::int32_t status = 0;
        const auto& call = contract->callTable[0];
        std::thread foreign{[&]
                            {
                                WioNativeAbiValue result{};
                                status = call.invoke(nullptr, 0, &result, call.context);
                            }};
        foreign.join();
        expect(status == WIO_NATIVE_ABI_WRONG_THREAD, "shared/static module accepted foreign entry");
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc == 2, "shared module path required");
        auto shared = wio::sdk::NativeModule::open(std::filesystem::u8path(argv[1]));
        qualify(shared);
        const auto* first = WioStaticTestModule();
        const auto* second = WioStaticSecondModule();
        expect(first && second && first != second, "multiple static modules did not own separate machines");
        // Both static getters and their code live until process shutdown.
        auto processLifetime = std::make_shared<int>(0);
        auto one = wio::sdk::NativeModule::attach(*first, processLifetime);
        auto two = wio::sdk::NativeModule::attach(*second, processLifetime);
        qualify(one);
        qualify(two);
        std::vector<wio::sdk::NativeValue> args;
        args.push_back(wio::sdk::NativeValue::string(std::string{"A\0B", 3}));
        auto escaped = shared.invoke("Echo", args);
        shared.close();
        expect(escaped.asString() == std::string{"VM:A\0B", 6}, "shared module result was not byte-exact or retained");
        auto clone = escaped.clone();
        escaped.reset();
        expect(clone.asString() == std::string{"VM:A\0B", 6}, "owning shared result clone lost library lifetime");
        clone.reset();
        std::cout << "vm-sdk-module-artifacts-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
