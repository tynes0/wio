/*
 * This file is part of the dtlog library, originally created by Tynes0.
 * For the latest version and updates, please visit the official dtlog GitHub repository:
 * https://github.com/tynes0/dtlog
 *
 * dtlog is a basic library for logging, providing fast and user-friendly use
 * It is released under the MIT License. See the LICENSE file in the root of the dtlog repository
 * or visit the above GitHub link for more details.
 *
 * For contributions, bug reports, or other inquiries, feel free to contact the author:
 * - GitHub: https://github.com/tynes0
 * - Email: cihanbilgihan@gmail.com
 */

#include "dtlog.h"
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace
{
    WORD get_win_color_code(dtlog::log_level level)
    {
        switch (level)
        {
        case dtlog::log_level::info:     return FOREGROUND_GREEN;
        case dtlog::log_level::debug:    return FOREGROUND_BLUE | FOREGROUND_INTENSITY; 
        case dtlog::log_level::warning:  return FOREGROUND_RED | FOREGROUND_GREEN; 
        case dtlog::log_level::error:    return FOREGROUND_RED;
        case dtlog::log_level::critical: return BACKGROUND_RED | FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case dtlog::log_level::trace:    return FOREGROUND_INTENSITY; 
        case dtlog::log_level::none:
            break;
        }
        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; 
    }

    void set_console_color(DWORD handle, dtlog::log_level level)
    {
        HANDLE hConsole = GetStdHandle(handle);
        if (hConsole != INVALID_HANDLE_VALUE) {
            SetConsoleTextAttribute(hConsole, get_win_color_code(level));
        }
    }

    void reset_console_color(DWORD handle)
    {
        HANDLE hConsole = GetStdHandle(handle);
        if (hConsole != INVALID_HANDLE_VALUE) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
    }
}

#else // (Linux, macOS, Unix)

#include <cstring>

namespace
{
    const char* get_ansi_color_code(dtlog::log_level level)
    {
        switch (level)
        {
        case dtlog::log_level::info:     return "\x1b[32m"; 
        case dtlog::log_level::debug:    return "\x1b[36m"; 
        case dtlog::log_level::warning:  return "\x1b[33m"; 
        case dtlog::log_level::error:    return "\x1b[31m"; 
        case dtlog::log_level::critical: return "\x1b[41m\x1b[37m"; 
        case dtlog::log_level::trace:    return "\x1b[90m"; 
        case dtlog::log_level::none:
            break;
        }
        return "\x1b[0m";
    }
}

#endif

namespace dtlog
{
    void console_sink::log(const std::string& msg, log_level level)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

#ifdef _WIN32
        set_console_color(STD_OUTPUT_HANDLE, level);
        DTLOG_UNUSED(std::fwrite(msg.c_str(), 1, msg.length(), stdout));
        reset_console_color(STD_OUTPUT_HANDLE);
#else
        const char* color_code = get_ansi_color_code(level);
        const char* reset_code = "\x1b[0m";
        
        DTLOG_UNUSED(std::fwrite(color_code, 1, std::strlen(color_code), stdout));
        DTLOG_UNUSED(std::fwrite(msg.c_str(), 1, msg.length(), stdout));
        DTLOG_UNUSED(std::fwrite(reset_code, 1, 4, stdout));
#endif
        if (should_flush(level))
            DTLOG_UNUSED(std::fflush(stdout));
    }

    void err_console_sink::log(const std::string& msg, log_level level)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

#ifdef _WIN32
        set_console_color(STD_ERROR_HANDLE, level);
        DTLOG_UNUSED(std::fwrite(msg.c_str(), 1, msg.length(), stderr));
        reset_console_color(STD_ERROR_HANDLE);
#else
        const char* color_code = get_ansi_color_code(level);
        const char* reset_code = "\x1b[0m";
        
        DTLOG_UNUSED(std::fwrite(color_code, 1, std::strlen(color_code), stderr));
        DTLOG_UNUSED(std::fwrite(msg.c_str(), 1, msg.length(), stderr));
        DTLOG_UNUSED(std::fwrite(reset_code, 1, 4, stderr));
#endif
        if (should_flush(level))
            DTLOG_UNUSED(std::fflush(stderr));
    }

    void debug_sink::log(const std::string& msg, log_level level)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        DTLOG_UNUSED(level);
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }
}