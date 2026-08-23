#include "miniwio_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <raylib.h>

namespace miniwio
{
    namespace
    {
        constexpr std::int32_t kActionExit = 1;
        constexpr float kHudHeight = 70.0f;
        constexpr float kGroundHeight = 92.0f;
        constexpr float kBirdX = 240.0f;
        constexpr float kBirdRadius = 18.0f;
        constexpr float kPipeWidth = 82.0f;

        enum class RoundState
        {
            Ready,
            Playing,
            GameOver
        };

        struct Bird
        {
            float y = 320.0f;
            float velocity = 0.0f;
        };

        struct Pipe
        {
            float x = 0.0f;
            float gapY = 320.0f;
            float gapSize = 210.0f;
            bool passed = false;
        };

        struct WorldEvents
        {
            std::int32_t spawned = 0;
            std::int32_t passed = 0;
            bool crashed = false;
        };

        Color color(const std::int32_t red, const std::int32_t green, const std::int32_t blue,
                    const std::uint8_t alpha = 255u)
        {
            return Color{
                static_cast<unsigned char>(std::clamp(red, 0, 255)),
                static_cast<unsigned char>(std::clamp(green, 0, 255)),
                static_cast<unsigned char>(std::clamp(blue, 0, 255)),
                alpha
            };
        }

        bool circleIntersectsRectangle(const float circleX, const float circleY, const float radius,
                                       const float x, const float y, const float width, const float height)
        {
            const float closestX = std::clamp(circleX, x, x + width);
            const float closestY = std::clamp(circleY, y, y + height);
            const float deltaX = circleX - closestX;
            const float deltaY = circleY - closestY;
            return deltaX * deltaX + deltaY * deltaY <= radius * radius;
        }

        class FlappyWorld
        {
        public:
            FlappyWorld(const float width, const float height)
                : width_(width), height_(height)
            {
                resetRound();
            }

            void resetRound()
            {
                bird_ = Bird{height_ * 0.46f, 0.0f};
                pipes_.clear();
                spawnTimer_ = 0.0f;
            }

            void beginRound(GameScript& script)
            {
                resetRound();
                spawnPipe(script, width_ + 120.0f);
            }

            WorldEvents update(GameScript& script, const float deltaTime, const bool flap)
            {
                WorldEvents events{};
                bird_.velocity = script.birdVelocity(bird_.velocity, flap, deltaTime);
                bird_.y += bird_.velocity * deltaTime;

                spawnTimer_ += deltaTime;
                if (spawnTimer_ >= script.spawnInterval())
                {
                    spawnTimer_ = 0.0f;
                    spawnPipe(script, width_ + 40.0f);
                    ++events.spawned;
                }

                const float speed = script.pipeSpeed();
                for (Pipe& pipe : pipes_)
                {
                    pipe.x -= speed * deltaTime;
                    if (!pipe.passed && pipe.x + kPipeWidth < kBirdX)
                    {
                        pipe.passed = true;
                        script.onPipePassed();
                        ++events.passed;
                    }
                }

                pipes_.erase(
                    std::remove_if(pipes_.begin(), pipes_.end(), [](const Pipe& pipe)
                    {
                        return pipe.x + kPipeWidth < -10.0f;
                    }),
                    pipes_.end());

                events.crashed = collides();
                return events;
            }

            [[nodiscard]] bool collides() const
            {
                const float groundY = height_ - kGroundHeight;
                if (bird_.y - kBirdRadius <= kHudHeight || bird_.y + kBirdRadius >= groundY)
                    return true;

                for (const Pipe& pipe : pipes_)
                {
                    const float gapTop = pipe.gapY - pipe.gapSize * 0.5f;
                    const float gapBottom = pipe.gapY + pipe.gapSize * 0.5f;
                    if (circleIntersectsRectangle(kBirdX, bird_.y, kBirdRadius,
                                                  pipe.x, kHudHeight, kPipeWidth, gapTop - kHudHeight) ||
                        circleIntersectsRectangle(kBirdX, bird_.y, kBirdRadius,
                                                  pipe.x, gapBottom, kPipeWidth, groundY - gapBottom))
                    {
                        return true;
                    }
                }
                return false;
            }

            [[nodiscard]] float autopilotTarget() const
            {
                for (const Pipe& pipe : pipes_)
                {
                    if (pipe.x + kPipeWidth >= kBirdX - 12.0f)
                        return pipe.gapY;
                }
                return height_ * 0.46f;
            }

            void forceCrash() { bird_.y = height_ + 100.0f; }
            [[nodiscard]] const Bird& bird() const noexcept { return bird_; }
            [[nodiscard]] const std::vector<Pipe>& pipes() const noexcept { return pipes_; }
            [[nodiscard]] float groundY() const noexcept { return height_ - kGroundHeight; }

        private:
            void spawnPipe(GameScript& script, const float x)
            {
                const float gapSize = script.pipeGap();
                const float minimum = kHudHeight + gapSize * 0.5f + 44.0f;
                const float maximum = groundY() - gapSize * 0.5f - 44.0f;
                std::uniform_real_distribution<float> gapDistribution(minimum, std::max(minimum, maximum));
                pipes_.push_back(Pipe{x, gapDistribution(random_), gapSize, false});
            }

