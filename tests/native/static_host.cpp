#include <cstdint>
#include <iostream>

extern "C" std::int32_t WioAddNumbers(std::int32_t lhs, std::int32_t rhs);

int main()
{
    std::cout << "Static export sum: " << WioAddNumbers(19, 23);
    return 0;
}
