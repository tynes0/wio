#pragma once

#include <cmath>
#include "raylib.h"

// Only string/ABI adaptation lives here. All scene drawing is Wio code.
namespace pacman_raylib
{
    inline void Open(int width, int height, const char* title)
    {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(width, height, title);
        SetWindowMinSize(800, 780);
        SetTargetFPS(60);
    }

    inline void Text(const char* value, float x, float y, int size, Color color)
    {
        DrawText(value, static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), size, color);
    }

    inline int TextWidth(const char* value, int size)
    {
        return MeasureText(value, size);
    }

    inline void Screenshot(const char* path)
    {
        TakeScreenshot(path);
    }
}

