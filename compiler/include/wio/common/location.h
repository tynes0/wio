#pragma once

#include <string>
#include <sstream>

namespace wio
{
    namespace common
    {
        struct Location
        {
            uint64_t line = 0;
            uint64_t column = 0;

            std::string toString() const
            {
                std::stringstream ss;
                ss << "Location: Line -> " << line << " Column -> " << column;
                return ss.str();
            }

            [[nodiscard]] bool isValid() const
            {
                return line != 0 && column != 0;
            }

            [[nodiscard]] operator bool() const
            {
                return isValid();
            }

            [[nodiscard]] static Location invalid()
            {
                return {
                    .line = 0,
                    .column = 0
                };
            }
        };
    }
}