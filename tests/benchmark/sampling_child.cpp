#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    // Synthetic protocol only, never a language performance implementation.
    const bool wrong = argc > 1 && std::string_view(argv[1]) == "wrong";
    std::cout << "{\"schemaVersion\":1,\"elapsedNanoseconds\":1000000,\"operations\":1,\"checksum\":\""
              << (wrong ? "wrong" : "42") << "\"}\n";
    return 0;
}
