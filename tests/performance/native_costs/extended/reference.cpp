#include "workloads.h"
#include <std_async.h>
#include <text.h>

#include <functional>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace
{
    wio::runtime::AsyncTask<std::uint64_t> ready(const std::uint64_t value)
    {
        co_return value;
    }

    struct Error
    {
        int code;
        std::string message;
    };

    std::uint64_t run(const std::string_view kind, const int count)
    {
        if (kind == "unicode")
        {
            const auto source = wio::runtime::Text::FromUtf8("A\xf0\x9f\x8c\x8d"
                                                             "e\xcc\x81"
                                                             "日本語");
            wio::runtime::Text output;
            for (int index = 0; index < count; ++index)
                output += source.slice(1, 3);
            const auto encoded = output._WF_ToString();
            return native_costs::Hash(encoded);
        }

        if (kind == "dictionary")
        {
            std::unordered_map<std::int32_t, std::uint64_t> values;
            for (int index = 0; index < count; ++index)
                values.insert_or_assign(index, static_cast<std::uint64_t>(index) * 3ULL);
            std::uint64_t sum = 0;
            for (int index = 0; index < count; ++index)
            {
                const auto found = values.find(index);
                sum = sum * 33ULL + (found == values.end() ? 99ULL : found->second);
            }

            const auto missing = values.find(count);
            return sum * 33ULL + (missing == values.end() ? 99ULL : missing->second);
        }

        std::uint64_t sum = 0;
        for (int index = 0; index < count; ++index)
        {
            if (kind == "result_error")
            {
                // A tagged-value error lower bound, not Wio's object/identity ABI.
                const std::variant<std::uint64_t, Error> result{
                    std::in_place_type<Error>,
                    Error{index, "expected diagnostic error payload longer than small string storage"}};
                if (!std::holds_alternative<Error>(result))
                    return 0;
                const auto error = std::get<Error>(result);
                sum = sum * 33ULL + static_cast<std::uint64_t>(error.code) + native_costs::Hash(error.message);
            }
            else if (kind == "weak")
                sum = sum * 33ULL + native_costs_extended::WeakRoundtrip(static_cast<std::uint64_t>(index));
            else if (kind == "closure")
            {
                const auto offset = static_cast<std::uint64_t>(index);
                const std::function<std::uint64_t(std::uint64_t)> callback = [offset](const std::uint64_t value)
                { return value + offset; };
                sum = sum * 33ULL + callback(1);
            }
            else if (kind == "async_ready")
            {
                const auto task = ready(static_cast<std::uint64_t>(index) + 1);
                if (!task.IsReady())
                    return 0;
                sum = sum * 33ULL + wio::runtime::BlockOn(task);
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
    if (kind != "unicode" && kind != "dictionary" && kind != "result_error" && kind != "weak" && kind != "closure" &&
        kind != "async_ready")
        return 3;
    try
    {
        const auto count = native_costs::ParseCount(argv[2]);
        native_costs::Begin();
        return native_costs::Report(run(kind, count));
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 2;
    }
}
