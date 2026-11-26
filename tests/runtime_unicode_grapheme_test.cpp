#include "std_unicode.h"

#include <iostream>
#include <string_view>

namespace
{
    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }
} // namespace

int main()
{
    bool ok = true;
    ok &= expect(wio::runtime::std_unicode::GraphemeCount("") == 0,
                 "Empty UTF-8 input must contain zero grapheme clusters");
    ok &= expect(wio::runtime::std_unicode::GraphemeCount("é") == 1,
                 "A combining sequence must form one grapheme cluster");
    ok &= expect(wio::runtime::std_unicode::GraphemeCount("👩‍💻") == 1,
                 "A ZWJ emoji sequence must form one grapheme cluster");
    return ok ? 0 : 1;
}
