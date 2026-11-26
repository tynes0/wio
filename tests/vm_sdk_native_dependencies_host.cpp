#include <wio_native_sdk.h>

#include <array>
#include <iostream>

extern "C" const WioSdkModuleSessionApi* WioStaticNativeContractSessionApi() noexcept;
extern "C" const WioSdkModuleTaskApi* WioStaticNativeContractTaskApi() noexcept;
extern "C" const WioSdkModuleContract* WioStaticNativeContract() noexcept;

namespace
{
    using Host = wio::sdk::NativeModule;
    using Value = wio::sdk::NativeValue;
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    template <class F> void rejects(F action)
    {
        bool rejected = false;
        try
        {
            action();
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        expect(rejected, "invalid native-dependent session was accepted");
    }
    struct Counters
    {
        wio::sdk::detail::LibraryHandle handle{};
        using Query = int (*)();
        Query loads{}, unloads{}, shutdowns{};
        explicit Counters(const std::filesystem::path& path)
        {
            handle = wio::sdk::detail::openLibrary(path);
            if (!handle)
            {
                const auto utf8 = path.u8string();
                throw std::runtime_error(
                    "deployed native producer/private dependency did not load: " + std::string{utf8.begin(), utf8.end()}
#if defined(_WIN32)
                    + " (Windows error " + std::to_string(GetLastError()) + ")"
#endif
                );
            }
            loads = wio::sdk::detail::loadSymbol<Query>(handle, "WioDeploymentLoads");
            unloads = wio::sdk::detail::loadSymbol<Query>(handle, "WioDeploymentUnloads");
            shutdowns = wio::sdk::detail::loadSymbol<Query>(handle, "WioDeploymentShutdowns");
            expect(loads && unloads && shutdowns, "native lifecycle counters missing");
        }
        ~Counters()
        {
            wio::sdk::detail::closeLibrary(handle);
        }
        void close()
        {
            wio::sdk::detail::closeLibrary(std::exchange(handle, {}));
        }
    };
    struct MissingFile
    {
        std::filesystem::path path, offline;
        explicit MissingFile(std::filesystem::path original) : path(std::move(original)), offline(path)
        {
            offline += ".offline";
            std::filesystem::rename(path, offline);
        }
        ~MissingFile()
        {
            std::error_code error;
            std::filesystem::rename(offline, path, error);
        }
    };

    template <class F> void qualify(F open, const std::filesystem::path& producer)
    {
        Counters native{producer};
        auto first = open();
        auto second = open();
        expect(native.loads() == 0, "opening a session executed user Load");
        std::array numbers{Value::integer(40), Value::integer(2)};
        rejects([&] { first.invoke("Add", numbers); });
        first.start();
        first.start();
        second.start();
        expect(native.loads() == 2 && first.invoke("Add", numbers).asInteger() == 42 + WIO_DEPLOY_BIAS,
               "declared native registry/private library did not execute");
        std::array strings{Value::string(std::string{"A\0B", 3})};
        auto escaped = first.invoke("Message", strings);
        first.stop();
        expect(native.unloads() == 1 && native.shutdowns() == 0,
               "one session shut down a native producer used by another session");
        expect(second.invoke("Add", numbers).asInteger() == 42 + WIO_DEPLOY_BIAS,
               "surviving native session lost registration/code");
        second.stop();
        expect(native.unloads() == 2 && native.shutdowns() == 1 && escaped.asString() == std::string{"native:A\0B", 10},
               "last-session shutdown/lifecycle/result storage was not balanced");
        first.close();
        second.close();
        native.close();
        {
            MissingFile missing{producer};
            rejects([&] { open(); });
        }
        auto recovered = open();
        recovered.start();
        expect(recovered.invoke("Add", numbers).asInteger() == 42 + WIO_DEPLOY_BIAS,
               "session could not recover after a failed dependency open");
        recovered.stop();
        recovered.close();
        expect(escaped.asString() == std::string{"native:A\0B", 10}, "producer unload invalidated a copied result");
    }

    int run(const std::vector<std::filesystem::path>& args)
    {
        try
        {
            expect(args.size() == 6, "deployment host requires shared/static/bad producer paths");
            // Native paths must come from the module image, not this directory.
            std::filesystem::current_path(std::filesystem::temp_directory_path());
            qualify([&] { return Host::open(args[1]); }, args[2]);
            auto lifetime = std::make_shared<int>(0);
            qualify(
                [&]
                {
                    const auto* sessions = WioStaticNativeContractSessionApi();
                    const auto* tasks = WioStaticNativeContractTaskApi();
                    expect(sessions && WioValidateSdkModuleTaskApi(tasks, sessions),
                           "static native session getters invalid");
                    expect(WioStaticNativeContract() == nullptr,
                           "native-dependent module admitted a legacy static instance");
                    return Host::attachSession(*sessions, lifetime, tasks);
                },
                args[3]);
            Counters partial{args[5]};
            rejects([&] { Host::open(args[4]); });
            expect(partial.loads() == 0 && partial.unloads() == 0 && partial.shutdowns() == 1,
                   "partial dependency failure published a session or failed to roll back native registration");
            std::cout << "vm-sdk-native-dependencies-ok\n";
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
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i]);
    return run(args);
}
#else
int main(int argc, char* argv[])
{
    std::vector<std::filesystem::path> args;
    for (int i = 0; i < argc; ++i)
        args.push_back(std::filesystem::u8path(argv[i]));
    return run(args);
}
#endif
