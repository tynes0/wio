#include <wio_native_sdk.h>

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 2)
        return 2;
    try
    {
        auto module = wio::sdk::NativeModule::open(std::filesystem::u8path(argv[1]));
    }
    catch (const std::runtime_error& error)
    {
        if (std::string_view{error.what()} == "invalid canonical Wio module descriptor")
        {
            std::cout << "invalid-vm-module-rejected\n";
            return 0;
        }
    }
    return 1;
}
