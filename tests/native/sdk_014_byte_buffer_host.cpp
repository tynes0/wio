#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 byte buffer host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        auto profile = module.load_object("Sdk14BufferProfile").create();
        auto field = profile.field("payload");
        if (!field.can_access_as<wio::sdk::WioByteBuffer>() || !field.supports_dynamic_value())
        {
            std::cerr << "ByteBuffer descriptor does not match its SDK value.\n";
            return EXIT_FAILURE;
        }

        auto initial = field.get_as<wio::sdk::WioByteBuffer>();
        if (initial.count() != 4u || initial.capacity() < 16u || initial.position() != 2u ||
            std::to_integer<unsigned>(initial.at(0u)) != 0x78u ||
            std::to_integer<unsigned>(initial.at(3u)) != 0x12u)
        {
            std::cerr << "Initial ByteBuffer did not cross the SDK bridge: count=" << initial.count()
                      << " capacity=" << initial.capacity() << " position=" << initial.position();
            if (initial.count() > 0u)
                std::cerr << " first=" << std::to_integer<unsigned>(initial.at(0u));
            if (initial.count() > 3u)
                std::cerr << " last=" << std::to_integer<unsigned>(initial.at(3u));
            std::cerr << '\n';
            return EXIT_FAILURE;
        }

        wio::sdk::WioByteBuffer payload(32u);
        payload.write_u16_le(0x1234u);
        payload.write(std::byte{0xABu});
        if (!payload.seek(1u))
            return EXIT_FAILURE;
        field.set_as(payload);

        const auto after = field.get_as<wio::sdk::WioByteBuffer>();
        if (after.count() != 3u || after.capacity() < 32u || after.position() != 1u ||
            std::to_integer<unsigned>(after.at(0u)) != 0x34u ||
            std::to_integer<unsigned>(after.at(1u)) != 0x12u ||
            std::to_integer<unsigned>(after.at(2u)) != 0xABu)
        {
            std::cerr << "ByteBuffer did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 buffer: count=" << after.count()
                  << " position=" << after.position()
                  << " capacity=" << after.capacity()
                  << " bytes=" << std::to_integer<unsigned>(after.at(0u))
                  << '-' << std::to_integer<unsigned>(after.at(1u))
                  << '-' << std::to_integer<unsigned>(after.at(2u)) << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
