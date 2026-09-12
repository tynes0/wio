#pragma once
#include <string>
#include <string_view>

namespace wio::codegen {
struct WirCppText {
    static std::string quote(std::string_view value) {
        std::string result = "\"";
        for (unsigned char c : value) {
            if (c == '"' || c == '\\') {
                result += '\\';
                result += static_cast<char>(c);
            } else if (c < 32 || c == 127) {
                result += '\\';
                result += static_cast<char>('0' + ((c >> 6) & 7));
                result += static_cast<char>('0' + ((c >> 3) & 7));
                result += static_cast<char>('0' + (c & 7));
            } else
                result += static_cast<char>(c);
        }
        return result + '"';
    }
};
} // namespace wio::codegen
