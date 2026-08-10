#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "intrinsics.h"
#include "raylib.h"

namespace atlas_native
{
    struct FontHandle
    {
        Font font{};
        bool owns = false;
    };

    inline std::string Utf8Path(const std::filesystem::path& value)
    {
        const auto encoded = value.generic_u8string();
        return std::string(encoded.begin(), encoded.end());
    }

    inline std::string JsonEscape(std::string_view value)
    {
        std::ostringstream out;
        for (const unsigned char character : value)
        {
            switch (character)
            {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (character < 0x20)
                {
                    constexpr char hex[] = "0123456789abcdef";
                    out << "\\u00" << hex[(character >> 4) & 0x0f] << hex[character & 0x0f];
                }
                else
                {
                    out << static_cast<char>(character);
                }
                break;
            }
        }
        return out.str();
    }

    inline bool IsIgnoredDirectory(const std::filesystem::path& path)
    {
        const std::string name = path.filename().string();
        return name == ".git" || name == ".wio-build" || name == ".wio-qualification" ||
               name == "node_modules" || name == "artifacts" || name == "dist" ||
               name == "build" || name.rfind("build-", 0) == 0;
    }

    inline std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    inline bool IsTextExtension(const std::string& extension)
    {
        return extension == ".wio" || extension == ".c" || extension == ".cc" ||
               extension == ".cpp" || extension == ".cxx" || extension == ".h" ||
               extension == ".hpp" || extension == ".md" || extension == ".txt" ||
               extension == ".json" || extension == ".toml" || extension == ".yaml" ||
               extension == ".yml" || extension == ".cmake";
    }

    struct FileRecord
    {
        std::string path;
        std::string extension;
        std::uint64_t bytes = 0;
        std::int64_t modified = 0;
    };

    inline wio::String ScanWorkspaceJson(const wio::String& requestedPath, std::uint64_t maxFiles)
    {
        try
        {
            std::filesystem::path root = requestedPath.empty()
                ? std::filesystem::current_path()
                : std::filesystem::absolute(std::filesystem::path(requestedPath.c_str()));
            root = root.lexically_normal();

            if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root))
            {
                return "{\"ok\":false,\"error\":\"Workspace directory does not exist.\"}";
            }

            std::uint64_t files = 0;
            std::uint64_t directories = 0;
            std::uint64_t bytes = 0;
            std::uint64_t lines = 0;
            std::uint64_t wioFiles = 0;
            std::uint64_t nativeFiles = 0;
            std::uint64_t docs = 0;
            std::uint64_t configs = 0;
            std::uint64_t other = 0;
            bool hasManifest = false;
            bool hasReadme = false;
            bool hasTests = false;
            bool truncated = false;
            std::vector<FileRecord> recent;

            std::error_code error;
            std::filesystem::recursive_directory_iterator iterator(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                error);
            const std::filesystem::recursive_directory_iterator end;

            while (iterator != end)
            {
                if (error)
                {
                    error.clear();
                    iterator.increment(error);
                    continue;
                }

                const auto entry = *iterator;
                const auto path = entry.path();
                if (entry.is_directory(error))
                {
                    ++directories;
                    if (IsIgnoredDirectory(path)) iterator.disable_recursion_pending();
                    if (Lower(path.filename().string()) == "tests") hasTests = true;
                    iterator.increment(error);
                    continue;
                }

                if (!entry.is_regular_file(error))
                {
                    iterator.increment(error);
                    continue;
                }

                if (files >= maxFiles)
                {
                    truncated = true;
                    break;
                }

                ++files;
                const std::string filename = Lower(path.filename().string());
                const std::string extension = Lower(path.extension().string());
                const std::uint64_t size = static_cast<std::uint64_t>(entry.file_size(error));
                bytes += error ? 0 : size;
                error.clear();

                if (filename == "wio.makewio" || filename == "wio.project.json") hasManifest = true;
                if (filename == "readme.md" || filename == "readme.txt") hasReadme = true;

                if (extension == ".wio") ++wioFiles;
                else if (extension == ".c" || extension == ".cc" || extension == ".cpp" ||
                         extension == ".cxx" || extension == ".h" || extension == ".hpp") ++nativeFiles;
                else if (extension == ".md" || extension == ".txt") ++docs;
                else if (extension == ".json" || extension == ".toml" || extension == ".yaml" ||
                         extension == ".yml" || extension == ".cmake" || filename == "wio.makewio") ++configs;
                else ++other;

                if (size <= 1024u * 1024u && IsTextExtension(extension))
                {
                    std::ifstream input(path, std::ios::binary);
                    lines += static_cast<std::uint64_t>(std::count(
                        std::istreambuf_iterator<char>(input),
                        std::istreambuf_iterator<char>(),
                        '\n'));
                }

                const auto writeTime = entry.last_write_time(error);
                const std::int64_t modified = error ? 0 : static_cast<std::int64_t>(writeTime.time_since_epoch().count());
                error.clear();
                recent.push_back(FileRecord{
                    Utf8Path(std::filesystem::relative(path, root, error)), extension, size, modified
                });
                error.clear();
                iterator.increment(error);
            }

            std::sort(recent.begin(), recent.end(), [](const FileRecord& left, const FileRecord& right)
            {
                return left.modified > right.modified;
            });
            if (recent.size() > 12) recent.resize(12);

            std::vector<std::string> insights;
            if (!hasManifest) insights.emplace_back("No Wio project manifest was found.");
            if (!hasReadme) insights.emplace_back("Add a README to explain the workspace.");
            if (!hasTests) insights.emplace_back("No tests directory was detected.");
            if (wioFiles == 0) insights.emplace_back("This workspace contains no Wio source files.");
            if (truncated) insights.emplace_back("The scan reached its configured file limit.");
            if (insights.empty()) insights.emplace_back("Workspace foundations look healthy.");

