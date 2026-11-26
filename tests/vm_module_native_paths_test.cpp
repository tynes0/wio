#include <wio/vm/module_session.h>

#include <array>
#include <fstream>
#include <iostream>

namespace
{
    unsigned char anchor = 1;
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    template <class F> void rejects(F action)
    {
        bool failed = false;
        try
        {
            action();
        }
        catch (const std::exception&)
        {
            failed = true;
        }
        expect(failed, "invalid native dependency path was accepted");
    }
    struct Files
    {
        std::filesystem::path directory, cwd;
        ~Files()
        {
            std::error_code error;
            std::filesystem::current_path(cwd, error);
            std::filesystem::remove_all(directory, error);
        }
    };
    wio::bytecode::Module fixture()
    {
        wio::bytecode::Module module;
        module.moduleKind = WIO_SDK_MODULE_WIO_LIBRARY;
        module.abiDescriptorVersion = 11;
        module.strings = {"native-path-test"};
        module.name = module.logicalName = module.stableKey = 0;
        module.stableId = WioStableSdkId(module.strings[0].c_str());
        module.contract.callTableStableId = 1;
        module.types = {wio::bytecode::Type{.kind = 1}};
        module.constants = {wio::bytecode::Constant{}};
        return module;
    }
} // namespace

int main(int argc, char* argv[])
{
    try
    {
        expect(argc > 0, "executable path missing");
        const auto image = std::filesystem::canonical(std::filesystem::absolute(argv[0])).parent_path();
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto relative = std::filesystem::u8path("native paths \xf0\x9f\x9a\x80 " + std::to_string(nonce));
        Files files{image / relative, std::filesystem::current_path()};
        std::filesystem::create_directories(files.directory);
        const auto native = files.directory / "producer.dll";
        std::ofstream{native, std::ios::binary} << "not a native image";
        const std::array names{relative / "producer.dll", relative / "producer.dll"};
        std::filesystem::current_path(std::filesystem::temp_directory_path());
        const auto resolved = wio::vm::moduleNativeLibraries(&anchor, names);
        expect(resolved.size() == 1 && resolved[0] == std::filesystem::canonical(native),
               "native path resolution used cwd or lost duplicate/Unicode identity");
        rejects([&] { (void)wio::vm::moduleNativeLibraries(nullptr, names); });
        rejects([&] { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{native}); });
        rejects(
            [&]
            { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{std::filesystem::path{"../producer.dll"}}); });
        rejects([&] { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{relative / "." / "producer.dll"}); });
        rejects([&] { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{std::filesystem::path{}}); });
        rejects(
            [&]
            {
                (void)wio::vm::moduleNativeLibraries(&anchor,
                                                     std::array{std::filesystem::path{std::string{"x\0y", 3}}});
            });
        rejects([&] { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{relative}); });
        rejects([&] { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{relative / "missing.dll"}); });
        expect(wio::vm::moduleNativeLibraries(nullptr, {}).empty(), "pure module unexpectedly needed an image anchor");
        // Link to a directory outside the executable image root. Windows may
        // deny symlink creation without Developer Mode; do not claim that case.
        std::error_code linkError;
        std::filesystem::create_directory_symlink(image.parent_path(), files.directory / "outside", linkError);
        if (!linkError)
            rejects([&] { (void)wio::vm::moduleNativeLibraries(&anchor, std::array{relative / "outside"}); });
        else
            std::cout << "native-path-symlink-case-unavailable\n";
        rejects([&] { wio::vm::ModuleSessionFactory factory{fixture(), {}, {relative / "producer.dll"}}; });
        wio::vm::ModuleSessionFactory factory{fixture(), {}, resolved};
        rejects([&] { (void)factory.defaultContract(); });
        WioSdkModuleSession session{};
        expect(factory.api().open(factory.api().factory, &session) != WIO_NATIVE_ABI_OK && !session.context &&
                   !session.contract,
               "failed native load published a partial session token");
        expect(factory.api().close(&session) == WIO_NATIVE_ABI_OK, "empty failed session was not safely closable");
        std::cout << "module-native-paths-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
