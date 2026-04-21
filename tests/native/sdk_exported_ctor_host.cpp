#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "SDK exported ctor host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);

        auto enemyType = module.load_object("Enemy");
        auto spawnType = module.load_component("SpawnPoint");

        if (enemyType.descriptor().createExport != nullptr || spawnType.descriptor().createExport != nullptr)
        {
            std::cerr << "Zero-argument exported create entry should be absent for ctor-only exported types." << '\n';
            return EXIT_FAILURE;
        }

        auto enemy = enemyType.create(10, 5);
        auto damage = enemy.method<std::int32_t(std::int32_t)>("Damage");
        const std::int32_t enemyHp = enemy.get<std::int32_t>("hp");
        const std::int32_t afterDamage = damage(4);

        auto point = spawnType.create(4, 9);
        const std::int32_t x = point.get<std::int32_t>("x");
        const std::int32_t y = point.get<std::int32_t>("y");

        std::cout << "SDK ctor exported types: exports=" << module.api().exportCount
                  << " types=" << module.api().typeCount
                  << " enemy=" << enemyHp
                  << " afterDamage=" << afterDamage
                  << " point=" << x << "," << y
                  << " ctorCount=" << enemyType.descriptor().constructorCount << "/" << spawnType.descriptor().constructorCount
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
