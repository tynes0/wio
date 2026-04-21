#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <module_api.h>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "SDK exported types host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);

        auto counterType = module.load_object("Counter");
        auto counter = counterType.create();
        auto add = counter.method<std::int32_t(std::int32_t)>("Add");
        auto mix = counter.method<std::int32_t(std::int32_t)>("Mix");

        const std::int32_t start = counter.get<std::int32_t>("value");
        counter.set("value", 20);
        const std::int32_t afterAdd = add(5);
        const std::int32_t mixed = mix(2);

        auto statsType = module.load_component("Stats");
        auto stats = statsType.create();
        const std::int32_t hp = stats.get<std::int32_t>("hp");
        stats.set("hp", 135);
        const std::int32_t hpAfter = stats.get<std::int32_t>("hp");
        const std::int32_t maxHp = stats.get<std::int32_t>("maxHp");

        const WioModuleField* seedField = WioFindModuleField(&counterType.descriptor(), "seed");
        const WioModuleField* maxHpField = WioFindModuleField(&statsType.descriptor(), "maxHp");
        if (seedField == nullptr ||
            maxHpField == nullptr ||
            seedField->setterExport != nullptr ||
            maxHpField->setterExport != nullptr ||
            (seedField->flags & WIO_MODULE_FIELD_READONLY) == 0u ||
            (maxHpField->flags & WIO_MODULE_FIELD_READONLY) == 0u)
        {
            std::cerr << "Read-only exported field metadata is incomplete." << '\n';
            return EXIT_FAILURE;
        }

        std::cout << "SDK exported types: exports=" << module.api().exportCount
                  << " types=" << module.api().typeCount
                  << " objectFields=" << counterType.descriptor().fieldCount
                  << " objectMethods=" << counterType.descriptor().methodCount
                  << " start=" << start
                  << " afterAdd=" << afterAdd
                  << " mixed=" << mixed
                  << " componentFields=" << statsType.descriptor().fieldCount
                  << " hp=" << hp
                  << " hpAfter=" << hpAfter
                  << " maxHp=" << maxHp
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