            int health = 100;
            if (!hasManifest) health -= 25;
            if (!hasReadme) health -= 15;
            if (!hasTests) health -= 20;
            if (wioFiles == 0) health -= 30;
            if (truncated) health -= 10;
            health = std::max(0, health);

            std::ostringstream out;
            out << "{\"ok\":true,\"root\":\"" << JsonEscape(Utf8Path(root)) << "\""
                << ",\"files\":" << files
                << ",\"directories\":" << directories
                << ",\"bytes\":" << bytes
                << ",\"lines\":" << lines
                << ",\"health\":" << health
                << ",\"truncated\":" << (truncated ? "true" : "false")
                << ",\"kinds\":["
                << "{\"name\":\"Wio\",\"count\":" << wioFiles << "},"
                << "{\"name\":\"Native\",\"count\":" << nativeFiles << "},"
                << "{\"name\":\"Docs\",\"count\":" << docs << "},"
                << "{\"name\":\"Config\",\"count\":" << configs << "},"
                << "{\"name\":\"Other\",\"count\":" << other << "}]"
                << ",\"recent\":[";

            for (std::size_t index = 0; index < recent.size(); ++index)
            {
                if (index != 0) out << ',';
                const FileRecord& item = recent[index];
                out << "{\"path\":\"" << JsonEscape(item.path) << "\",\"extension\":\""
                    << JsonEscape(item.extension) << "\",\"bytes\":" << item.bytes << '}';
            }

            out << "],\"insights\":[";
            for (std::size_t index = 0; index < insights.size(); ++index)
            {
                if (index != 0) out << ',';
                out << '"' << JsonEscape(insights[index]) << '"';
            }
            out << "]}";
            return out.str();
        }
        catch (const std::exception& exception)
        {
            return "{\"ok\":false,\"error\":\"" + JsonEscape(exception.what()) + "\"}";
        }
        catch (...)
        {
            return "{\"ok\":false,\"error\":\"Unknown workspace scan failure.\"}";
        }
    }

    inline void Init(const int width, const int height, const wio::String& title)
    {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
        InitWindow(width, height, title.c_str());
        SetWindowMinSize(1100, 700);
        SetTargetFPS(60);
    }

    inline void Shutdown() { CloseWindow(); }
    inline bool ShouldClose() { return WindowShouldClose(); }
    inline int Width() { return GetScreenWidth(); }
    inline int Height() { return GetScreenHeight(); }
    inline float Delta() { return GetFrameTime(); }
    inline void Begin() { BeginDrawing(); }
    inline void End() { EndDrawing(); }
    inline void Clear(const Color color) { ClearBackground(color); }
    inline void Rect(const Rectangle bounds, const Color color) { DrawRectangleRec(bounds, color); }
    inline void RoundRect(const Rectangle bounds, const float roundness, const Color color)
    {
        DrawRectangleRounded(bounds, roundness, 12, color);
    }
    inline void RoundLine(const Rectangle bounds, const float roundness, const float thick, const Color color)
    {
        DrawRectangleRoundedLinesEx(bounds, roundness, 12, thick, color);
    }
    inline void Line(const float x1, const float y1, const float x2, const float y2, const float thick, const Color color)
    {
        DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, thick, color);
    }
    inline void Circle(const float x, const float y, const float radius, const Color color)
    {
        DrawCircleV(Vector2{x, y}, radius, color);
    }

    inline void* LoadFontHandle(const wio::String& path, const int size)
    {
        auto* handle = new FontHandle{};
        if (!path.empty() && FileExists(path.c_str()))
        {
            handle->font = LoadFontEx(path.c_str(), size, nullptr, 0);
            handle->owns = handle->font.texture.id != 0;
        }
        if (!handle->owns) handle->font = GetFontDefault();
        return handle;
    }

    inline void UnloadFontHandle(void* value)
    {
        auto* handle = static_cast<FontHandle*>(value);
        if (!handle) return;
        if (handle->owns) UnloadFont(handle->font);
        delete handle;
    }

    inline void DrawFontText(void* value, const wio::String& text, const float x, const float y,
                             const float size, const float spacing, const Color color)
    {
        auto* handle = static_cast<FontHandle*>(value);
        if (handle) DrawTextEx(handle->font, text.c_str(), Vector2{x, y}, size, spacing, color);
    }

    inline float MeasureFontText(void* value, const wio::String& text, const float size, const float spacing)
    {
        auto* handle = static_cast<FontHandle*>(value);
        return handle ? MeasureTextEx(handle->font, text.c_str(), size, spacing).x : 0.0f;
    }

    inline float MouseX() { return static_cast<float>(GetMouseX()); }
    inline float MouseY() { return static_cast<float>(GetMouseY()); }
    inline bool LeftPressed() { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
    inline bool KeyPressed(const int key) { return IsKeyPressed(key); }
    inline bool KeyDown(const int key) { return IsKeyDown(key); }
    inline int Character() { return GetCharPressed(); }
    inline void Cursor(const int cursor) { SetMouseCursor(cursor); }
    inline void Clipboard(const wio::String& text) { SetClipboardText(text.c_str()); }
    inline void Screenshot(const wio::String& path) { TakeScreenshot(path.c_str()); }

    inline wio::String DroppedPath()
    {
        if (!IsFileDropped()) return "";
        const FilePathList paths = LoadDroppedFiles();
        std::string result;
        if (paths.count > 0 && paths.paths && paths.paths[0]) result = paths.paths[0];
        UnloadDroppedFiles(paths);
        return result;
    }
}
