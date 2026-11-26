#include "probe.h"
#include <ref.h>

#include <charconv>
#include <string_view>
#include <variant>
#include <vector>

namespace
{
    struct Counter final : wio::runtime::RefCountedObject
    {
        std::uint64_t value;

        explicit Counter(const std::uint64_t initial) : value(initial)
        {
        }

        std::uint64_t Read() const
        {
            return value;
        }
    };

    struct Pair
    {
        std::uint64_t first;
        std::uint64_t second;
    };

    std::uint64_t run(const std::string_view kind, const int count)
    {
        const std::string fragment = "0123456789abcdef0123456789abcdef";
        if (kind == "concat" || kind == "builder" || kind == "format")
        {
            std::string output;
            for (int index = 0; index < count; ++index)
            {
                if (kind == "concat")
                    output += fragment;
                else if (kind == "builder")
                    output.append(fragment);
                else
                {
                    char buffer[32];
                    const auto formatted = std::to_chars(buffer, buffer + sizeof(buffer), index);
                    output.append(buffer, formatted.ptr);
                    output.push_back('/');
                }
            }

            // Builder's public ToString creates a snapshot in the Wio fixture too.
            if (kind != "concat")
            {
                const auto snapshot = output;
                return native_costs::Hash(snapshot);
            }

            return native_costs::Hash(output);
        }

        if (kind == "bytes")
        {
            std::vector<std::uint8_t> output;
            for (int index = 0; index < count; ++index)
                for (unsigned shift = 0; shift < 32; shift += 8)
                    output.push_back(static_cast<std::uint8_t>(static_cast<unsigned>(index) >> shift));
            const auto snapshot = output;
            std::uint64_t sum = 14695981039346656037ULL;
            for (const auto byte : snapshot)
                sum = (sum ^ byte) * 1099511628211ULL;
            return sum;
        }

        if (kind == "array")
        {
            std::vector<std::uint64_t> values;
            for (int index = 0; index < count; ++index)
                values.push_back(static_cast<std::uint64_t>(index));
            std::uint64_t sum = 0;
            for (const auto value : values)
                sum = sum * 33ULL + value;
            return sum;
        }

        std::uint64_t sum = 0;
        for (int index = 0; index < count; ++index)
        {
            if (kind == "component")
            {
                const Pair value{static_cast<std::uint64_t>(index), static_cast<std::uint64_t>(index) + 1};
                sum = sum * 33ULL + value.first + value.second;
            }
            else if (kind == "object")
            {
                auto value = wio::runtime::Ref<Counter>::Create(static_cast<std::uint64_t>(index));
                auto alias = value;
                sum = sum * 33ULL + alias->Read();
            }
            else if (kind == "native_call")
                sum = native_costs::Step(sum);
            else if (kind == "result")
            {
                // An optimized tagged-value lower bound, not Wio's current object ABI.
                const std::variant<std::uint64_t, std::string> value{static_cast<std::uint64_t>(index)};
                sum = sum * 33ULL + std::get<std::uint64_t>(value);
            }
        }

        return sum;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view kind = argv[1];
    if (kind != "concat" && kind != "builder" && kind != "format" && kind != "bytes" && kind != "array" &&
        kind != "component" && kind != "object" && kind != "native_call" && kind != "result")
        return 3;
    const auto count = native_costs::ParseCount(argv[2]);
    native_costs::Begin();
    return native_costs::Report(run(kind, count));
}
