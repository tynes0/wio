#pragma once

#include <string>
#include <string_view>

namespace wio::codegen::cpp_identifier
{
    bool isCppReservedIdentifier(std::string_view identifier);
    bool isValidCppIdentifier(std::string_view identifier);
    bool isValidCppSymbolPath(std::string_view symbolPath, bool allowQualified);
    std::string sanitizeCppIdentifier(std::string_view identifier);
    void replaceCppIdentifier(std::string& value, std::string_view from, std::string_view to);
}
