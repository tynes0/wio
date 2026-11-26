#include "builder_benchmark.h"

int main()
{
    builder_benchmark::Begin();
    std::string value;
    for (int index = 0; index < 30000; ++index)
    {
        value.append("0123456789abcdef0123456789abcdef");
    }

    const auto snapshot = value;
    return builder_benchmark::Report(snapshot);
}
