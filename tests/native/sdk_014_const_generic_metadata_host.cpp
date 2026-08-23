#include <cstdlib>
#include <iostream>
#include <string_view>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 const-generic metadata host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        const auto info = module.inspect();
        if (info.descriptor_version != 8u)
        {
            std::cerr << "Const-generic metadata requires ABI descriptor version 8.\n";
            return EXIT_FAILURE;
        }

        auto profile = module.load_object("Sdk14GenericMetadata").create();
        const auto block = profile.field("block").type();
        const auto label = profile.field("label").type();
        const auto empty = profile.field("empty").type();

        if (!block.is_component() || block.generic_argument_count() != 2u ||
            !block.generic_argument(0u).is_primitive() ||
            !block.generic_argument(1u).is_const_value() ||
            block.generic_argument(1u).const_value() != "4" ||
            !block.generic_argument(1u).const_value_type().is_integer() ||
            block.generic_argument(1u).const_value_type().abi_type() != WIO_ABI_USIZE)
        {
            std::cerr << "Integer const-generic metadata is incomplete.\n";
            return EXIT_FAILURE;
        }

        if (!label.is_component() || label.generic_argument_count() != 1u ||
            !label.generic_argument(0u).is_const_value() ||
            label.generic_argument(0u).const_value() != "sdk" ||
            !label.generic_argument(0u).const_value_type().is_string() ||
            !empty.generic_argument(0u).is_const_value() ||
            empty.generic_argument(0u).const_value() != "" ||
            empty.generic_argument(0u).raw()->constValue == nullptr)
        {
            std::cerr << "String const-generic metadata is incomplete.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 const metadata: abi=" << info.descriptor_version
                  << " block=" << block.generic_argument(1u).const_value()
                  << " label=" << label.generic_argument(0u).const_value()
                  << " empty=ok\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
