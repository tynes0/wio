#pragma once

#include <cstdint>
#include <string>

namespace miniwio
{
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct VisualTheme
    {
        std::int32_t backgroundR = 18;
        std::int32_t backgroundG = 22;
        std::int32_t backgroundB = 32;
        std::int32_t accentR = 75;
        std::int32_t accentG = 210;
        std::int32_t accentB = 160;
    };

    struct RunOptions
    {
        bool headless = false;
        std::int32_t headlessFrames = 240;
        std::int32_t width = 1000;
        std::int32_t height = 700;
    };

    struct RunSummary
    {
        std::int32_t frames = 0;
        std::int32_t spawned = 0;
        std::int32_t collisions = 0;
        std::int32_t finalEnemies = 0;
        std::int32_t finalScore = 0;
        std::int32_t finalWave = 0;
        Vec2 player{};
    };

    class GameScript
    {
    public:
        virtual ~GameScript() = default;

        virtual std::string title() const = 0;
        virtual void start() = 0;
        virtual std::int32_t tick(float inputX, float inputY, float deltaTime,
                                  std::int32_t enemyCount, bool headless) = 0;
        virtual Vec2 playerVelocity(float inputX, float inputY) = 0;
        virtual Vec2 enemyVelocity(float deltaX, float deltaY) = 0;
        virtual std::int32_t onCollision() = 0;
        virtual std::int32_t score() const = 0;
        virtual std::int32_t wave() const = 0;
        virtual VisualTheme theme() const = 0;
    };

    class Engine
    {
    public:
        RunSummary run(GameScript& script, const RunOptions& options);
    };
}
