#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "console_backend.h"

namespace wio::runtime::console::detail
{
    class ConsoleState final
    {
    public:
        ConsoleState();

        std::recursive_mutex Mutex;
        std::unique_ptr<ConsoleBackend> Backend;
        TextReaderPtr In;
        TextWriterPtr Out;
        TextWriterPtr Error;
        std::string NewLine;
        std::string Title;
        ConsoleColor Foreground = ConsoleColor::Gray;
        ConsoleColor Background = ConsoleColor::Black;
        ConsoleEncoding InputEncoding = ConsoleEncoding::Utf8;
        ConsoleEncoding OutputEncoding = ConsoleEncoding::Utf8;
        bool TreatControlCAsInput = false;
        bool CursorVisible = true;
        int CursorSize = 25;
        CursorPosition Cursor {};
        ConsoleSize Buffer { .Width = 120, .Height = 30 };
        ConsoleRect Window { .Left = 0, .Top = 0, .Width = 120, .Height = 30 };
        ConsoleSize LargestWindow { .Width = 120, .Height = 30 };
    };

    [[nodiscard]] inline ConsoleState& State()
    {
        static ConsoleState instance;
        return instance;
    }
}
