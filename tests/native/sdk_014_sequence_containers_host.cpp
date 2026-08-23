#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include <wio_sdk.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Wio SDK 0.14 sequence containers host expected a library path.\n";
        return EXIT_FAILURE;
    }

    try
    {
        using IntOption = wio::sdk::WioOption<std::int32_t>;
        using WorkQueue = wio::sdk::WioQueue<IntOption>;
        using TagSet = wio::sdk::WioUnorderedSet<std::string>;
        using LevelSet = wio::sdk::WioOrderedSet<std::int32_t>;

        auto module = wio::sdk::Module::load(argv[1]);
        auto state = module.load_object("Sdk14SequenceContainers").create();
        auto workField = state.field("work");
        auto tagsField = state.field("tags");
        auto levelsField = state.field("levels");

        if (!workField.can_access_as<WorkQueue>() || !tagsField.can_access_as<TagSet>() ||
            !levelsField.can_access_as<LevelSet>())
        {
            std::cerr << "Sequence container descriptors do not match their SDK values.\n";
            return EXIT_FAILURE;
        }

        auto initialWork = workField.get_as<WorkQueue>();
        auto initialTags = tagsField.get_as<TagSet>();
        auto initialLevels = levelsField.get_as<LevelSet>();
        const auto initialWorkValues = initialWork.to_array();
        if (initialWorkValues.size() != 3u || initialWorkValues[0].value_or(0) != 2 ||
            initialWorkValues[1].is_some() || initialWorkValues[2].value_or(0) != 6 ||
            !initialTags.contains("red") || !initialTags.contains("blue") ||
            initialLevels.count() != 3u || initialLevels.first() != 1 || initialLevels.last() != 3)
        {
            std::cerr << "Initial sequence containers did not cross the SDK bridge.\n";
            return EXIT_FAILURE;
        }

        WorkQueue work;
        work.push(IntOption::none());
        work.push(IntOption::some(14));
        TagSet tags;
        (void)tags.add("host");
        (void)tags.add("sdk");
        LevelSet levels;
        (void)levels.add(9);
        (void)levels.add(4);
        (void)levels.add(7);

        workField.set_as(std::move(work));
        tagsField.set_as(std::move(tags));
        levelsField.set_as(std::move(levels));

        const auto workAfter = workField.get_as<WorkQueue>().to_array();
        const auto tagsAfter = tagsField.get_as<TagSet>();
        const auto levelsAfter = levelsField.get_as<LevelSet>();
        if (workAfter.size() != 2u || workAfter[0].is_some() || workAfter[1].value_or(0) != 14 ||
            !tagsAfter.contains("host") || !tagsAfter.contains("sdk") ||
            levelsAfter.first() != 4 || levelsAfter.last() != 9)
        {
            std::cerr << "Sequence containers did not round-trip through Wio.\n";
            return EXIT_FAILURE;
        }

        std::cout << "SDK 0.14 sequences: queue=" << workAfter.size()
                  << " tags=" << tagsAfter.count()
                  << " levels=" << levelsAfter.first() << '-' << levelsAfter.last() << '\n';
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
