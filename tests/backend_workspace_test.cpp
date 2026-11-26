#include "wio/common/filesystem/backend_workspace.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int main(int argc, char* argv[])
{
    namespace fs = std::filesystem;
    try
    {
        expect(argc == 2, "workspace test needs an output directory");
        const auto root = fs::absolute(fs::u8path(argv[1])) /
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto unicode = root / fs::u8path(u8"cwd Türkçe 🚀");
        fs::create_directories(unicode);
        const auto previous = fs::current_path();
        fs::current_path(unicode);
        fs::path ownedDirectory;
        {
            wio::common::filesystem::BackendWorkspace workspace;
            ownedDirectory = workspace.directory();
            const auto output = workspace.file("object", 7);
            expect(output.is_relative(), "Unicode cwd must use an ASCII relative tool path");
            for (const auto value : output.native())
                expect(value > 0 && value < 128, "external tool output path is not ASCII");
            std::ofstream(output) << "owned object";
            expect(fs::is_regular_file(output), "relative workspace output does not resolve in original cwd");
            wio::common::filesystem::BackendWorkspace second;
            expect(second.directory() != ownedDirectory, "concurrent workspaces collided");
            bool rejected = false;
            try
            {
                workspace.file("../escape");
            }
            catch (const std::exception&)
            {
                rejected = true;
            }
            expect(rejected && !fs::exists(unicode / "escape-0.tmp"), "workspace allowed a nonliteral path stem");
        }
        expect(!fs::exists(ownedDirectory), "owned workspace was not cleaned");
        fs::path retained;
        {
            wio::common::filesystem::BackendWorkspace workspace(unicode);
            retained = workspace.directory();
            std::ofstream(workspace.file("artifact")) << "owned image";
            std::ofstream(retained / "user-side-output") << "must survive cleanup";
        }
        expect(fs::is_regular_file(retained / "user-side-output") && !fs::exists(retained / "artifact-0.tmp"),
               "cleanup deleted unknown output or retained its own image");
        fs::current_path(previous);
        std::cout << "Backend workspace ASCII paths, collision isolation and bounded cleanup passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
