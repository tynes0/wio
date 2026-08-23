#include "miniwio_engine.h"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

#include <wio_sdk.h>

namespace
{
    class WioGameScript final : public miniwio::GameScript
    {
    public:
        explicit WioGameScript(const std::string& modulePath)
            : module_(wio::sdk::Module::load(modulePath)),
              instance_(module_.load_object("GameScript").create()),
              start_(instance_.method<void()>("Start")),
              tick_(instance_.method<std::int32_t(float, bool, bool)>("Tick")),
              birdVelocity_(instance_.method<float(float, bool, float)>("BirdVelocity")),
              pipeSpeed_(instance_.method<float()>("PipeSpeed")),
              pipeGap_(instance_.method<float()>("PipeGap")),
              spawnInterval_(instance_.method<float()>("SpawnInterval")),
              onPipePassed_(instance_.method<std::int32_t()>("OnPipePassed")),
              onCrash_(instance_.method<std::int32_t()>("OnCrash"))
        {
            const auto type = module_.load_object("GameScript");
            if (type.list_fields().size() != 20u || type.descriptor().methodCount != 8u)
                throw std::runtime_error("The Wio Flappy script SDK contract is incomplete.");
        }

        std::string title() const override { return instance_.get<wio::string>("title"); }
        void start() override { start_(); }

        std::int32_t tick(const float deltaTime, const bool alive, const bool headless) override
        {
            module_.update(deltaTime);
            return tick_(deltaTime, alive, headless);
        }

        float birdVelocity(const float currentVelocity, const bool flapPressed, const float deltaTime) override
        {
            return birdVelocity_(currentVelocity, flapPressed, deltaTime);
        }

        float pipeSpeed() override { return pipeSpeed_(); }
        float pipeGap() override { return pipeGap_(); }
        float spawnInterval() override { return spawnInterval_(); }
        std::int32_t onPipePassed() override { return onPipePassed_(); }
        std::int32_t onCrash() override { return onCrash_(); }
        std::int32_t score() const override { return instance_.get<std::int32_t>("score"); }
        std::int32_t bestScore() const override { return instance_.get<std::int32_t>("bestScore"); }
        std::int32_t level() const override { return instance_.get<std::int32_t>("level"); }

        miniwio::FlappyTheme theme() const override
        {
            return {
                instance_.get<std::int32_t>("skyR"),
                instance_.get<std::int32_t>("skyG"),
                instance_.get<std::int32_t>("skyB"),
                instance_.get<std::int32_t>("birdR"),
                instance_.get<std::int32_t>("birdG"),
                instance_.get<std::int32_t>("birdB"),
                instance_.get<std::int32_t>("pipeR"),
                instance_.get<std::int32_t>("pipeG"),
                instance_.get<std::int32_t>("pipeB")
            };
        }

    private:
        wio::sdk::Module module_;
        wio::sdk::WioObject instance_;
        std::function<void()> start_;
        std::function<std::int32_t(float, bool, bool)> tick_;
        std::function<float(float, bool, float)> birdVelocity_;
        std::function<float()> pipeSpeed_;
        std::function<float()> pipeGap_;
        std::function<float()> spawnInterval_;
        std::function<std::int32_t()> onPipePassed_;
        std::function<std::int32_t()> onCrash_;
    };
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "MiniWio engine expected a Wio game script module path.\n";
        return EXIT_FAILURE;
    }

    miniwio::RunOptions options{};
    for (int index = 2; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--headless") options.headless = true;
        if (argument == "--windowed") options.headless = false;
    }

    try
    {
        WioGameScript script(argv[1]);
        miniwio::Engine engine;
        const miniwio::RunSummary summary = engine.run(script, options);

        if (options.headless &&
            (summary.frames != 720 || summary.pipesSpawned < 6 || summary.pipesPassed < 2 ||
             summary.crashes < 1 || summary.bestScore < 2))
        {
            std::cerr << "MiniWio Flappy headless acceptance contract failed.\n";
            return EXIT_FAILURE;
        }

        std::cout << "MiniWio Flappy: mode=" << (options.headless ? "headless" : "windowed")
                  << " frames=" << summary.frames
                  << " spawned=" << summary.pipesSpawned
                  << " passed=" << summary.pipesPassed
                  << " crashes=" << summary.crashes
                  << " restarts=" << summary.restarts
                  << " score=" << summary.finalScore
                  << " best=" << summary.bestScore
                  << " level=" << summary.level
                  << " birdY=" << static_cast<std::int32_t>(summary.birdY)
                  << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "MiniWio engine failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