            float width_ = 960.0f;
            float height_ = 720.0f;
            Bird bird_{};
            std::vector<Pipe> pipes_{};
            float spawnTimer_ = 0.0f;
            std::mt19937 random_{0x464c4150u};
        };

        bool flapPressed()
        {
            return IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP) ||
                   IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        }

        bool headlessFlap(const FlappyWorld& world)
        {
            const float target = world.autopilotTarget() - 8.0f;
            return world.bird().y > target + 10.0f && world.bird().velocity > -120.0f;
        }

        void drawCloud(const float x, const float y, const float scale)
        {
            const Color cloud = color(255, 255, 255, 155);
            DrawCircleV(Vector2{x, y}, 26.0f * scale, cloud);
            DrawCircleV(Vector2{x + 28.0f * scale, y - 8.0f * scale}, 34.0f * scale, cloud);
            DrawCircleV(Vector2{x + 65.0f * scale, y}, 24.0f * scale, cloud);
            DrawRectangleRounded(Rectangle{x, y, 66.0f * scale, 25.0f * scale}, 0.8f, 8, cloud);
        }

        void drawPipe(const Pipe& pipe, const float groundY, const FlappyTheme& theme)
        {
            const Color body = color(theme.pipeR, theme.pipeG, theme.pipeB);
            const Color shadow = color(theme.pipeR - 24, theme.pipeG - 34, theme.pipeB - 20);
            const Color highlight = color(theme.pipeR + 38, theme.pipeG + 35, theme.pipeB + 28);
            const float gapTop = pipe.gapY - pipe.gapSize * 0.5f;
            const float gapBottom = pipe.gapY + pipe.gapSize * 0.5f;
            const Rectangle top{pipe.x, kHudHeight, kPipeWidth, gapTop - kHudHeight};
            const Rectangle bottom{pipe.x, gapBottom, kPipeWidth, groundY - gapBottom};

            DrawRectangleRounded(top, 0.16f, 8, body);
            DrawRectangleRounded(bottom, 0.16f, 8, body);
            DrawRectangle(static_cast<int>(pipe.x + kPipeWidth - 15.0f), static_cast<int>(top.y), 15,
                          static_cast<int>(top.height), shadow);
            DrawRectangle(static_cast<int>(pipe.x + kPipeWidth - 15.0f), static_cast<int>(bottom.y), 15,
                          static_cast<int>(bottom.height), shadow);
            DrawRectangle(static_cast<int>(pipe.x + 10.0f), static_cast<int>(top.y), 8,
                          static_cast<int>(top.height), highlight);
            DrawRectangle(static_cast<int>(pipe.x + 10.0f), static_cast<int>(bottom.y), 8,
                          static_cast<int>(bottom.height), highlight);
            DrawRectangleRounded(Rectangle{pipe.x - 7.0f, gapTop - 27.0f, kPipeWidth + 14.0f, 30.0f},
                                 0.2f, 6, body);
            DrawRectangleRounded(Rectangle{pipe.x - 7.0f, gapBottom - 3.0f, kPipeWidth + 14.0f, 30.0f},
                                 0.2f, 6, body);
        }

        void drawBird(const Bird& bird, const FlappyTheme& theme)
        {
            const float rotation = std::clamp(bird.velocity * 0.055f, -22.0f, 48.0f);
            const Color body = color(theme.birdR, theme.birdG, theme.birdB);
            const Color wing = color(theme.birdR - 38, theme.birdG - 55, theme.birdB - 22);
            const Color outline = color(72, 54, 44);

            DrawCircleV(Vector2{kBirdX + 2.0f, bird.y + 3.0f}, kBirdRadius + 2.0f, outline);
            DrawCircleV(Vector2{kBirdX, bird.y}, kBirdRadius, body);
            DrawEllipse(static_cast<int>(kBirdX - 8.0f), static_cast<int>(bird.y + 5.0f), 13.0f, 8.0f, wing);
            DrawCircleV(Vector2{kBirdX + 8.0f, bird.y - 7.0f}, 6.0f, color(255, 255, 255));
            DrawCircleV(Vector2{kBirdX + 10.0f, bird.y - 7.0f}, 2.5f, color(28, 32, 40));
            DrawTriangle(Vector2{kBirdX + 15.0f, bird.y - 1.0f},
                         Vector2{kBirdX + 31.0f, bird.y + 4.0f + rotation * 0.02f},
                         Vector2{kBirdX + 15.0f, bird.y + 8.0f}, color(245, 116, 64));
        }

