#include "miniwio_engine.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
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
              tick_(instance_.method<std::int32_t(float, float, float, std::int32_t, bool)>("Tick")),
              playerVelocityX_(instance_.method<float(float, float)>("PlayerVelocityX")),
              playerVelocityY_(instance_.method<float(float, float)>("PlayerVelocityY")),
              enemyVelocityX_(instance_.method<float(float, float)>("EnemyVelocityX")),
              enemyVelocityY_(instance_.method<float(float, float)>("EnemyVelocityY")),
              onCollision_(instance_.method<std::int32_t()>("OnCollision"))
        {
            const auto info = module_.inspect();
            if (!info.has_capability(WIO_MODULE_CAP_TYPE_METADATA_V2))
                throw std::runtime_error("The Wio game script does not expose type metadata v2.");

            const auto type = module_.load_object("GameScript");
            if (type.list_fields().size() != 13u || type.descriptor().methodCount != 7u)
                throw std::runtime_error("The Wio GameScript SDK contract is incomplete.");
        }

        std::string title() const override
        {
            return instance_.get<wio::string>("title");
        }

        void start() override
        {
            start_();
        }

        std::int32_t tick(const float inputX, const float inputY, const float deltaTime,
                          const std::int32_t enemyCount, const bool headless) override
        {
            module_.update(deltaTime);
            return tick_(inputX, inputY, deltaTime, enemyCount, headless);
        }

        miniwio::Vec2 playerVelocity(const float inputX, const float inputY) override
        {
            return {playerVelocityX_(inputX, inputY), playerVelocityY_(inputX, inputY)};
        }

        miniwio::Vec2 enemyVelocity(const float deltaX, const float deltaY) override
        {
            return {enemyVelocityX_(deltaX, deltaY), enemyVelocityY_(deltaX, deltaY)};
        }

        std::int32_t onCollision() override
        {
            return onCollision_();
        }

        std::int32_t score() const override
        {
            return instance_.get<std::int32_t>("score");
        }

        std::int32_t wave() const override
        {
            return instance_.get<std::int32_t>("wave");
        }

        miniwio::VisualTheme theme() const override
        {
            return {
                instance_.get<std::int32_t>("backgroundR"),
                instance_.get<std::int32_t>("backgroundG"),
                instance_.get<std::int32_t>("backgroundB"),
                instance_.get<std::int32_t>("accentR"),
                instance_.get<std::int32_t>("accentG"),
                instance_.get<std::int32_t>("accentB")
            };
        }

    private:
        wio::sdk::Module module_;
        wio::sdk::WioObject instance_;
        std::function<void()> start_;
        std::function<std::int32_t(float, float, float, std::int32_t, bool)> tick_;
        std::function<float(float, float)> playerVelocityX_;
        std::function<float(float, float)> playerVelocityY_;
        std::function<float(float, float)> enemyVelocityX_;
        std::function<float(float, float)> enemyVelocityY_;
        std::function<std::int32_t()> onCollision_;
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
            (summary.frames != 240 || summary.spawned < 6 || summary.collisions < 1 ||
             summary.finalScore < 100 || summary.finalWave != 3))
        {
            std::cerr << "MiniWio headless acceptance contract failed.\n";
            return EXIT_FAILURE;
        }

        std::cout << "MiniWio engine: mode=" << (options.headless ? "headless" : "windowed")
                  << " frames=" << summary.frames
                  << " spawned=" << summary.spawned
                  << " enemies=" << summary.finalEnemies
                  << " collisions=" << summary.collisions
                  << " score=" << summary.finalScore
                  << " wave=" << summary.finalWave
                  << " player=" << static_cast<std::int32_t>(summary.player.x)
                  << ',' << static_cast<std::int32_t>(summary.player.y)
                  << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "MiniWio engine failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
