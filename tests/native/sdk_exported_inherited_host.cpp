#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "SDK exported inherited host expected a library path." << '\n';
        return EXIT_FAILURE;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        auto bossType = module.load_object("Boss");
        auto boss = bossType.create();

        auto score0 = boss.method<std::int32_t()>("Score");
        auto score1 = boss.method<std::int32_t(std::int32_t)>("Score");
        auto score2 = boss.method<std::int32_t(std::int32_t, std::int32_t)>("Score");
        auto heal = boss.method<std::int32_t(std::int32_t)>("Heal");
        auto enrage = boss.method<std::int32_t(std::int32_t)>("Enrage");

        const std::int32_t hp = boss.get<std::int32_t>("hp");
        const std::int32_t id = boss.get<std::int32_t>("id");
        const std::int32_t phase = boss.get<std::int32_t>("phase");
        const std::int32_t baseScore = score0();
        const std::int32_t overloadedScore = score1(5);
        const std::int32_t newPhase = enrage(2);
        const std::int32_t healResult = heal(4);
        const std::int32_t derivedScore = score2(1, 2);

        std::cout << "SDK inherited exported types: exports=" << module.api().exportCount
                  << " types=" << module.api().typeCount
                  << " fields=" << bossType.descriptor().fieldCount
                  << " methods=" << bossType.descriptor().methodCount
                  << " hp=" << hp
                  << " id=" << id
                  << " phase=" << phase
                  << " score0=" << baseScore
                  << " score1=" << overloadedScore
                  << " enrage=" << newPhase
                  << " heal=" << healResult
                  << " score2=" << derivedScore
                  << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
