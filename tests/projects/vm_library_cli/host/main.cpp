#include <wio_native_sdk.h>

#include <array>
#include <iostream>

#if defined(WIO_TEST_STATIC)
extern "C" const WioSdkModuleSessionApi* WioCliStaticContractSessionApi() noexcept;
extern "C" const WioSdkModuleTaskApi* WioCliStaticContractTaskApi() noexcept;
#endif

namespace
{
    int run(const std::vector<std::filesystem::path>& args)
    {
        try
        {
            using Host = wio::sdk::NativeModule;
            using Value = wio::sdk::NativeValue;
#if defined(WIO_TEST_STATIC)
            if (args.size() != 3)
                throw std::runtime_error("static VM host argument forwarding failed");
            const auto* sessions = WioCliStaticContractSessionApi();
            const auto* tasks = WioCliStaticContractTaskApi();
            if (!sessions || !tasks)
                throw std::runtime_error("static VM getters missing");
            auto module = Host::attachSession(*sessions, std::make_shared<int>(0), tasks);
            constexpr std::size_t argument = 1;
#else
            if (args.size() != 4)
                throw std::runtime_error("shared VM host argument forwarding failed");
            auto module = Host::open(args[1]);
            constexpr std::size_t argument = 2;
#endif
            if (args[argument] != "manifest-arg" || args[argument + 1] != "two words")
                throw std::runtime_error("VM host received incorrect arguments");
            std::array numbers{Value::integer(40)};
            if (module.invoke("Answer", numbers).asInteger() != 42)
                throw std::runtime_error("VM export/native bridge answer mismatch");
            const std::string bytes{"A\0B", 3};
            std::array strings{Value::string(bytes)};
            auto escaped = module.invoke("Echo", strings);
            module.close();
            if (escaped.asString() != bytes)
                throw std::runtime_error("VM result ownership lost after close");
            std::cout << "vm-library-cli-ok\n";
            return 0;
        }
        catch (const std::exception& error)
        {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t* argv[])
{
    std::vector<std::filesystem::path> args;
    for (int index = 0; index < argc; ++index)
        args.emplace_back(argv[index]);
    return run(args);
}
#else
int main(int argc, char* argv[])
{
    std::vector<std::filesystem::path> args;
    for (int index = 0; index < argc; ++index)
        args.push_back(std::filesystem::u8path(argv[index]));
    return run(args);
}
#endif
