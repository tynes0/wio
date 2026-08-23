#pragma once

#include <cstdint>
#include <string>

namespace miniwio
{
    struct FlappyTheme
    {
        std::int32_t skyR = 104;
        std::int32_t skyG = 196;
        std::int32_t skyB = 232;
        std::int32_t birdR = 255;
        std::int32_t birdG = 210;
        std::int32_t birdB = 70;
        std::int32_t pipeR = 64;
        std::int32_t pipeG = 180;
        std::int32_t pipeB = 92;
    };

    struct RunOptions
    {
        bool headless = false;
        std::int32_t headlessFrames = 720;
        std::int32_t width = 960;
        std::int32_t height = 720;
    };

    struct RunSummary
    {
        std::int32_t frames = 0;
        std::int32_t pipesSpawned = 0;
        std::int32_t pipesPassed = 0;
        std::int32_t crashes = 0;
        std::int32_t restarts = 0;
        std::int32_t finalScore = 0;
        std::int32_t bestScore = 0;
        std::int32_t level = 1;
        float birdY = 0.0f;
    };

    class GameScript
    {
    public:
        virtual ~GameScript() = default;

        virtual std::string title() const = 0;
        virtual void start() = 0;
        virtual std::int32_t tick(float deltaTime, bool alive, bool headless) = 0;
        virtual float birdVelocity(float currentVelocity, bool flapPressed, float deltaTime) = 0;
        virtual float pipeSpeed() = 0;
        virtual float pipeGap() = 0;
        virtual float spawnInterval() = 0;
        virtual std::int32_t onPipePassed() = 0;
        virtual std::int32_t onCrash() = 0;
        virtual std::int32_t score() const = 0;
        virtual std::int32_t bestScore() const = 0;
        virtual std::int32_t level() const = 0;
        virtual FlappyTheme theme() const = 0;
    };

    class Engine
    {
    public:
        RunSummary run(GameScript& script, const RunOptions& options);
    };
}
