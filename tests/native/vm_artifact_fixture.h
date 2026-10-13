#pragma once
#include <iostream>
#include <string>
namespace vm_artifact_fixture
{
    inline int Add(int a, int b)
    {
        return a + b;
    }
    inline void Print(std::string message)
    {
        std::cout << message << std::endl;
    }
} // namespace vm_artifact_fixture
