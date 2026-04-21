#pragma once

#include <string>
#include <sstream>

namespace wio
{
    namespace common
    {
        struct Location
        {
            std::string file;
            uint64_t line = 0;
            uint64_t column = 0;

            std::string toString() const
            {
                std::stringstream ss;
                ss << "Location: ";
                if (!file.empty())
                    ss << "File -> " << file << " ";
                ss << "Line -> " << line << " Column -> " << column;
                return ss.str();
            }

            std::string toDiagnosticString() const
            {
                std::stringstream ss;

                if (!file.empty())
                {
                    ss << file;
                    if (line != 0)
                    {
                        ss << ":" << line;
                        if (column != 0)
                            ss << ":" << column;
                    }
                    return ss.str();
                }

                if (line != 0)
                {
                    ss << "Line " << line;
                    if (column != 0)
                        ss << ", Col " << column;
                    return ss.str();
                }

                return "unknown";
            }

            [[nodiscard]] bool hasFile() const
            {
                return !file.empty();
            }

            [[nodiscard]] bool hasSourceContext() const
            {
                return hasFile() && line != 0;
            }

            [[nodiscard]] bool hasExactPosition() const
            {
                return line != 0 && column != 0;
            }

            [[nodiscard]] bool isValid() const
            {
                return hasExactPosition();
            }

            [[nodiscard]] operator bool() const
            {
                return isValid();
            }

            [[nodiscard]] static Location invalid()
            {
                return {
                    .file = "",
                    .line = 0,
                    .column = 0
                };
            }
        };
    }
}