        void drawScene(const FlappyWorld& world, GameScript& script, const RoundState state,
                       const std::int32_t frames)
        {
            const FlappyTheme theme = script.theme();
            const int width = GetScreenWidth();
            const int height = GetScreenHeight();
            const Color skyTop = color(theme.skyR, theme.skyG, theme.skyB);
            const Color skyBottom = color(theme.skyR + 42, theme.skyG + 35, theme.skyB + 15);
            DrawRectangleGradientV(0, 0, width, height, skyTop, skyBottom);

            const float cloudOffset = std::fmod(static_cast<float>(frames) * 0.28f, static_cast<float>(width + 220));
            drawCloud(static_cast<float>(width) - cloudOffset, 150.0f, 0.9f);
            drawCloud(static_cast<float>(width) * 0.55f - cloudOffset * 0.45f, 250.0f, 0.62f);
            drawCloud(static_cast<float>(width) + 180.0f - cloudOffset * 0.72f, 355.0f, 0.75f);

            for (const Pipe& pipe : world.pipes())
                drawPipe(pipe, world.groundY(), theme);

            DrawRectangle(0, static_cast<int>(world.groundY()), width, static_cast<int>(kGroundHeight),
                          color(225, 196, 92));
            DrawRectangle(0, static_cast<int>(world.groundY()), width, 13, color(102, 188, 78));
            for (int x = -30; x < width + 30; x += 42)
            {
                const int scroll = (frames * 2) % 42;
                DrawRectangle(x - scroll, static_cast<int>(world.groundY() + 28.0f), 24, 7,
                              color(197, 157, 70));
            }

            drawBird(world.bird(), theme);

            DrawRectangle(0, 0, width, static_cast<int>(kHudHeight), color(24, 37, 55, 205));
            DrawText(script.title().c_str(), 22, 18, 27, color(244, 249, 255));
            const std::string stats = "BEST " + std::to_string(script.bestScore()) +
                "   LEVEL " + std::to_string(script.level());
            DrawText(stats.c_str(), width - MeasureText(stats.c_str(), 21) - 22, 23, 21, color(206, 229, 244));

            const std::string score = std::to_string(script.score());
            DrawText(score.c_str(), width / 2 - MeasureText(score.c_str(), 52) / 2 + 3, 91, 52, color(43, 67, 82, 150));
            DrawText(score.c_str(), width / 2 - MeasureText(score.c_str(), 52) / 2, 87, 52, color(255, 255, 255));

            if (state != RoundState::Playing)
            {
                const char* heading = state == RoundState::Ready ? "SKY HOPPER" : "CRASHED!";
                const char* prompt = state == RoundState::Ready
                    ? "SPACE / UP / CLICK TO FLY"
                    : "SPACE / CLICK TO TRY AGAIN";
                const Rectangle card{width * 0.5f - 220.0f, height * 0.5f - 90.0f, 440.0f, 165.0f};
                DrawRectangleRounded(card, 0.16f, 12, color(23, 36, 52, 228));
                DrawRectangleRoundedLinesEx(card, 0.16f, 12, 3.0f, color(255, 255, 255, 95));
                DrawText(heading, width / 2 - MeasureText(heading, 38) / 2,
                         static_cast<int>(card.y + 28.0f), 38, color(255, 221, 92));
                DrawText(prompt, width / 2 - MeasureText(prompt, 20) / 2,
                         static_cast<int>(card.y + 96.0f), 20, color(223, 236, 246));
            }
        }
    }

    RunSummary Engine::run(GameScript& script, const RunOptions& options)
    {
        script.start();
        FlappyWorld world(static_cast<float>(options.width), static_cast<float>(options.height));
        RoundState state = RoundState::Ready;
        RunSummary summary{};

        if (!options.headless)
        {
            SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
            InitWindow(options.width, options.height, script.title().c_str());
            SetTargetFPS(60);
        }
        else
        {
            world.beginRound(script);
            state = RoundState::Playing;
            ++summary.pipesSpawned;
            ++summary.restarts;
        }

        while (true)
        {
            if (!options.headless && WindowShouldClose())
                break;

            const float deltaTime = options.headless
                ? 1.0f / 60.0f
                : std::clamp(GetFrameTime(), 0.0f, 0.04f);
            bool flap = options.headless ? false : flapPressed();

            if (state != RoundState::Playing && (flap || options.headless))
            {
                world.beginRound(script);
                state = RoundState::Playing;
                ++summary.pipesSpawned;
                ++summary.restarts;
                flap = true;
            }

            const std::int32_t actions = script.tick(deltaTime, state == RoundState::Playing, options.headless);

            if (state == RoundState::Playing)
            {
                if (options.headless)
                    flap = headlessFlap(world);

                WorldEvents events = world.update(script, deltaTime, flap);
                summary.pipesSpawned += events.spawned;
                summary.pipesPassed += events.passed;

                if (options.headless && summary.crashes == 0 && summary.frames == 500)
                {
                    world.forceCrash();
                    events.crashed = true;
                }

                if (events.crashed)
                {
                    ++summary.crashes;
                    script.onCrash();
                    state = RoundState::GameOver;
                }
            }

            ++summary.frames;

            if (!options.headless)
            {
                BeginDrawing();
                drawScene(world, script, state, summary.frames);
                EndDrawing();
            }

            if ((actions & kActionExit) != 0)
                break;
            if (options.headless && summary.frames >= options.headlessFrames)
                break;
        }

        if (!options.headless)
            CloseWindow();

        summary.finalScore = script.score();
        summary.bestScore = script.bestScore();
        summary.level = script.level();
        summary.birdY = world.bird().y;
        return summary;
    }
}
