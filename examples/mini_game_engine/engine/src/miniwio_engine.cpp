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
        constexpr std::int32_t kActionSpawn = 1;
        constexpr std::int32_t kActionExit = 2;

        struct Entity
        {
            std::int32_t id = 0;
            Vec2 position{};
            float radius = 14.0f;
        };

        class World
        {
        public:
            World(const float width, const float height)
                : width_(width), height_(height), player_{1, {width * 0.5f, height * 0.5f}, 18.0f}
            {
            }

            void spawnEnemy(const bool closeToPlayer = false)
            {
                Vec2 position{};
                if (closeToPlayer)
                {
                    position = {player_.position.x + 24.0f, player_.position.y};
                }
                else
                {
                    position = {xDistribution_(random_), yDistribution_(random_)};
                }

                enemies_.push_back(Entity{nextId_++, position, 13.0f});
            }

            void movePlayer(const Vec2 velocity, const float deltaTime)
            {
                player_.position.x = std::clamp(
                    player_.position.x + velocity.x * deltaTime,
                    player_.radius,
                    width_ - player_.radius);
                player_.position.y = std::clamp(
                    player_.position.y + velocity.y * deltaTime,
                    player_.radius,
                    height_ - player_.radius);
            }

            std::int32_t updateEnemies(GameScript& script, const float deltaTime)
            {
                std::int32_t collisions = 0;
                for (Entity& enemy : enemies_)
                {
                    const float deltaX = player_.position.x - enemy.position.x;
                    const float deltaY = player_.position.y - enemy.position.y;
                    const Vec2 velocity = script.enemyVelocity(deltaX, deltaY);
                    enemy.position.x += velocity.x * deltaTime;
                    enemy.position.y += velocity.y * deltaTime;

                    const float afterX = player_.position.x - enemy.position.x;
                    const float afterY = player_.position.y - enemy.position.y;
                    const float combinedRadius = player_.radius + enemy.radius;
                    if (afterX * afterX + afterY * afterY <= combinedRadius * combinedRadius)
                    {
                        ++collisions;
                        script.onCollision();
                        enemy.position = {xDistribution_(random_), yDistribution_(random_)};
                    }
                }
                return collisions;
            }

            [[nodiscard]] const Entity& player() const noexcept { return player_; }
            [[nodiscard]] const std::vector<Entity>& enemies() const noexcept { return enemies_; }
            [[nodiscard]] std::int32_t enemyCount() const noexcept
            {
                return static_cast<std::int32_t>(enemies_.size());
            }

        private:
            float width_ = 1000.0f;
            float height_ = 700.0f;
            Entity player_{};
            std::vector<Entity> enemies_{};
            std::int32_t nextId_ = 2;
            std::mt19937 random_{0x57494fu};
            std::uniform_real_distribution<float> xDistribution_{50.0f, 950.0f};
            std::uniform_real_distribution<float> yDistribution_{90.0f, 650.0f};
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

        Vec2 windowInput()
        {
            Vec2 result{};
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) result.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) result.x += 1.0f;
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) result.y -= 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) result.y += 1.0f;
            return result;
        }

        Vec2 headlessInput(const std::int32_t frame)
        {
            const std::int32_t phase = (frame / 45) % 4;
            if (phase == 0) return {1.0f, 0.0f};
            if (phase == 1) return {0.0f, 1.0f};
            if (phase == 2) return {-1.0f, 0.0f};
            return {0.0f, -1.0f};
        }

        void drawWorld(const World& world, GameScript& script)
        {
            const VisualTheme theme = script.theme();
            ClearBackground(color(theme.backgroundR, theme.backgroundG, theme.backgroundB));

            const int width = GetScreenWidth();
            const int height = GetScreenHeight();
            for (int x = 0; x < width; x += 50)
                DrawLine(x, 0, x, height, color(255, 255, 255, 18));
            for (int y = 0; y < height; y += 50)
                DrawLine(0, y, width, y, color(255, 255, 255, 18));

            const Entity& player = world.player();
            DrawCircleV(Vector2{player.position.x, player.position.y}, player.radius,
                        color(theme.accentR, theme.accentG, theme.accentB));
            DrawCircleLinesV(Vector2{player.position.x, player.position.y}, player.radius + 3.0f,
                             color(225, 255, 245));

            for (const Entity& enemy : world.enemies())
            {
                DrawCircleV(Vector2{enemy.position.x, enemy.position.y}, enemy.radius, color(242, 91, 112));
                DrawCircleLinesV(Vector2{enemy.position.x, enemy.position.y}, enemy.radius + 2.0f,
                                 color(255, 190, 200));
            }

            DrawRectangle(0, 0, width, 66, color(10, 13, 20, 235));
            DrawText(script.title().c_str(), 22, 16, 26, color(235, 242, 255));
            const std::string stats = "Score " + std::to_string(script.score()) +
                "   Wave " + std::to_string(script.wave()) +
                "   Enemies " + std::to_string(world.enemyCount());
            DrawText(stats.c_str(), width - MeasureText(stats.c_str(), 20) - 22, 22, 20, color(170, 184, 210));
            DrawText("WASD / arrows move - behavior comes from Wio", 22, height - 34, 18, color(150, 165, 190));
        }
    }

    RunSummary Engine::run(GameScript& script, const RunOptions& options)
    {
        script.start();
        World world(static_cast<float>(options.width), static_cast<float>(options.height));
        world.spawnEnemy(true);
        world.spawnEnemy();
        world.spawnEnemy();

        RunSummary summary{};
        summary.spawned = 3;

        if (!options.headless)
        {
            SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
            InitWindow(options.width, options.height, script.title().c_str());
            SetWindowMinSize(720, 480);
            SetTargetFPS(60);
        }

        while (true)
        {
            if (!options.headless && WindowShouldClose())
                break;

            const float deltaTime = options.headless
                ? 1.0f / 60.0f
                : std::clamp(GetFrameTime(), 0.0f, 0.05f);
            const Vec2 input = options.headless ? headlessInput(summary.frames) : windowInput();
            const std::int32_t actions = script.tick(
                input.x, input.y, deltaTime, world.enemyCount(), options.headless);

            world.movePlayer(script.playerVelocity(input.x, input.y), deltaTime);
            summary.collisions += world.updateEnemies(script, deltaTime);

            if ((actions & kActionSpawn) != 0 && world.enemyCount() < 14)
            {
                world.spawnEnemy();
                ++summary.spawned;
            }

            ++summary.frames;

            if (!options.headless)
            {
                BeginDrawing();
                drawWorld(world, script);
                EndDrawing();
            }

            if ((actions & kActionExit) != 0)
                break;
            if (options.headless && summary.frames >= options.headlessFrames)
                break;
        }

        if (!options.headless)
            CloseWindow();

        summary.finalEnemies = world.enemyCount();
        summary.finalScore = script.score();
        summary.finalWave = script.wave();
        summary.player = world.player().position;
        return summary;
    }
}
