#include "wio/common/filesystem/filesystem.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

int main()
{
    namespace fs = std::filesystem;
    namespace wfs = wio::common::filesystem;

    if (wfs::writeFileBase(nullptr, "data"))
    {
        std::cerr << "writeFileBase must report a null FILE as false\n";
        return 1;
    }

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("wio-filesystem-policy-" + std::to_string(unique));
    std::error_code cleanupError;

    if (!wfs::createDirectories(root) || !wfs::createDirectories(root))
    {
        std::cerr << "createDirectories must be idempotent\n";
        fs::remove_all(root, cleanupError);
        return 1;
    }

    const fs::path emptyFile = root / "empty.wio";
    FILE* file = wfs::openFile(emptyFile, "wb");
    if (!file)
    {
        std::cerr << "failed to create temporary test file\n";
        fs::remove_all(root, cleanupError);
        return 1;
    }
    std::fclose(file);

    if (!wfs::readFile(emptyFile).empty() ||
        !wfs::readFile(root / "missing.wio").empty() ||
        !wfs::listFiles(root / "missing", true).empty())
    {
        std::cerr << "read/list operational failures must return an empty result\n";
        fs::remove_all(root, cleanupError);
        return 1;
    }

    if (wfs::writeFilepath(std::string("data"), root / "missing" / "output.txt"))
    {
        std::cerr << "writeFilepath must report an open failure as false\n";
        fs::remove_all(root, cleanupError);
        return 1;
    }

    if (wfs::getAbsPath(root).empty())
    {
        std::cerr << "getAbsPath must return either the absolute path or its input fallback\n";
        fs::remove_all(root, cleanupError);
        return 1;
    }

    fs::remove_all(root, cleanupError);
    return cleanupError ? 1 : 0;
}
