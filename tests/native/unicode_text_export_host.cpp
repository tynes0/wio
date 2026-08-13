#include <text.h>

#include <cstddef>
#include <iostream>

extern "C" wio::runtime::Text WioDecorateText(wio::runtime::Text value);
extern "C" std::size_t WioTextCodePointCount(wio::runtime::Text value);

int main()
{
    const auto source = wio::runtime::Text::FromUtf8("Merhaba 🌍");
    const auto decorated = WioDecorateText(source);
    const bool valid = decorated == wio::runtime::Text::FromUtf8("⟦Merhaba 🌍⟧");
    std::cout << "Text export: valid=" << std::boolalpha << valid
              << " count=" << WioTextCodePointCount(source);
    return 0;
}
