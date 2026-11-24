#pragma once

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include "raylib.h"

// String/font-resource adaptation only. All scene drawing is Wio code.
namespace pacman_raylib
{
    struct UiFonts
    {
        Font body{};
        Font heading{};
    };

    inline UiFonts fonts;
    inline constexpr float TextScale = 1.2f;

    inline std::string FontPath(const char* relative)
    {
        if (FileExists(relative)) return relative;

        const auto directory = std::string(GetApplicationDirectory());
        const auto besideExecutable = directory + relative;
        if (FileExists(besideExecutable.c_str())) return besideExecutable;

        const auto projectAsset = directory + "../../" + relative;
        if (FileExists(projectAsset.c_str())) return projectAsset;

        throw std::runtime_error(std::string("Missing Pacman UI font: ") + relative);
    }

    inline Font LoadUiFont(const char* relative, int size)
    {
        // Load UI glyphs and Turkish letters. This bundled subset has no grave
        // accent glyph; requesting it would warn on every launch.
        std::array<int, 106> codepoints{};
        int count = 0;
        for (int value = 32; value <= 126; ++value)
        {
            if (value == 96) continue;
            codepoints[count++] = value;
        }

        for (const int value : {0x00c7, 0x00e7, 0x00d6, 0x00f6, 0x00dc, 0x00fc,
                 0x011e, 0x011f, 0x0130, 0x0131, 0x015e, 0x015f})
        {
            codepoints[count++] = value;
        }

        const auto path = FontPath(relative);
        const auto font = LoadFontEx(path.c_str(), size, codepoints.data(), count);
        const auto defaultTexture = GetFontDefault().texture.id;
        if (!IsFontValid(font) || font.texture.id == 0 || font.texture.id == defaultTexture)
        {
            if (font.texture.id != defaultTexture) UnloadFont(font);
            throw std::runtime_error(std::string("Could not load Pacman UI font: ") + path);
        }

        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
        return font;
    }

    inline void UnloadFonts()
    {
        if (fonts.body.texture.id != 0)
        {
            UnloadFont(fonts.body);
            fonts.body = {};
        }

        if (fonts.heading.texture.id != 0)
        {
            UnloadFont(fonts.heading);
            fonts.heading = {};
        }
    }

    inline void Open(int width, int height, const char* title)
    {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(width, height, title);
        if (!IsWindowReady()) throw std::runtime_error("Could not open the Pacman window.");
        SetWindowMinSize(800, 780);
        SetTargetFPS(60);
        try
        {
            fonts.body = LoadUiFont("assets/fonts/Manrope-Medium.ttf", 32);
            fonts.heading = LoadUiFont("assets/fonts/Manrope-Bold.ttf", 64);
        }
        catch (...)
        {
            UnloadFonts();
            CloseWindow();
            throw;
        }
    }

    inline void Close()
    {
        UnloadFonts();
        if (IsWindowReady()) CloseWindow();
    }

    inline const Font& TextFont(int size)
    {
        if (fonts.body.texture.id == 0 || fonts.heading.texture.id == 0)
        {
            throw std::logic_error("Open the Pacman window before drawing text.");
        }

        return size >= 24 ? fonts.heading : fonts.body;
    }

    inline void Text(const char* value, float x, float y, int size, Color color)
    {
        DrawTextEx(TextFont(size), value, Vector2{std::round(x), std::round(y)},
            static_cast<float>(size) * TextScale, 0.3f, color);
    }

    inline int TextWidth(const char* value, int size)
    {
        return static_cast<int>(std::ceil(MeasureTextEx(TextFont(size), value,
            static_cast<float>(size) * TextScale, 0.3f).x));
    }

    inline int TextHeight(const char* value, int size)
    {
        return static_cast<int>(std::ceil(MeasureTextEx(TextFont(size), value,
            static_cast<float>(size) * TextScale, 0.3f).y));
    }

    inline void Screenshot(const char* path)
    {
        TakeScreenshot(path);
    }
}
