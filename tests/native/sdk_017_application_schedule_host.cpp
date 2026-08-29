#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "wio_sdk.h"

namespace
{
    bool has_flag(const WioApplicationStageDescriptor& stage, const std::uint32_t flag)
    {
        return (stage.flags & flag) != 0u;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "SDK application schedule host expected one module path.\n";
        return 2;
    }

    try
    {
        auto module = wio::sdk::Module::load(argv[1]);
        if (!module.has_capability(WIO_MODULE_CAP_APPLICATION_SCHEDULE_V1))
            throw std::runtime_error("application schedule capability is missing");

        auto application = module.application();
        const auto stages = application.stages();
        if (stages.size() != 4u)
            throw std::runtime_error("unexpected application schedule stage count");

        if (std::string_view(stages[0].logicalName) != "input" ||
            !has_flag(stages[0], WIO_APPLICATION_STAGE_CONTAINS_SYSTEM))
            throw std::runtime_error("system stage metadata mismatch");

        if (std::string_view(stages[1].logicalName) != "Physics" ||
            std::string_view(stages[1].afterStage) != "input" ||
            std::abs(stages[1].fixedHz - 20.0) > 0.000001 ||
            !has_flag(stages[1], WIO_APPLICATION_STAGE_FIXED) ||
            !has_flag(stages[1], WIO_APPLICATION_STAGE_CONTAINS_APPLICATION))
            throw std::runtime_error("fixed stage metadata mismatch");

        if (std::string_view(stages[2].logicalName) != "Render" ||
            std::string_view(stages[2].afterStage) != "Physics" ||
            !has_flag(stages[2], WIO_APPLICATION_STAGE_MAIN_THREAD))
            throw std::runtime_error("main stage metadata mismatch");

        if (std::string_view(stages[3].logicalName) != "Update" ||
            stages[3].order != 3u ||
            !has_flag(stages[3], WIO_APPLICATION_STAGE_CONTAINS_APPLICATION))
            throw std::runtime_error("application update stage metadata mismatch");

        application.start();
        (void)application.update(0.05);
        application.close();

        std::cout << "sdk-application-schedule-ok stages=4 fixed=20 main=Render\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
