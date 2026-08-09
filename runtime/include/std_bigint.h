#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_bigint
{
    inline std::string Normalize(const std::string_view text)
    {
        if (text.empty()) return {};
        std::size_t index = 0;
        bool negative = false;
        if (text[index] == '-' || text[index] == '+') { negative = text[index] == '-'; ++index; }
        if (index == text.size()) return {};
        for (std::size_t cursor = index; cursor < text.size(); ++cursor)
            if (text[cursor] < '0' || text[cursor] > '9') return {};
        while (index < text.size() && text[index] == '0') ++index;
        if (index == text.size()) return "0";
        return (negative ? "-" : "") + std::string(text.substr(index));
    }

    inline bool Negative(const std::string_view value) noexcept { return value.starts_with('-'); }
    inline std::string_view Absolute(const std::string_view value) noexcept { return Negative(value) ? value.substr(1) : value; }
    inline int CompareAbsolute(const std::string_view left, const std::string_view right) noexcept
    {
        if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
        return left == right ? 0 : (left < right ? -1 : 1);
    }
    inline std::string AddAbsolute(std::string_view left, std::string_view right)
    {
        std::string output; output.reserve(std::max(left.size(), right.size()) + 1);
        std::ptrdiff_t li = static_cast<std::ptrdiff_t>(left.size()) - 1;
        std::ptrdiff_t ri = static_cast<std::ptrdiff_t>(right.size()) - 1;
        int carry = 0;
        while (li >= 0 || ri >= 0 || carry)
        {
            const int sum = carry + (li >= 0 ? left[static_cast<std::size_t>(li--)] - '0' : 0) +
                (ri >= 0 ? right[static_cast<std::size_t>(ri--)] - '0' : 0);
            output.push_back(static_cast<char>('0' + sum % 10)); carry = sum / 10;
        }
        std::reverse(output.begin(), output.end()); return output;
    }
    inline std::string SubtractAbsolute(std::string_view left, std::string_view right)
    {
        std::string output; output.reserve(left.size()); int borrow = 0;
        std::ptrdiff_t ri = static_cast<std::ptrdiff_t>(right.size()) - 1;
        for (std::ptrdiff_t li = static_cast<std::ptrdiff_t>(left.size()) - 1; li >= 0; --li)
        {
            int digit = left[static_cast<std::size_t>(li)] - '0' - borrow -
                (ri >= 0 ? right[static_cast<std::size_t>(ri--)] - '0' : 0);
            if (digit < 0) { digit += 10; borrow = 1; } else borrow = 0;
            output.push_back(static_cast<char>('0' + digit));
        }
        while (output.size() > 1 && output.back() == '0') output.pop_back();
        std::reverse(output.begin(), output.end()); return output;
    }
    inline std::string Add(const std::string_view leftText, const std::string_view rightText)
    {
        const std::string left = Normalize(leftText), right = Normalize(rightText);
        if (left.empty() || right.empty()) return {};
        const bool ln = Negative(left), rn = Negative(right);
        const auto la = Absolute(left), ra = Absolute(right);
        if (ln == rn) { const std::string sum = AddAbsolute(la, ra); return ln && sum != "0" ? "-" + sum : sum; }
        const int comparison = CompareAbsolute(la, ra);
        if (comparison == 0) return "0";
        const bool resultNegative = comparison > 0 ? ln : rn;
        const std::string difference = comparison > 0 ? SubtractAbsolute(la, ra) : SubtractAbsolute(ra, la);
        return resultNegative ? "-" + difference : difference;
    }
    inline std::string Subtract(const std::string_view left, const std::string_view right)
    {
        const std::string normalized = Normalize(right); if (normalized.empty()) return {};
        return Add(left, Negative(normalized) ? normalized.substr(1) : "-" + normalized);
    }
    inline std::string Multiply(const std::string_view leftText, const std::string_view rightText)
    {
        const std::string left = Normalize(leftText), right = Normalize(rightText);
        if (left.empty() || right.empty()) return {};
        const auto la = Absolute(left), ra = Absolute(right);
        if (la == "0" || ra == "0") return "0";
        std::vector<int> digits(la.size() + ra.size(), 0);
        for (std::ptrdiff_t li = static_cast<std::ptrdiff_t>(la.size()) - 1; li >= 0; --li)
            for (std::ptrdiff_t ri = static_cast<std::ptrdiff_t>(ra.size()) - 1; ri >= 0; --ri)
            {
                const std::size_t position = static_cast<std::size_t>(li + ri + 1);
                const int product = (la[static_cast<std::size_t>(li)] - '0') * (ra[static_cast<std::size_t>(ri)] - '0') + digits[position];
                digits[position] = product % 10; digits[position - 1] += product / 10;
            }
        std::string output; std::size_t index = 0; while (index < digits.size() && digits[index] == 0) ++index;
        if (Negative(left) != Negative(right)) output.push_back('-');
        for (; index < digits.size(); ++index) output.push_back(static_cast<char>('0' + digits[index]));
        return output;
    }
    inline std::string Divide(const std::string_view leftText, const std::string_view rightText)
    {
        const std::string left = Normalize(leftText), right = Normalize(rightText);
        if (left.empty() || right.empty() || Absolute(right) == "0") return {};
        const auto dividend = Absolute(left), divisor = Absolute(right);
        std::string remainder = "0", quotient;
        for (const char digit : dividend)
        {
            remainder = Normalize((remainder == "0" ? std::string{} : remainder) + digit);
            int count = 0;
            while (CompareAbsolute(remainder, divisor) >= 0) { remainder = SubtractAbsolute(remainder, divisor); ++count; }
            quotient.push_back(static_cast<char>('0' + count));
        }
        quotient = Normalize(quotient);
        if (quotient != "0" && Negative(left) != Negative(right)) quotient.insert(quotient.begin(), '-');
        return quotient;
    }
    inline std::int32_t Compare(const std::string_view leftText, const std::string_view rightText) noexcept
    {
        const std::string left = Normalize(leftText), right = Normalize(rightText);
        if (left.empty() || right.empty()) return 0;
        if (Negative(left) != Negative(right)) return Negative(left) ? -1 : 1;
        const int absolute = CompareAbsolute(Absolute(left), Absolute(right));
        return Negative(left) ? -absolute : absolute;
    }
}
