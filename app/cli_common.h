#pragma once

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <vector>

namespace wio::tooling::cli
{
    inline bool IsHelpToken(const std::string_view value)
    {
        return value == "--help" || value == "-h" || value == "help";
    }

    inline bool IsVersionToken(const std::string_view value)
    {
        return value == "--version" || value == "-v" || value == "version";
    }

    inline size_t EditDistance(const std::string_view left, const std::string_view right)
    {
        std::vector<size_t> previous(right.size() + 1);
        std::vector<size_t> current(right.size() + 1);
        for (size_t index = 0; index <= right.size(); ++index)
            previous[index] = index;

        for (size_t leftIndex = 0; leftIndex < left.size(); ++leftIndex)
        {
            current[0] = leftIndex + 1;
            for (size_t rightIndex = 0; rightIndex < right.size(); ++rightIndex)
            {
                const size_t substitutionCost = left[leftIndex] == right[rightIndex] ? 0 : 1;
                current[rightIndex + 1] = std::min({
                    current[rightIndex] + 1,
                    previous[rightIndex + 1] + 1,
                    previous[rightIndex] + substitutionCost
                });
            }
            previous.swap(current);
        }

        return previous.back();
    }

    inline std::optional<std::string_view> SuggestCommand(
        const std::string_view input,
        const std::initializer_list<std::string_view> candidates,
        const size_t maximumDistance = 2)
    {
        std::optional<std::string_view> best;
        size_t bestDistance = maximumDistance + 1;
        for (const std::string_view candidate : candidates)
        {
            const size_t distance = EditDistance(input, candidate);
            if (distance < bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }
        return best;
    }
}
