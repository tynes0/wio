#pragma once

#include <string>
#include <string_view>

#include "../../../include/std_console.h"

namespace wio::runtime::console::detail
{
    class ConsoleBackend // NOLINT(cppcoreguidelines-special-member-functions)
    {
    public:
        virtual ~ConsoleBackend() = default;

        [[nodiscard]] virtual Result<ConsoleCapabilities> Capabilities() noexcept = 0;

        [[nodiscard]] virtual Result<ConsoleColor> ForegroundColor() noexcept = 0;
        [[nodiscard]] virtual Result<void> ForegroundColor(ConsoleColor color) noexcept = 0;
        [[nodiscard]] virtual Result<ConsoleColor> BackgroundColor() noexcept = 0;
        [[nodiscard]] virtual Result<void> BackgroundColor(ConsoleColor color) noexcept = 0;
        [[nodiscard]] virtual Result<void> ResetColor() noexcept = 0;

        [[nodiscard]] virtual Result<CursorPosition> GetCursorPosition() noexcept = 0;
        [[nodiscard]] virtual Result<void> SetCursorPosition(int left, int top) noexcept = 0;
        [[nodiscard]] virtual Result<bool> CursorVisible() noexcept = 0;
        [[nodiscard]] virtual Result<void> CursorVisible(bool visible) noexcept = 0;
        [[nodiscard]] virtual Result<int> CursorSize() noexcept = 0;
        [[nodiscard]] virtual Result<void> CursorSize(int size) noexcept = 0;

        [[nodiscard]] virtual Result<ConsoleSize> BufferSize() noexcept = 0;
        [[nodiscard]] virtual Result<void> SetBufferSize(int width, int height) noexcept = 0;

        [[nodiscard]] virtual Result<ConsoleRect> WindowRect() noexcept = 0;
        [[nodiscard]] virtual Result<void> SetWindowPosition(int left, int top) noexcept = 0;
        [[nodiscard]] virtual Result<void> SetWindowSize(int width, int height) noexcept = 0;
        [[nodiscard]] virtual Result<ConsoleSize> LargestWindowSize() noexcept = 0;

        [[nodiscard]] virtual Result<void> Clear() noexcept = 0;
        [[nodiscard]] virtual Result<void> MoveBufferArea(const MoveBufferAreaOptions& options) noexcept = 0;

        [[nodiscard]] virtual Result<ConsoleKeyInfo> ReadKey(bool intercept) = 0;
        [[nodiscard]] virtual Result<bool> KeyAvailable() noexcept = 0;

        [[nodiscard]] virtual Result<void> Beep() noexcept = 0;
        [[nodiscard]] virtual Result<void> Beep(int frequency, int durationMs) noexcept = 0;

        [[nodiscard]] virtual Result<std::string> Title() = 0;
        [[nodiscard]] virtual Result<void> Title(std::string_view value) = 0;

        [[nodiscard]] virtual Result<bool> CapsLock() noexcept = 0;
        [[nodiscard]] virtual Result<bool> NumberLock() noexcept = 0;

        [[nodiscard]] virtual Result<bool> TreatControlCAsInput() noexcept = 0;
        [[nodiscard]] virtual Result<void> TreatControlCAsInput(bool value) noexcept = 0;

        [[nodiscard]] virtual Result<bool> IsInputRedirected() noexcept = 0;
        [[nodiscard]] virtual Result<bool> IsOutputRedirected() noexcept = 0;
        [[nodiscard]] virtual Result<bool> IsErrorRedirected() noexcept = 0;

        [[nodiscard]] virtual Result<ConsoleEncoding> InputEncoding() noexcept = 0;
        [[nodiscard]] virtual Result<void> InputEncoding(ConsoleEncoding encoding) noexcept = 0;
        [[nodiscard]] virtual Result<ConsoleEncoding> OutputEncoding() noexcept = 0;
        [[nodiscard]] virtual Result<void> OutputEncoding(ConsoleEncoding encoding) noexcept = 0;
    };

}
