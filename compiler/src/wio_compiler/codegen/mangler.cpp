#include "wio/codegen/mangler.h"
#include <algorithm>

namespace wio::codegen
{
    using namespace sema;

    std::string Mangler::mangleType(const Ref<sema::Type>& type)
    {
        if (!type) return "unknown";
        std::string typeStr = type->toString();
        
        std::ranges::replace(typeStr, ' ', '_');
        std::ranges::replace(typeStr, '[', 'A');
        std::ranges::replace(typeStr, ']', 'E');
        std::ranges::replace(typeStr, ';', 'S');
        std::ranges::replace(typeStr, '*', 'P');
        std::ranges::replace(typeStr, '&', 'R');
        
        return typeStr;
    }

    std::string Mangler::mangleInterface(const std::string& scopePath, const std::string& name)
    {
        std::string mangled = "_WI_"; // Wio Interface
        if (!scopePath.empty()) mangled += scopePath + "_";
        mangled += name;
        return mangled;
    }

    std::string Mangler::mangleFunction(const std::string& name, const std::vector<Ref<sema::Type>>& paramTypes, const std::string& scopePath)
    {
        if (name == "main" && scopePath.empty()) return "main";

        std::string mangled = "_WF_"; // Wio Function
        if (!scopePath.empty()) mangled += scopePath + "_";
        mangled += name;
        
        for (const auto& type : paramTypes)
        {
            mangled += "_" + mangleType(type);
        }
        
        return mangled;
    }

    std::string Mangler::mangleStruct(const std::string& name, const std::string& scopePath)
    {
        std::string mangled = "_WS_"; // Wio Struct
        if (!scopePath.empty()) mangled += scopePath + "_";
        mangled += name;
        return mangled;
    }

    std::string Mangler::mangleGlobalVar(const std::string& name, const std::string& scopePath)
    {
        std::string mangled = "_WG_"; // Wio Global Variable
        if (!scopePath.empty()) mangled += scopePath + "_";
        mangled += name;
        return mangled;
    }
}