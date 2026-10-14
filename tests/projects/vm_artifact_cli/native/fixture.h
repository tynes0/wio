#pragma once

#include <filesystem>
#include <iostream>
#include <string>

namespace vm_project_artifact
{
    inline constexpr int Revision = 0;

    inline int Add(int left, int right)
    {
        return left + right + Revision;
    }

    inline bool CorrectDirectory()
    {
        return std::filesystem::current_path().filename() == "run cwd";
    }

    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_project_artifact
