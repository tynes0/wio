#pragma once

#include <string>
#include <vector>
#include "wio/sema/type.h"

namespace wio::codegen
{
    class Mangler
    {
    public:
        static std::string mangleFunction(const std::string& name, const std::vector<Ref<sema::Type>>& paramTypes, const std::string& scopePath = "");
        
        static std::string mangleStruct(const std::string& name, const std::string& scopePath = "");
        
        static std::string mangleGlobalVar(const std::string& name, const std::string& scopePath = "");
        
        static std::string mangleType(const Ref<sema::Type>& type);

        static std::string mangleInterface(const std::string& name, const std::string& scopePath = "");
    };
}