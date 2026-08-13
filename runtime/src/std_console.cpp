#include "../include/std_console.h"
#include "detail/io_detail/console_state.h"
#include "detail/io_detail/buffered_text_writer.h"
#include "detail/io_detail/file_text_reader.h"
#include "detail/io_detail/file_text_writer.h"
#include "detail/io_detail/io_helpers.h"

#if defined(_WIN32)
    #include "detail/io_detail/windows_console_text_reader.h"
#include "detail/io_detail/windows_console_text_writer.h"
#endif

namespace wio::runtime::std_console
{
    namespace
    {
        [[nodiscard]] console::ConsoleError& LastStoredError() noexcept
        {
            static thread_local console::ConsoleError error {};
            return error;
        }

        [[nodiscard]] StatusCode ToStatusCode(const console::ConsoleError error) noexcept
        {
            return static_cast<StatusCode>(error.Status);
        }

        [[nodiscard]] std::int32_t ToStatusI32(const console::ConsoleError error) noexcept
        {
            return static_cast<std::int32_t>(ToStatusCode(error));
        }

        template <typename T>
        [[nodiscard]] StatusCode StoreResult(console::Result<T> result, T* out = nullptr) noexcept
        {
            if (!result)
            {
                detail::StoreLastError(result.Error());
                return ToStatusCode(result.Error());
            }

            if (out != nullptr)
                *out = std::move(result).Value();

            detail::ClearStoredLastError();
            return static_cast<StatusCode>(Status::Ok);
        }

        [[nodiscard]] StatusCode StoreResult(console::Result<void> result) noexcept
        {
            if (!result)
            {
                detail::StoreLastError(result.Error());
                return ToStatusCode(result.Error());
            }

            detail::ClearStoredLastError();
            return static_cast<StatusCode>(Status::Ok);
        }

        template <typename T, typename Transform>
        [[nodiscard]] StatusCode StoreTransformed(console::Result<T> result, Transform&& transform) noexcept
        {
            if (!result)
            {
                detail::StoreLastError(result.Error());
                return ToStatusCode(result.Error());
            }

            transform(std::move(result).Value());
            detail::ClearStoredLastError();
            return static_cast<StatusCode>(Status::Ok);
        }

        template <typename Callable>
        [[nodiscard]] std::int32_t WriteStatus(Callable&& callable)
        {
            auto result = callable();
            if (!result)
            {
                detail::StoreLastError(result.Error());
                return ToStatusI32(result.Error());
            }

            detail::ClearStoredLastError();
            return 0;
        }

        template <typename Callable>
        [[nodiscard]] std::string StringResult(Callable&& callable)
        {
            auto result = callable();
            if (!result)
            {
                detail::StoreLastError(result.Error());
                return {};
            }

            detail::ClearStoredLastError();
            return std::move(result).Value();
        }

        template <typename Callable>
        [[nodiscard]] char CharResult(Callable&& callable)
        {
            auto result = callable();
            if (!result)
            {
                detail::StoreLastError(result.Error());
                return '\0';
            }

            detail::ClearStoredLastError();
            return static_cast<char>(result.Value());
        }
    }

    namespace detail
    {
        void StoreLastError(const console::ConsoleError& error) noexcept
        {
            LastStoredError() = error.Ok()
                ? console::MakeConsoleError(console::ConsoleStatus::UnknownError)
                : error;
        }

        void ClearStoredLastError() noexcept
        {
            LastStoredError() = {};
        }
    }

    void ClearLastError() noexcept
    {
        detail::ClearStoredLastError();
    }

    [[nodiscard]] StatusCode LastStatus() noexcept
    {
        return static_cast<StatusCode>(LastStoredError().Status);
    }

    [[nodiscard]] ErrorDomainCode LastErrorDomain() noexcept
    {
        return static_cast<ErrorDomainCode>(LastStoredError().Domain);
    }

    [[nodiscard]] int LastNativeCode() noexcept
    {
        return LastStoredError().NativeCode;
    }

    [[nodiscard]] int LastErrorLine() noexcept
    {
        return LastStoredError().Line;
    }

    [[nodiscard]] std::string LastErrorFile()
    {
        return LastStoredError().File != nullptr
            ? std::string(LastStoredError().File)
            : std::string {};
    }

    [[nodiscard]] std::string StatusName(const StatusCode status)
    {
        return std::string(console::ToString(static_cast<Status>(status)));
    }

    [[nodiscard]] std::string ErrorDomainName(const ErrorDomainCode domain)
    {
        return std::string(console::ToString(static_cast<ErrorDomain>(domain)));
    }

    std::int32_t WriteValue(const bool value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const char value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::int8_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::int16_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::int32_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::int64_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::uint8_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::uint16_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::uint32_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::uint64_t value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const float value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const double value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const char* value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(char* value)
    {
        return WriteValue(static_cast<const char*>(value));
    }

    std::int32_t WriteValue(const std::string& value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const std::string_view value)
    {
        return WriteStatus([&] { return console::Write(value); });
    }

    std::int32_t WriteValue(const wio::runtime::Text& value)
    {
        return WriteValue(value.Utf8());
    }

    std::int32_t WriteLine() noexcept
    {
        return WriteStatus([] { return console::WriteLine(); });
    }

    std::int32_t WriteLineValue(const bool value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const char value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::int8_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::int16_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::int32_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::int64_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::uint8_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::uint16_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::uint32_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::uint64_t value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const float value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const double value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const char* value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(char* value)
    {
        return WriteLineValue(static_cast<const char*>(value));
    }

    std::int32_t WriteLineValue(const std::string& value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const std::string_view value)
    {
        return WriteStatus([&] { return console::WriteLine(value); });
    }

    std::int32_t WriteLineValue(const wio::runtime::Text& value)
    {
        return WriteLineValue(value.Utf8());
    }

    std::int32_t WriteSegment(const std::string_view value, const std::size_t index, const std::size_t count)
    {
        return WriteStatus([&] { return console::Write(value, index, count); });
    }

    std::int32_t WriteBuffer(const char* buffer, const int index, const int count) noexcept
    {
        return WriteStatus([&] { return console::Write(buffer, index, count); });
    }

    std::int32_t WriteTextDecimal(const std::string_view value)
    {
        return WriteStatus([&] { return console::Write(console::TextDecimal(std::string(value))); });
    }

    std::int32_t WriteLineTextDecimal(const std::string_view value)
    {
        return WriteStatus([&] { return console::WriteLine(console::TextDecimal(std::string(value))); });
    }

    std::int32_t WriteErrorText(const std::string_view value) noexcept
    {
        return WriteStatus([&]() -> console::Result<console::IoCount>
        {
            auto error = console::Error();
            if (!error)
                return error.Error();
            return error.Value().Write(value);
        });
    }

    std::int32_t WriteErrorLine() noexcept
    {
        return WriteStatus([&]() -> console::Result<console::IoCount>
        {
            auto error = console::Error();
            if (!error)
                return error.Error();

            auto newline = console::NewLine();
            if (!newline)
                return newline.Error();

            return error.Value().Write(newline.Value());
        });
    }

    std::int32_t WriteErrorLineText(const std::string_view value) noexcept
    {
        return WriteStatus([&]() -> console::Result<console::IoCount>
        {
            auto error = console::Error();
            if (!error)
                return error.Error();

            auto first = error.Value().Write(value);
            if (!first)
                return first;

            auto newline = console::NewLine();
            if (!newline)
                return newline.Error();

            auto second = error.Value().Write(newline.Value());
            if (!second)
                return second;

            return console::detail::SumIoCount(first.Value(), second.Value());
        });
    }

    std::string Input()
    {
        return StringResult([] { return console::ReadLine(); });
    }

    std::string Input(const std::string& prompt)
    {
        const auto status = WriteValue(prompt);
        if (status != 0)
            return {};

        return Input();
    }

    std::string InputN(const std::size_t count)
    {
        return StringResult([&] { return console::ReadCount(count); });
    }

    char InputChar(const bool isHidden)
    {
        return isHidden
            ? CharResult([] { return console::ReadHidden(); })
            : CharResult([] { return console::Read(); });
    }

    char InputChar()
    {
        return InputChar(false);
    }

    std::string InputWord()
    {
        return StringResult([] { return console::ReadWord(); });
    }

    std::string InputUntil(const char delimiter, const bool includeDelimiter)
    {
        return StringResult([&] { return console::ReadUntil(delimiter, includeDelimiter); });
    }

    std::string InputUntil(const char delimiter)
    {
        return InputUntil(delimiter, false);
    }

    [[nodiscard]] StatusCode Capabilities(
        bool& colors,
        bool& cursorPosition,
        bool& cursorVisibility,
        bool& cursorSize,
        bool& bufferSize,
        bool& windowPosition,
        bool& windowSizeGet,
        bool& windowSizeSet,
        bool& largestWindowSize,
        bool& moveBufferArea,
        bool& keyAvailable,
        bool& readKey,
        bool& title,
        bool& beep,
        bool& beepFrequency,
        bool& keyboardToggleState,
        bool& treatControlCAsInput,
        bool& encodingSet) noexcept
    {
        return StoreTransformed(console::Capabilities(), [&](const console::ConsoleCapabilities& value)
        {
            colors = value.Colors;
            cursorPosition = value.CursorPosition;
            cursorVisibility = value.CursorVisibility;
            cursorSize = value.CursorSize;
            bufferSize = value.BufferSize;
            windowPosition = value.WindowPosition;
            windowSizeGet = value.WindowSizeGet;
            windowSizeSet = value.WindowSizeSet;
            largestWindowSize = value.LargestWindowSize;
            moveBufferArea = value.MoveBufferArea;
            keyAvailable = value.KeyAvailable;
            readKey = value.ReadKey;
            title = value.Title;
            beep = value.Beep;
            beepFrequency = value.BeepFrequency;
            keyboardToggleState = value.KeyboardToggleState;
            treatControlCAsInput = value.TreatControlCAsInput;
            encodingSet = value.EncodingSet;
        });
    }

    [[nodiscard]] StatusCode GetBackgroundColor(ColorCode& color) noexcept
    {
        return StoreTransformed(console::BackgroundColor(), [&](const console::ConsoleColor value)
        {
            color = static_cast<ColorCode>(value);
        });
    }

    [[nodiscard]] StatusCode SetBackgroundColor(const ColorCode color) noexcept
    {
        return StoreResult(console::BackgroundColor(static_cast<Color>(color)));
    }

    [[nodiscard]] StatusCode GetForegroundColor(ColorCode& color) noexcept
    {
        return StoreTransformed(console::ForegroundColor(), [&](const console::ConsoleColor value)
        {
            color = static_cast<ColorCode>(value);
        });
    }

    [[nodiscard]] StatusCode SetForegroundColor(const ColorCode color) noexcept
    {
        return StoreResult(console::ForegroundColor(static_cast<Color>(color)));
    }

    [[nodiscard]] StatusCode GetBufferHeight(int& value) noexcept { return StoreResult(console::BufferHeight(), &value); }
    [[nodiscard]] StatusCode SetBufferHeight(const int value) noexcept { return StoreResult(console::BufferHeight(value)); }
    [[nodiscard]] StatusCode GetBufferWidth(int& value) noexcept { return StoreResult(console::BufferWidth(), &value); }
    [[nodiscard]] StatusCode SetBufferWidth(const int value) noexcept { return StoreResult(console::BufferWidth(value)); }
    [[nodiscard]] StatusCode GetCapsLock(bool& value) noexcept { return StoreResult(console::CapsLock(), &value); }
    [[nodiscard]] StatusCode GetCursorLeft(int& value) noexcept { return StoreResult(console::CursorLeft(), &value); }
    [[nodiscard]] StatusCode SetCursorLeft(const int value) noexcept { return StoreResult(console::CursorLeft(value)); }
    [[nodiscard]] StatusCode GetCursorSize(int& value) noexcept { return StoreResult(console::CursorSize(), &value); }
    [[nodiscard]] StatusCode SetCursorSize(const int value) noexcept { return StoreResult(console::CursorSize(value)); }
    [[nodiscard]] StatusCode GetCursorTop(int& value) noexcept { return StoreResult(console::CursorTop(), &value); }
    [[nodiscard]] StatusCode SetCursorTop(const int value) noexcept { return StoreResult(console::CursorTop(value)); }
    [[nodiscard]] StatusCode GetCursorVisible(bool& value) noexcept { return StoreResult(console::CursorVisible(), &value); }
    [[nodiscard]] StatusCode SetCursorVisible(const bool value) noexcept { return StoreResult(console::CursorVisible(value)); }

    [[nodiscard]] StatusCode GetInputEncoding(EncodingCode& encoding) noexcept
    {
        return StoreTransformed(console::InputEncoding(), [&](const console::ConsoleEncoding value)
        {
            encoding = static_cast<EncodingCode>(value);
        });
    }

    [[nodiscard]] StatusCode SetInputEncoding(const EncodingCode encoding) noexcept
    {
        return StoreResult(console::InputEncoding(static_cast<Encoding>(encoding)));
    }

    [[nodiscard]] StatusCode GetOutputEncoding(EncodingCode& encoding) noexcept
    {
        return StoreTransformed(console::OutputEncoding(), [&](const console::ConsoleEncoding value)
        {
            encoding = static_cast<EncodingCode>(value);
        });
    }

    [[nodiscard]] StatusCode SetOutputEncoding(const EncodingCode encoding) noexcept
    {
        return StoreResult(console::OutputEncoding(static_cast<Encoding>(encoding)));
    }

    [[nodiscard]] StatusCode GetIsErrorRedirected(bool& value) noexcept { return StoreResult(console::IsErrorRedirected(), &value); }
    [[nodiscard]] StatusCode GetIsInputRedirected(bool& value) noexcept { return StoreResult(console::IsInputRedirected(), &value); }
    [[nodiscard]] StatusCode GetIsOutputRedirected(bool& value) noexcept { return StoreResult(console::IsOutputRedirected(), &value); }
    [[nodiscard]] StatusCode GetKeyAvailable(bool& value) noexcept { return StoreResult(console::KeyAvailable(), &value); }
    [[nodiscard]] StatusCode GetLargestWindowHeight(int& value) noexcept { return StoreResult(console::LargestWindowHeight(), &value); }
    [[nodiscard]] StatusCode GetLargestWindowWidth(int& value) noexcept { return StoreResult(console::LargestWindowWidth(), &value); }
    [[nodiscard]] StatusCode GetNumberLock(bool& value) noexcept { return StoreResult(console::NumberLock(), &value); }
    [[nodiscard]] StatusCode GetNewLine(std::string& value) { return StoreResult(console::NewLine(), &value); }
    [[nodiscard]] StatusCode SetNewLine(std::string_view value) { return StoreResult(console::NewLine(value)); }
    [[nodiscard]] StatusCode GetTitle(std::string& value) { return StoreResult(console::Title(), &value); }
    [[nodiscard]] StatusCode SetTitle(std::string_view value) { return StoreResult(console::Title(value)); }
    [[nodiscard]] StatusCode GetTreatControlCAsInput(bool& value) noexcept { return StoreResult(console::TreatControlCAsInput(), &value); }
    [[nodiscard]] StatusCode SetTreatControlCAsInput(const bool value) noexcept { return StoreResult(console::TreatControlCAsInput(value)); }
    [[nodiscard]] StatusCode GetWindowHeight(int& value) noexcept { return StoreResult(console::WindowHeight(), &value); }
    [[nodiscard]] StatusCode SetWindowHeight(const int value) noexcept { return StoreResult(console::WindowHeight(value)); }
    [[nodiscard]] StatusCode GetWindowLeft(int& value) noexcept { return StoreResult(console::WindowLeft(), &value); }
    [[nodiscard]] StatusCode SetWindowLeft(const int value) noexcept { return StoreResult(console::WindowLeft(value)); }
    [[nodiscard]] StatusCode GetWindowTop(int& value) noexcept { return StoreResult(console::WindowTop(), &value); }
    [[nodiscard]] StatusCode SetWindowTop(const int value) noexcept { return StoreResult(console::WindowTop(value)); }
    [[nodiscard]] StatusCode GetWindowWidth(int& value) noexcept { return StoreResult(console::WindowWidth(), &value); }
    [[nodiscard]] StatusCode SetWindowWidth(const int value) noexcept { return StoreResult(console::WindowWidth(value)); }
    [[nodiscard]] StatusCode Beep() noexcept { return StoreResult(console::Beep()); }
    [[nodiscard]] StatusCode Beep(const int frequency, const int durationMs) noexcept { return StoreResult(console::Beep(frequency, durationMs)); }
    [[nodiscard]] StatusCode Clear() noexcept { return StoreResult(console::Clear()); }

    [[nodiscard]] StatusCode GetCursorPosition(int& left, int& top) noexcept
    {
        return StoreTransformed(console::GetCursorPosition(), [&](const console::CursorPosition& value)
        {
            left = value.Left;
            top = value.Top;
        });
    }

    [[nodiscard]] StatusCode MoveBufferArea(
        const int sourceLeft,
        const int sourceTop,
        const int sourceWidth,
        const int sourceHeight,
        const int targetLeft,
        const int targetTop) noexcept
    {
        return StoreResult(console::MoveBufferArea(
            sourceLeft,
            sourceTop,
            sourceWidth,
            sourceHeight,
            targetLeft,
            targetTop));
    }

    [[nodiscard]] StatusCode MoveBufferArea(
        const int sourceLeft,
        const int sourceTop,
        const int sourceWidth,
        const int sourceHeight,
        const int targetLeft,
        const int targetTop,
        const std::int32_t sourceChar,
        const ColorCode sourceForeColor,
        const ColorCode sourceBackColor) noexcept
    {
        return StoreResult(console::MoveBufferArea(
            sourceLeft,
            sourceTop,
            sourceWidth,
            sourceHeight,
            targetLeft,
            targetTop,
            static_cast<char32_t>(sourceChar),
            static_cast<Color>(sourceForeColor),
            static_cast<Color>(sourceBackColor)));
    }

    [[nodiscard]] StatusCode OpenStandardError(
        const std::size_t bufferSize,
        bool& redirected,
        std::size_t& actualBufferSize,
        bool& hasReader,
        bool& hasWriter,
        bool& isOpen) noexcept
    {
        return StoreTransformed(console::OpenStandardError(bufferSize), [&](const console::StandardStream& value)
        {
            redirected = value.Redirected;
            actualBufferSize = value.BufferSize;
            hasReader = static_cast<bool>(value.Reader);
            hasWriter = static_cast<bool>(value.Writer);
            isOpen = value.File != nullptr;
        });
    }

    [[nodiscard]] StatusCode OpenStandardInput(
        const std::size_t bufferSize,
        bool& redirected,
        std::size_t& actualBufferSize,
        bool& hasReader,
        bool& hasWriter,
        bool& isOpen) noexcept
    {
        return StoreTransformed(console::OpenStandardInput(bufferSize), [&](const console::StandardStream& value)
        {
            redirected = value.Redirected;
            actualBufferSize = value.BufferSize;
            hasReader = static_cast<bool>(value.Reader);
            hasWriter = static_cast<bool>(value.Writer);
            isOpen = value.File != nullptr;
        });
    }

    [[nodiscard]] StatusCode OpenStandardOutput(
        const std::size_t bufferSize,
        bool& redirected,
        std::size_t& actualBufferSize,
        bool& hasReader,
        bool& hasWriter,
        bool& isOpen) noexcept
    {
        return StoreTransformed(console::OpenStandardOutput(bufferSize), [&](const console::StandardStream& value)
        {
            redirected = value.Redirected;
            actualBufferSize = value.BufferSize;
            hasReader = static_cast<bool>(value.Reader);
            hasWriter = static_cast<bool>(value.Writer);
            isOpen = value.File != nullptr;
        });
    }

    [[nodiscard]] StatusCode Read(int& value) noexcept { return StoreResult(console::Read(), &value); }
    [[nodiscard]] StatusCode ReadCount(const std::size_t count, std::string& value) { return StoreResult(console::ReadCount(count), &value); }
    [[nodiscard]] StatusCode ReadHidden(int& value) noexcept { return StoreResult(console::ReadHidden(), &value); }

    [[nodiscard]] StatusCode ReadKey(std::int32_t& keyChar, KeyCode& key, ModifierMask& modifiers)
    {
        return StoreTransformed(console::ReadKey(), [&](const console::ConsoleKeyInfo& value)
        {
            keyChar = static_cast<std::int32_t>(value.KeyChar);
            key = static_cast<KeyCode>(value.Key);
            modifiers = static_cast<ModifierMask>(value.Modifiers);
        });
    }

    [[nodiscard]] StatusCode ReadKey(const bool intercept, std::int32_t& keyChar, KeyCode& key, ModifierMask& modifiers)
    {
        return StoreTransformed(console::ReadKey(intercept), [&](const console::ConsoleKeyInfo& value)
        {
            keyChar = static_cast<std::int32_t>(value.KeyChar);
            key = static_cast<KeyCode>(value.Key);
            modifiers = static_cast<ModifierMask>(value.Modifiers);
        });
    }

    [[nodiscard]] StatusCode ReadLine(std::string& value) { return StoreResult(console::ReadLine(), &value); }
    [[nodiscard]] StatusCode ReadUntil(const char delimiter, const bool includeDelimiter, std::string& value) { return StoreResult(console::ReadUntil(delimiter, includeDelimiter), &value); }
    [[nodiscard]] StatusCode ReadWord(std::string& value) { return StoreResult(console::ReadWord(), &value); }
    [[nodiscard]] StatusCode ResetColor() noexcept { return StoreResult(console::ResetColor()); }
    [[nodiscard]] StatusCode SetBufferSize(const int width, const int height) noexcept { return StoreResult(console::SetBufferSize(width, height)); }
    [[nodiscard]] StatusCode SetCursorPosition(const int left, const int top) noexcept { return StoreResult(console::SetCursorPosition(left, top)); }
    [[nodiscard]] StatusCode SetWindowPosition(const int left, const int top) noexcept { return StoreResult(console::SetWindowPosition(left, top)); }
    [[nodiscard]] StatusCode SetWindowSize(const int width, const int height) noexcept { return StoreResult(console::SetWindowSize(width, height)); }
    [[nodiscard]] StatusCode FlushError() noexcept { return StoreResult(console::FlushError()); }
    [[nodiscard]] StatusCode FlushOut() noexcept { return StoreResult(console::FlushOut()); }
}

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
    #include <poll.h>
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#define WIO_CONSOLE_ERROR(StatusValue) \
    ::wio::runtime::console::MakeConsoleError( \
        (StatusValue), \
        ::wio::runtime::console::ConsoleErrorDomain::Generic, \
        0, \
        __FILE__, \
        __LINE__ \
    )

#define WIO_CONSOLE_ERROR_DOMAIN(StatusValue, DomainValue) \
    ::wio::runtime::console::MakeConsoleError( \
        (StatusValue), \
        (DomainValue), \
        0, \
        __FILE__, \
        __LINE__ \
    )

// ===== src/Console.cpp =====
namespace wio::runtime::console
{
    namespace
    {
        [[nodiscard]] Result<void> ValidateNonNegative(const int value) noexcept
        {
            if (value < 0)
                return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

            return {};
        }

        [[nodiscard]] Result<void> ValidatePositive(const int value) noexcept
        {
            if (value <= 0)
                return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

            return {};
        }

        [[nodiscard]] Result<std::string_view> CheckedSlice(
            const std::string_view value,
            const std::size_t index,
            const std::size_t count) noexcept
        {
            if (index > value.size() || count > value.size() - index)
                return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

            return value.substr(index, count);
        }

        [[nodiscard]] std::string_view SpanView(const std::span<const char> value) noexcept
        {
            if (value.empty())
                return {};

            return { value.data(), value.size() };
        }

        [[nodiscard]] Result<void> ApplyStandardInputBuffer(FILE* file, const std::size_t bufferSize) noexcept
        {
            if (file == nullptr)
                return WIO_CONSOLE_ERROR_DOMAIN(ConsoleStatus::NullArgument, ConsoleErrorDomain::Stdio);

            if (bufferSize == 0)
                return {};

            if (std::setvbuf(file, nullptr, _IOFBF, bufferSize) != 0)
                return WIO_CONSOLE_ERROR_DOMAIN(ConsoleStatus::IoError, ConsoleErrorDomain::Stdio);

            return {};
        }

        [[nodiscard]] Result<TextWriterPtr> MakeBufferedWriter(TextWriterPtr writer, const std::size_t bufferSize) noexcept
        {
            if (!writer)
                return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);

            if (bufferSize == 0)
                return writer;

            try
            {
                return TextWriterPtr(std::make_shared<detail::BufferedTextWriter>(std::move(writer), bufferSize));
            }
            catch (const std::bad_alloc&)
            {
                return WIO_CONSOLE_ERROR(ConsoleStatus::IoError);
            }
            catch (...)
            {
                return WIO_CONSOLE_ERROR(ConsoleStatus::UnknownError);
            }
        }

    }

    ScopedConsoleLock Lock()
    {
        return ScopedConsoleLock(
            std::unique_lock<std::recursive_mutex>(detail::State().Mutex)
        );
    }

    Result<ConsoleCapabilities> Capabilities() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->Capabilities();
    }

    Result<ConsoleColor> BackgroundColor() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->BackgroundColor();
    }

    Result<void> BackgroundColor(const ConsoleColor color) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->BackgroundColor(color);
    }

    Result<int> BufferHeight() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto size = detail::State().Backend->BufferSize();
        if (!size)
            return size.Error();
        return size.Value().Height;
    }

    Result<void> BufferHeight(const int value) noexcept
    {
        auto valid = ValidatePositive(value);
        if (!valid)
            return valid;

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto size = detail::State().Backend->BufferSize();
        if (!size)
            return size.Error();
        return detail::State().Backend->SetBufferSize(size.Value().Width, value);
    }

    Result<int> BufferWidth() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto size = detail::State().Backend->BufferSize();
        if (!size)
            return size.Error();
        return size.Value().Width;
    }

    Result<void> BufferWidth(const int value) noexcept
    {
        auto valid = ValidatePositive(value);
        if (!valid)
            return valid;

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto size = detail::State().Backend->BufferSize();
        if (!size)
            return size.Error();
        return detail::State().Backend->SetBufferSize(value, size.Value().Height);
    }

    Result<bool> CapsLock() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->CapsLock();
    }

    Result<int> CursorLeft() noexcept
    {
        auto pos = GetCursorPosition();
        if (!pos)
            return pos.Error();
        return pos.Value().Left;
    }

    Result<void> CursorLeft(const int value) noexcept
    {
        auto valid = ValidateNonNegative(value);
        if (!valid)
            return valid;

        auto pos = GetCursorPosition();
        if (!pos)
            return pos.Error();
        return SetCursorPosition(value, pos.Value().Top);
    }

    Result<int> CursorSize() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->CursorSize();
    }

    Result<void> CursorSize(const int value) noexcept
    {
        if (value < 1 || value > 100)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->CursorSize(value);
    }

    Result<int> CursorTop() noexcept
    {
        auto pos = GetCursorPosition();
        if (!pos)
            return pos.Error();
        return pos.Value().Top;
    }

    Result<void> CursorTop(const int value) noexcept
    {
        auto valid = ValidateNonNegative(value);
        if (!valid)
            return valid;

        auto pos = GetCursorPosition();
        if (!pos)
            return pos.Error();
        return SetCursorPosition(pos.Value().Left, value);
    }

    Result<bool> CursorVisible() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->CursorVisible();
    }

    Result<void> CursorVisible(const bool value) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->CursorVisible(value);
    }

    Result<TextWriterView> Error() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Error)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return TextWriterView(detail::State().Error, &detail::State().Mutex);
    }

    Result<LockedTextWriterView> LockedError()
    {
        std::unique_lock<std::recursive_mutex> lock(detail::State().Mutex);
        if (!detail::State().Error)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return LockedTextWriterView(detail::State().Error, std::move(lock));
    }

    Result<ConsoleColor> ForegroundColor() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->ForegroundColor();
    }

    Result<void> ForegroundColor(const ConsoleColor color) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->ForegroundColor(color);
    }

    Result<TextReaderView> In() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return TextReaderView(detail::State().In, &detail::State().Mutex);
    }

    Result<LockedTextReaderView> LockedIn()
    {
        std::unique_lock<std::recursive_mutex> lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return LockedTextReaderView(detail::State().In, std::move(lock));
    }

    Result<ConsoleEncoding> InputEncoding() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->InputEncoding();
    }

    Result<void> InputEncoding(const ConsoleEncoding encoding) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->InputEncoding(encoding);
    }

    Result<bool> IsErrorRedirected() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->IsErrorRedirected();
    }

    Result<bool> IsInputRedirected() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->IsInputRedirected();
    }

    Result<bool> IsOutputRedirected() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->IsOutputRedirected();
    }

    Result<bool> KeyAvailable() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->KeyAvailable();
    }

    Result<int> LargestWindowHeight() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto size = detail::State().Backend->LargestWindowSize();
        if (!size)
            return size.Error();
        return size.Value().Height;
    }

    Result<int> LargestWindowWidth() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto size = detail::State().Backend->LargestWindowSize();
        if (!size)
            return size.Error();
        return size.Value().Width;
    }

    Result<bool> NumberLock() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->NumberLock();
    }

    Result<std::string> NewLine()
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().NewLine;
    }

    Result<void> NewLine(const std::string_view value)
    {
        if (value.empty())
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidArgument);

        auto guard = std::scoped_lock(detail::State().Mutex);
        detail::State().NewLine.assign(value.data(), value.size());
        return {};
    }

    Result<TextWriterView> Out() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Out)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return TextWriterView(detail::State().Out, &detail::State().Mutex);
    }

    Result<LockedTextWriterView> LockedOut()
    {
        std::unique_lock<std::recursive_mutex> lock(detail::State().Mutex);
        if (!detail::State().Out)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return LockedTextWriterView(detail::State().Out, std::move(lock));
    }

    Result<ConsoleEncoding> OutputEncoding() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->OutputEncoding();
    }

    Result<void> OutputEncoding(const ConsoleEncoding encoding) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->OutputEncoding(encoding);
    }

    Result<std::string> Title()
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->Title();
    }

    Result<void> Title(const std::string_view value)
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->Title(value);
    }

    Result<bool> TreatControlCAsInput() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->TreatControlCAsInput();
    }

    Result<void> TreatControlCAsInput(const bool value) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->TreatControlCAsInput(value);
    }

    Result<int> WindowHeight() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return rect.Value().Height;
    }

    Result<void> WindowHeight(const int value) noexcept
    {
        auto valid = ValidatePositive(value);
        if (!valid)
            return valid;

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return detail::State().Backend->SetWindowSize(rect.Value().Width, value);
    }

    Result<int> WindowLeft() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return rect.Value().Left;
    }

    Result<void> WindowLeft(const int value) noexcept
    {
        auto valid = ValidateNonNegative(value);
        if (!valid)
            return valid;

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return detail::State().Backend->SetWindowPosition(value, rect.Value().Top);
    }

    Result<int> WindowTop() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return rect.Value().Top;
    }

    Result<void> WindowTop(const int value) noexcept
    {
        auto valid = ValidateNonNegative(value);
        if (!valid)
            return valid;

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return detail::State().Backend->SetWindowPosition(rect.Value().Left, value);
    }

    Result<int> WindowWidth() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return rect.Value().Width;
    }

    Result<void> WindowWidth(const int value) noexcept
    {
        auto valid = ValidatePositive(value);
        if (!valid)
            return valid;

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto rect = detail::State().Backend->WindowRect();
        if (!rect)
            return rect.Error();
        return detail::State().Backend->SetWindowSize(value, rect.Value().Height);
    }

    Result<void> Beep() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->Beep();
    }

    Result<void> Beep(const int frequency, const int durationMs) noexcept
    {
        if (frequency <= 0 || durationMs <= 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->Beep(frequency, durationMs);
    }

    Result<void> Clear() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->Clear();
    }

    Result<CursorPosition> GetCursorPosition() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->GetCursorPosition();
    }

    Result<void> MoveBufferArea(
        const int sourceLeft,
        const int sourceTop,
        const int sourceWidth,
        const int sourceHeight,
        const int targetLeft,
        const int targetTop) noexcept
    {
        return MoveBufferArea(MoveBufferAreaOptions {
            .SourceLeft = sourceLeft,
            .SourceTop = sourceTop,
            .SourceWidth = sourceWidth,
            .SourceHeight = sourceHeight,
            .TargetLeft = targetLeft,
            .TargetTop = targetTop,
            .Fill = ConsoleCell {}
        });
    }

    Result<void> MoveBufferArea(
        const int sourceLeft,
        const int sourceTop,
        const int sourceWidth,
        const int sourceHeight,
        const int targetLeft,
        const int targetTop,
        const char32_t sourceChar,
        const ConsoleColor sourceForeColor,
        const ConsoleColor sourceBackColor) noexcept
    {
        return MoveBufferArea(MoveBufferAreaOptions {
            .SourceLeft = sourceLeft,
            .SourceTop = sourceTop,
            .SourceWidth = sourceWidth,
            .SourceHeight = sourceHeight,
            .TargetLeft = targetLeft,
            .TargetTop = targetTop,
            .Fill = ConsoleCell {
                .Character = sourceChar,
                .Foreground = sourceForeColor,
                .Background = sourceBackColor
            }
        });
    }

    Result<void> MoveBufferArea(const MoveBufferAreaOptions& options) noexcept
    {
        if (options.SourceLeft < 0 || options.SourceTop < 0 ||
            options.SourceWidth <= 0 || options.SourceHeight <= 0 ||
            options.TargetLeft < 0 || options.TargetTop < 0)
        {
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);
        }

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->MoveBufferArea(options);
    }

    Result<StandardStream> OpenStandardError(const std::size_t bufferSize) noexcept
    {
        auto redirected = IsErrorRedirected();
        if (!redirected)
            return redirected.Error();

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto writer = MakeBufferedWriter(detail::State().Error, bufferSize);
        if (!writer)
            return writer.Error();

        return StandardStream {
            .File = stderr,
            .Redirected = redirected.Value(),
            .BufferSize = bufferSize,
            .Reader = nullptr,
            .Writer = writer.Value()
        };
    }

    Result<StandardStream> OpenStandardInput(const std::size_t bufferSize) noexcept
    {
        auto redirected = IsInputRedirected();
        if (!redirected)
            return redirected.Error();

        auto buffered = ApplyStandardInputBuffer(stdin, bufferSize);
        if (!buffered)
            return buffered.Error();

        auto guard = std::scoped_lock(detail::State().Mutex);
        return StandardStream {
            .File = stdin,
            .Redirected = redirected.Value(),
            .BufferSize = bufferSize,
            .Reader = detail::State().In,
            .Writer = nullptr
        };
    }

    Result<StandardStream> OpenStandardOutput(const std::size_t bufferSize) noexcept
    {
        auto redirected = IsOutputRedirected();
        if (!redirected)
            return redirected.Error();

        auto guard = std::scoped_lock(detail::State().Mutex);
        auto writer = MakeBufferedWriter(detail::State().Out, bufferSize);
        if (!writer)
            return writer.Error();

        return StandardStream {
            .File = stdout,
            .Redirected = redirected.Value(),
            .BufferSize = bufferSize,
            .Reader = nullptr,
            .Writer = writer.Value()
        };
    }

    Result<int> Read() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().In->Read();
    }

    Result<std::string> ReadCount(const std::size_t count)
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().In->ReadCount(count);
    }

    Result<int> ReadHidden() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);

        char ch = '\0';
        const auto result = ::wio::runtime::detail::io_helpers::InputCharHidden(ch);

        if (!result.Ok())
        {
            return MakeConsoleError(
                detail::MapReadError(result.error),
                ConsoleErrorDomain::Stdio
            );
        }

        return static_cast<int>(static_cast<unsigned char>(ch));
    }

    Result<ConsoleKeyInfo> ReadKey()
    {
        return ReadKey(false);
    }

    Result<ConsoleKeyInfo> ReadKey(const bool intercept)
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->ReadKey(intercept);
    }

    Result<std::string> ReadLine()
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().In->ReadLine();
    }

    Result<std::string> ReadUntil(const char delimiter, const bool includeDelimiter)
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().In->ReadUntil(delimiter, includeDelimiter);
    }

    Result<std::string> ReadWord()
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().In)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().In->ReadWord();
    }

    Result<void> ResetColor() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->ResetColor();
    }

    Result<void> SetBufferSize(const int width, const int height) noexcept
    {
        if (width <= 0 || height <= 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->SetBufferSize(width, height);
    }

    Result<void> SetCursorPosition(const int left, const int top) noexcept
    {
        if (left < 0 || top < 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->SetCursorPosition(left, top);
    }

    Result<void> SetError(TextWriterPtr writer) noexcept
    {
        if (!writer)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);

        auto guard = std::scoped_lock(detail::State().Mutex);
        detail::State().Error = std::move(writer);
        return {};
    }

    Result<void> SetIn(TextReaderPtr reader) noexcept
    {
        if (!reader)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);

        auto guard = std::scoped_lock(detail::State().Mutex);
        detail::State().In = std::move(reader);
        return {};
    }

    Result<void> SetOut(TextWriterPtr writer) noexcept
    {
        if (!writer)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);

        auto guard = std::scoped_lock(detail::State().Mutex);
        detail::State().Out = std::move(writer);
        return {};
    }

    Result<void> SetWindowPosition(const int left, const int top) noexcept
    {
        if (left < 0 || top < 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->SetWindowPosition(left, top);
    }

    Result<void> SetWindowSize(const int width, const int height) noexcept
    {
        if (width <= 0 || height <= 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        auto guard = std::scoped_lock(detail::State().Mutex);
        return detail::State().Backend->SetWindowSize(width, height);
    }

    Result<void> FlushError() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Error)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().Error->Flush();
    }

    Result<void> FlushOut() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Out)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().Out->Flush();
    }

    Result<IoCount> Write(const std::string_view value) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Out)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().Out->Write(value);
    }

    Result<IoCount> Write(const char* value) noexcept
    {
        if (value == nullptr)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return Write(std::string_view(value, std::strlen(value)));
    }

    Result<IoCount> Write(const char value) noexcept
    {
        return Write(std::string_view(&value, 1));
    }

    Result<IoCount> Write(const bool value) noexcept
    {
        return Write(value ? "True" : "False");
    }

    Result<IoCount> Write(const char* buffer, const int index, const int count) noexcept
    {
        if (buffer == nullptr)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        if (index < 0 || count < 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        const std::string_view text(buffer, std::strlen(buffer));
        return Write(text, static_cast<std::size_t>(index), static_cast<std::size_t>(count));
    }

    Result<IoCount> Write(
        const std::string_view value,
        const std::size_t index,
        const std::size_t count) noexcept
    {
        auto slice = CheckedSlice(value, index, count);
        if (!slice)
            return slice.Error();

        return Write(slice.Value());
    }

    Result<IoCount> Write(const std::span<const char> value) noexcept
    {
        return Write(SpanView(value));
    }

    Result<IoCount> Write(
        const std::span<const char> value,
        const std::size_t index,
        const std::size_t count) noexcept
    {
        return Write(SpanView(value), index, count);
    }

    Result<IoCount> Write(const TextDecimal& value) noexcept
    {
        return Write(value.Text);
    }

    Result<IoCount> Write(const ConsoleObject& value) noexcept
    {
        try
        {
            return Write(value.ToConsoleString());
        }
        catch (...)
        {
            return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Formatting);
        }
    }

    Result<IoCount> WriteLine() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Out)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().Out->Write(detail::State().NewLine);
    }

    Result<IoCount> WriteLine(const std::string_view value) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Out)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);

        auto first = detail::State().Out->Write(value);
        if (!first)
            return first;

        auto second = detail::State().Out->Write(detail::State().NewLine);
        if (!second)
            return second;

        return detail::SumIoCount(first.Value(), second.Value());
    }

    Result<IoCount> WriteLine(const char* value) noexcept
    {
        if (value == nullptr)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return WriteLine(std::string_view(value, std::strlen(value)));
    }

    Result<IoCount> WriteLine(const char value) noexcept
    {
        return WriteLine(std::string_view(&value, 1));
    }

    Result<IoCount> WriteLine(const bool value) noexcept
    {
        return WriteLine(value ? "True" : "False");
    }

    Result<IoCount> WriteLine(const char* buffer, const int index, const int count) noexcept
    {
        if (buffer == nullptr)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        if (index < 0 || count < 0)
            return WIO_CONSOLE_ERROR(ConsoleStatus::InvalidRange);

        const std::string_view text(buffer, std::strlen(buffer));
        return WriteLine(text, static_cast<std::size_t>(index), static_cast<std::size_t>(count));
    }

    Result<IoCount> WriteLine(
        const std::string_view value,
        const std::size_t index,
        const std::size_t count) noexcept
    {
        auto slice = CheckedSlice(value, index, count);
        if (!slice)
            return slice.Error();

        return WriteLine(slice.Value());
    }

    Result<IoCount> WriteLine(const std::span<const char> value) noexcept
    {
        return WriteLine(SpanView(value));
    }

    Result<IoCount> WriteLine(
        const std::span<const char> value,
        const std::size_t index,
        const std::size_t count) noexcept
    {
        return WriteLine(SpanView(value), index, count);
    }

    Result<IoCount> WriteLine(const TextDecimal& value) noexcept
    {
        return WriteLine(value.Text);
    }

    Result<IoCount> WriteLine(const ConsoleObject& value) noexcept
    {
        try
        {
            return WriteLine(value.ToConsoleString());
        }
        catch (...)
        {
            return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Formatting);
        }
    }

    Result<IoCount> WriteError(const std::string_view value) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Error)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().Error->Write(value);
    }

    Result<IoCount> WriteErrorLine() noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Error)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);
        return detail::State().Error->Write(detail::State().NewLine);
    }

    Result<IoCount> WriteErrorLine(const std::string_view value) noexcept
    {
        auto guard = std::scoped_lock(detail::State().Mutex);
        if (!detail::State().Error)
            return WIO_CONSOLE_ERROR(ConsoleStatus::NullArgument);

        auto first = detail::State().Error->Write(value);
        if (!first)
            return first;

        auto second = detail::State().Error->Write(detail::State().NewLine);
        if (!second)
            return second;

        return detail::SumIoCount(first.Value(), second.Value());
    }

}

// ===== src/ConsoleState.cpp =====
namespace wio::runtime::console::detail
{
    [[nodiscard]] static std::unique_ptr<ConsoleBackend> MakePlatformConsoleBackend(); // NOLINT(misc-use-anonymous-namespace)

    namespace
    {
        [[nodiscard]] TextReaderPtr MakeStdInReader()
        {
#if defined(_WIN32)
            return MakeWindowsConsoleReader(stdin, STD_INPUT_HANDLE);
#else
            return std::make_shared<FileTextReader>(stdin);
#endif
        }

        [[nodiscard]] TextWriterPtr MakeStdOutWriter()
        {
#if defined(_WIN32)
            return MakeWindowsConsoleWriter(stdout, STD_OUTPUT_HANDLE);
#else
            return std::make_shared<FileTextWriter>(stdout);
#endif
        }

        [[nodiscard]] TextWriterPtr MakeStdErrWriter()
        {
#if defined(_WIN32)
            return MakeWindowsConsoleWriter(stderr, STD_ERROR_HANDLE);
#else
            return std::make_shared<FileTextWriter>(stderr);
#endif
        }
    }

    ConsoleState::ConsoleState()
        : Backend(MakePlatformConsoleBackend())
        , In(MakeStdInReader())
        , Out(MakeStdOutWriter())
        , Error(MakeStdErrWriter())
#if defined(_WIN32)
        , NewLine("\r\n")
#else
        , NewLine("\n")
#endif
    {
    }

}

// ===== src/platform/AnsiConsoleBackend.cpp =====
#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))

namespace wio::runtime::console::detail
{
    namespace
    {
        [[nodiscard]] ConsoleError PosixError(const ConsoleStatus status) noexcept
        {
            return MakeConsoleError(status, ConsoleErrorDomain::Posix, errno);
        }

        [[nodiscard]] Result<void> Unsupported() noexcept
        {
            return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Terminal);
        }

        [[nodiscard]] Result<void> WriteAnsi(const std::string_view text) noexcept
        {
            const auto result = ::wio::runtime::detail::io_helpers::Write(stdout, text);

            if (!result.Ok())
            {
                return MakeConsoleError(
                    MapWriteError(result.error),
                    ConsoleErrorDomain::Stdio
                );
            }

            std::fflush(stdout);
            return {};
        }

        [[nodiscard]] int ToAnsiForeground(const ConsoleColor color) noexcept
        {
            switch (color)
            {
            case ConsoleColor::Black: return 30;
            case ConsoleColor::DarkRed: return 31;
            case ConsoleColor::DarkGreen: return 32;
            case ConsoleColor::DarkYellow: return 33;
            case ConsoleColor::DarkBlue: return 34;
            case ConsoleColor::DarkMagenta: return 35;
            case ConsoleColor::DarkCyan: return 36;
            case ConsoleColor::Gray: return 37;
            case ConsoleColor::DarkGray: return 90;
            case ConsoleColor::Red: return 91;
            case ConsoleColor::Green: return 92;
            case ConsoleColor::Yellow: return 93;
            case ConsoleColor::Blue: return 94;
            case ConsoleColor::Magenta: return 95;
            case ConsoleColor::Cyan: return 96;
            case ConsoleColor::White: return 97;
            default: return 37;
            }
        }

        [[nodiscard]] int ToAnsiBackground(const ConsoleColor color) noexcept
        {
            switch (color)
            {
            case ConsoleColor::Black: return 40;
            case ConsoleColor::DarkRed: return 41;
            case ConsoleColor::DarkGreen: return 42;
            case ConsoleColor::DarkYellow: return 43;
            case ConsoleColor::DarkBlue: return 44;
            case ConsoleColor::DarkMagenta: return 45;
            case ConsoleColor::DarkCyan: return 46;
            case ConsoleColor::Gray: return 47;
            case ConsoleColor::DarkGray: return 100;
            case ConsoleColor::Red: return 101;
            case ConsoleColor::Green: return 102;
            case ConsoleColor::Yellow: return 103;
            case ConsoleColor::Blue: return 104;
            case ConsoleColor::Magenta: return 105;
            case ConsoleColor::Cyan: return 106;
            case ConsoleColor::White: return 107;
            default: return 40;
            }
        }

        [[nodiscard]] ConsoleKey AsciiToConsoleKey(const unsigned char ch) noexcept
        {
            if (ch >= '0' && ch <= '9')
                return static_cast<ConsoleKey>(static_cast<int>(ConsoleKey::D0) + (ch - '0'));

            if (ch >= 'a' && ch <= 'z')
                return static_cast<ConsoleKey>(static_cast<int>(ConsoleKey::A) + (ch - 'a'));

            if (ch >= 'A' && ch <= 'Z')
                return static_cast<ConsoleKey>(static_cast<int>(ConsoleKey::A) + (ch - 'A'));

            switch (ch)
            {
            case 8:
            case 127:
                return ConsoleKey::Backspace;
            case '\t':
                return ConsoleKey::Tab;
            case '\n':
            case '\r':
                return ConsoleKey::Enter;
            case 27:
                return ConsoleKey::Escape;
            case ' ':
                return ConsoleKey::Spacebar;
            case '+':
                return ConsoleKey::OemPlus;
            case ',':
                return ConsoleKey::OemComma;
            case '-':
                return ConsoleKey::OemMinus;
            case '.':
                return ConsoleKey::OemPeriod;
            case '/':
                return ConsoleKey::Oem2;
            case '`':
                return ConsoleKey::Oem3;
            case '[':
                return ConsoleKey::Oem4;
            case '\\':
                return ConsoleKey::Oem5;
            case ']':
                return ConsoleKey::Oem6;
            case '\'':
                return ConsoleKey::Oem7;
            default:
                return ConsoleKey::None;
            }
        }

        [[nodiscard]] Result<unsigned char> ReadByte() noexcept
        {
            unsigned char ch = 0;
            const ssize_t n = ::read(STDIN_FILENO, &ch, 1);

            if (n < 0)
                return PosixError(ConsoleStatus::IoError);

            if (n == 0)
                return MakeConsoleError(ConsoleStatus::EndOfFile, ConsoleErrorDomain::Posix);

            return ch;
        }

        [[nodiscard]] int Utf8ContinuationCount(const unsigned char first) noexcept
        {
            if ((first & 0x80U) == 0U)
                return 0;
            if ((first & 0xE0U) == 0xC0U)
                return 1;
            if ((first & 0xF0U) == 0xE0U)
                return 2;
            if ((first & 0xF8U) == 0xF0U)
                return 3;
            return -1;
        }

        [[nodiscard]] Result<char32_t> DecodeUtf8CodePoint(
            const unsigned char first,
            std::string& consumedBytes) noexcept
        {
            consumedBytes.clear();
            consumedBytes.push_back(static_cast<char>(first));

            const int extra = Utf8ContinuationCount(first);
            if (extra < 0)
                return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Terminal);

            if (extra == 0)
                return static_cast<char32_t>(first);

            char32_t codePoint = static_cast<char32_t>(first & ((1U << (6 - extra)) - 1U));
            for (int i = 0; i < extra; ++i)
            {
                auto next = ReadByte();
                if (!next)
                    return next.Error();

                const unsigned char byte = next.Value();
                consumedBytes.push_back(static_cast<char>(byte));

                if ((byte & 0xC0U) != 0x80U)
                    return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Terminal);

                codePoint = static_cast<char32_t>((codePoint << 6U) | static_cast<char32_t>(byte & 0x3FU));
            }

            if ((extra == 1 && codePoint < 0x80U) ||
                (extra == 2 && codePoint < 0x800U) ||
                (extra == 3 && codePoint < 0x10000U) ||
                codePoint > 0x10FFFFU ||
                (codePoint >= 0xD800U && codePoint <= 0xDFFFU))
            {
                return MakeConsoleError(ConsoleStatus::EncodingError, ConsoleErrorDomain::Terminal);
            }

            return codePoint;
        }

        [[nodiscard]] ConsoleKey TildeSequenceToKey(const int value) noexcept
        {
            switch (value)
            {
            case 1:
            case 7:
                return ConsoleKey::Home;
            case 2:
                return ConsoleKey::Insert;
            case 3:
                return ConsoleKey::Delete;
            case 4:
            case 8:
                return ConsoleKey::End;
            case 5:
                return ConsoleKey::PageUp;
            case 6:
                return ConsoleKey::PageDown;
            case 11:
                return ConsoleKey::F1;
            case 12:
                return ConsoleKey::F2;
            case 13:
                return ConsoleKey::F3;
            case 14:
                return ConsoleKey::F4;
            case 15:
                return ConsoleKey::F5;
            case 17:
                return ConsoleKey::F6;
            case 18:
                return ConsoleKey::F7;
            case 19:
                return ConsoleKey::F8;
            case 20:
                return ConsoleKey::F9;
            case 21:
                return ConsoleKey::F10;
            case 23:
                return ConsoleKey::F11;
            case 24:
                return ConsoleKey::F12;
            case 25:
                return ConsoleKey::F13;
            case 26:
                return ConsoleKey::F14;
            case 28:
                return ConsoleKey::F15;
            case 29:
                return ConsoleKey::F16;
            case 31:
                return ConsoleKey::F17;
            case 32:
                return ConsoleKey::F18;
            case 33:
                return ConsoleKey::F19;
            case 34:
                return ConsoleKey::F20;
            default:
                return ConsoleKey::Escape;
            }
        }

        [[nodiscard]] ConsoleKey Ss3SequenceToKey(const unsigned char value) noexcept
        {
            switch (value)
            {
            case 'P': return ConsoleKey::F1;
            case 'Q': return ConsoleKey::F2;
            case 'R': return ConsoleKey::F3;
            case 'S': return ConsoleKey::F4;
            case 'A': return ConsoleKey::UpArrow;
            case 'B': return ConsoleKey::DownArrow;
            case 'C': return ConsoleKey::RightArrow;
            case 'D': return ConsoleKey::LeftArrow;
            case 'H': return ConsoleKey::Home;
            case 'F': return ConsoleKey::End;
            default: return ConsoleKey::Escape;
            }
        }

        [[nodiscard]] ConsoleModifiers XtermModifierToConsoleModifiers(const int value) noexcept
        {
            ConsoleModifiers modifiers = ConsoleModifiers::None;
            const int flags = value - 1;

            if ((flags & 1) != 0)
                modifiers |= ConsoleModifiers::Shift;
            if ((flags & 2) != 0)
                modifiers |= ConsoleModifiers::Alt;
            if ((flags & 4) != 0)
                modifiers |= ConsoleModifiers::Control;

            return modifiers;
        }

        [[nodiscard]] ConsoleKey CsiFinalToKey(const unsigned char final) noexcept
        {
            switch (final)
            {
            case 'A': return ConsoleKey::UpArrow;
            case 'B': return ConsoleKey::DownArrow;
            case 'C': return ConsoleKey::RightArrow;
            case 'D': return ConsoleKey::LeftArrow;
            case 'H': return ConsoleKey::Home;
            case 'F': return ConsoleKey::End;
            case 'Z': return ConsoleKey::Tab;
            default: return ConsoleKey::Escape;
            }
        }

        struct CsiSequence final
        {
            int Params[8] {};
            std::size_t Count = 0;
            unsigned char Final = 0;
        };

        [[nodiscard]] Result<CsiSequence> ReadCsiSequence() noexcept
        {
            CsiSequence sequence {};
            int current = 0;
            bool hasCurrent = false;

            while (true)
            {
                auto byte = ReadByte();
                if (!byte)
                    return byte.Error();

                const unsigned char ch = byte.Value();
                if (ch >= '0' && ch <= '9')
                {
                    hasCurrent = true;
                    current = current * 10 + static_cast<int>(ch - '0');
                    continue;
                }

                if (ch == ';')
                {
                    if (sequence.Count < 8)
                        sequence.Params[sequence.Count++] = hasCurrent ? current : 0;
                    current = 0;
                    hasCurrent = false;
                    continue;
                }

                if (sequence.Count < 8)
                    sequence.Params[sequence.Count++] = hasCurrent ? current : 0;

                sequence.Final = ch;
                return sequence;
            }
        }

        [[nodiscard]] Result<ConsoleKeyInfo> DecodeAltModifiedKey(const unsigned char first) noexcept
        {
            ConsoleKeyInfo info {};
            std::string consumedBytes;
            auto codePoint = DecodeUtf8CodePoint(first, consumedBytes);
            if (!codePoint)
                return codePoint.Error();

            info.KeyChar = codePoint.Value();
            info.Key = AsciiToConsoleKey(first);
            info.Modifiers = ConsoleModifiers::Alt;
            return info;
        }

        class ScopedRawTerminal final
        {
        public:
            ScopedRawTerminal() noexcept
            {
                if (!::isatty(STDIN_FILENO))
                    return;

                if (::tcgetattr(STDIN_FILENO, &m_Old) != 0)
                    return;

                termios current = m_Old;
                current.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
                current.c_cc[VMIN] = 1;
                current.c_cc[VTIME] = 0;

                if (::tcsetattr(STDIN_FILENO, TCSANOW, &current) == 0)
                    m_Active = true;
            }

            ~ScopedRawTerminal()
            {
                if (m_Active)
                    (void)::tcsetattr(STDIN_FILENO, TCSANOW, &m_Old);
            }

            [[nodiscard]] bool Active() const noexcept
            {
                return m_Active;
            }

        private:
            termios m_Old {};
            bool m_Active = false;
        };
    }

    class AnsiConsoleBackend final : public ConsoleBackend
    {
    public:
        [[nodiscard]] Result<ConsoleCapabilities> Capabilities() noexcept override
        {
            const bool isOutTty = ::isatty(STDOUT_FILENO) != 0;
            const bool isInTty = ::isatty(STDIN_FILENO) != 0;

            return ConsoleCapabilities {
                .Colors = isOutTty,
                .CursorPosition = isOutTty,
                .CursorVisibility = isOutTty,
                .CursorSize = false,
                .BufferSize = false,
                .WindowPosition = false,
                .WindowSizeGet = isOutTty,
                .WindowSizeSet = false,
                .LargestWindowSize = false,
                .MoveBufferArea = false,
                .KeyAvailable = isInTty,
                .ReadKey = isInTty,
                .Title = isOutTty,
                .Beep = true,
                .BeepFrequency = false,
                .KeyboardToggleState = false,
                .TreatControlCAsInput = isInTty,
                .EncodingSet = false
            };
        }

        [[nodiscard]] Result<ConsoleColor> ForegroundColor() noexcept override
        {
            return m_Foreground;
        }

        [[nodiscard]] Result<void> ForegroundColor(const ConsoleColor color) noexcept override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            char buffer[32] {};
            std::snprintf(buffer, sizeof(buffer), "\x1B[%dm", ToAnsiForeground(color));
            auto result = WriteAnsi(buffer);
            if (!result)
                return result;

            m_Foreground = color;
            return {};
        }

        [[nodiscard]] Result<ConsoleColor> BackgroundColor() noexcept override
        {
            return m_Background;
        }

        [[nodiscard]] Result<void> BackgroundColor(const ConsoleColor color) noexcept override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            char buffer[32] {};
            std::snprintf(buffer, sizeof(buffer), "\x1B[%dm", ToAnsiBackground(color));
            auto result = WriteAnsi(buffer);
            if (!result)
                return result;

            m_Background = color;
            return {};
        }

        [[nodiscard]] Result<void> ResetColor() noexcept override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            auto result = WriteAnsi("\x1B[0m");
            if (!result)
                return result;

            m_Foreground = ConsoleColor::Gray;
            m_Background = ConsoleColor::Black;
            return {};
        }

        [[nodiscard]] Result<CursorPosition> GetCursorPosition() noexcept override
        {
            if (!::isatty(STDIN_FILENO) || !::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::Unavailable, ConsoleErrorDomain::Terminal);

            ScopedRawTerminal raw;
            if (!raw.Active())
                return PosixError(ConsoleStatus::PlatformError);

            auto writeResult = WriteAnsi("\x1B[6n");
            if (!writeResult)
                return writeResult.Error();

            char response[64] {};
            std::size_t index = 0;

            while (index + 1 < sizeof(response))
            {
                unsigned char ch = 0;
                const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
                if (n <= 0)
                    return PosixError(ConsoleStatus::IoError);

                response[index++] = static_cast<char>(ch);
                if (ch == 'R')
                    break;
            }

            int row = 0;
            int col = 0;
            if (std::sscanf(response, "\x1B[%d;%dR", &row, &col) != 2)
                return MakeConsoleError(ConsoleStatus::PlatformError, ConsoleErrorDomain::Terminal);

            return CursorPosition {
                .Left = col - 1,
                .Top = row - 1
            };
        }

        [[nodiscard]] Result<void> SetCursorPosition(const int left, const int top) noexcept override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            char buffer[64] {};
            std::snprintf(buffer, sizeof(buffer), "\x1B[%d;%dH", top + 1, left + 1);
            return WriteAnsi(buffer);
        }

        [[nodiscard]] Result<bool> CursorVisible() noexcept override
        {
            return m_CursorVisible;
        }

        [[nodiscard]] Result<void> CursorVisible(const bool visible) noexcept override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            auto result = WriteAnsi(visible ? "\x1B[?25h" : "\x1B[?25l");
            if (!result)
                return result;

            m_CursorVisible = visible;
            return {};
        }

        [[nodiscard]] Result<int> CursorSize() noexcept override
        {
            return Unsupported().Error();
        }

        [[nodiscard]] Result<void> CursorSize(int) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<ConsoleSize> BufferSize() noexcept override
        {
            return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Terminal);
        }

        [[nodiscard]] Result<void> SetBufferSize(int, int) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<ConsoleRect> WindowRect() noexcept override
        {
            winsize size {};
            if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0)
                return PosixError(ConsoleStatus::PlatformError);

            return ConsoleRect {
                .Left = 0,
                .Top = 0,
                .Width = static_cast<int>(size.ws_col),
                .Height = static_cast<int>(size.ws_row)
            };
        }

        [[nodiscard]] Result<void> SetWindowPosition(int, int) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<void> SetWindowSize(int, int) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<ConsoleSize> LargestWindowSize() noexcept override
        {
            return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Terminal);
        }

        [[nodiscard]] Result<void> Clear() noexcept override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            return WriteAnsi("\x1B[2J\x1B[H");
        }

        [[nodiscard]] Result<void> MoveBufferArea(const MoveBufferAreaOptions&) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<ConsoleKeyInfo> ReadKey(const bool intercept) override
        {
            if (!::isatty(STDIN_FILENO))
                return MakeConsoleError(ConsoleStatus::InputRedirected, ConsoleErrorDomain::Terminal);

            ScopedRawTerminal raw;
            if (!raw.Active())
                return PosixError(ConsoleStatus::PlatformError);

            auto firstResult = ReadByte();
            if (!firstResult)
                return firstResult.Error();

            const unsigned char first = firstResult.Value();
            ConsoleKeyInfo info {};
            std::string consumedBytes;

            if (first == 27)
            {
                info.Key = ConsoleKey::Escape;
                info.KeyChar = U'\x1B';

                pollfd fd { STDIN_FILENO, POLLIN, 0 };
                if (::poll(&fd, 1, 15) > 0)
                {
                    auto secondResult = ReadByte();
                    if (!secondResult)
                        return secondResult.Error();

                    const unsigned char second = secondResult.Value();
                    if (second == '[')
                    {
                        auto sequence = ReadCsiSequence();
                        if (!sequence)
                            return sequence.Error();

                        const CsiSequence& csi = sequence.Value();
                        const int firstParam = csi.Count > 0 && csi.Params[0] != 0 ? csi.Params[0] : 1;
                        const int modifierParam = csi.Count > 1 ? csi.Params[1] : 1;

                        if (csi.Final == '~')
                            info.Key = TildeSequenceToKey(firstParam);
                        else
                            info.Key = CsiFinalToKey(csi.Final);

                        info.KeyChar = U'\0';
                        info.Modifiers = XtermModifierToConsoleModifiers(modifierParam);

                        if (csi.Final == 'Z')
                            info.Modifiers |= ConsoleModifiers::Shift;
                    }
                    else if (second == 'O')
                    {
                        auto thirdResult = ReadByte();
                        if (!thirdResult)
                            return thirdResult.Error();

                        info.Key = Ss3SequenceToKey(thirdResult.Value());
                        info.KeyChar = U'\0';
                    }
                    else
                    {
                        auto alt = DecodeAltModifiedKey(second);
                        if (!alt)
                            return alt.Error();

                        info = alt.Value();
                        consumedBytes.push_back(static_cast<char>(second));
                    }
                }
            }
            else if (first >= 1 && first <= 26)
            {
                info.Key = static_cast<ConsoleKey>(static_cast<int>(ConsoleKey::A) + first - 1);
                info.KeyChar = U'\0';
                info.Modifiers = ConsoleModifiers::Control;
            }
            else if (first == 0x7FU)
            {
                info.Key = ConsoleKey::Backspace;
                info.KeyChar = U'\b';
            }
            else
            {
                auto codePoint = DecodeUtf8CodePoint(first, consumedBytes);
                if (!codePoint)
                    return codePoint.Error();

                info.KeyChar = codePoint.Value();
                info.Key = AsciiToConsoleKey(first);
            }

            if (!intercept && info.KeyChar != U'\0')
            {
                if (consumedBytes.empty())
                    consumedBytes.push_back(static_cast<char>(first));

                (void)::wio::runtime::detail::io_helpers::Write(stdout, consumedBytes);
                std::fflush(stdout);
            }

            return info;
        }

        [[nodiscard]] Result<bool> KeyAvailable() noexcept override
        {
            pollfd fd { STDIN_FILENO, POLLIN, 0 };
            const int result = ::poll(&fd, 1, 0);

            if (result < 0)
                return PosixError(ConsoleStatus::IoError);

            return result > 0 && (fd.revents & POLLIN) != 0;
        }

        [[nodiscard]] Result<void> Beep() noexcept override
        {
            return WriteAnsi("\a");
        }

        [[nodiscard]] Result<void> Beep(int, int) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<std::string> Title() override
        {
            return m_Title;
        }

        [[nodiscard]] Result<void> Title(const std::string_view value) override
        {
            if (!::isatty(STDOUT_FILENO))
                return MakeConsoleError(ConsoleStatus::OutputRedirected, ConsoleErrorDomain::Terminal);

            std::string sequence;
            sequence.reserve(value.size() + 8);
            sequence += "\x1B]0;";
            sequence += value;
            sequence += "\x07";

            auto result = WriteAnsi(sequence);
            if (!result)
                return result;

            m_Title.assign(value.begin(), value.end());
            return {};
        }

        [[nodiscard]] Result<bool> CapsLock() noexcept override
        {
            return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Terminal);
        }

        [[nodiscard]] Result<bool> NumberLock() noexcept override
        {
            return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Terminal);
        }

        [[nodiscard]] Result<bool> TreatControlCAsInput() noexcept override
        {
            return m_TreatControlCAsInput;
        }

        [[nodiscard]] Result<void> TreatControlCAsInput(const bool value) noexcept override
        {
            if (!::isatty(STDIN_FILENO))
                return MakeConsoleError(ConsoleStatus::InputRedirected, ConsoleErrorDomain::Terminal);

            termios current {};
            if (::tcgetattr(STDIN_FILENO, &current) != 0)
                return PosixError(ConsoleStatus::PlatformError);

            if (value)
                current.c_lflag &= static_cast<unsigned>(~ISIG);
            else
                current.c_lflag |= ISIG;

            if (::tcsetattr(STDIN_FILENO, TCSANOW, &current) != 0)
                return PosixError(ConsoleStatus::PlatformError);

            m_TreatControlCAsInput = value;
            return {};
        }

        [[nodiscard]] Result<bool> IsInputRedirected() noexcept override
        {
            return ::isatty(STDIN_FILENO) == 0;
        }

        [[nodiscard]] Result<bool> IsOutputRedirected() noexcept override
        {
            return ::isatty(STDOUT_FILENO) == 0;
        }

        [[nodiscard]] Result<bool> IsErrorRedirected() noexcept override
        {
            return ::isatty(STDERR_FILENO) == 0;
        }

        [[nodiscard]] Result<ConsoleEncoding> InputEncoding() noexcept override
        {
            return ConsoleEncoding::Utf8;
        }

        [[nodiscard]] Result<void> InputEncoding(ConsoleEncoding) noexcept override
        {
            return Unsupported();
        }

        [[nodiscard]] Result<ConsoleEncoding> OutputEncoding() noexcept override
        {
            return ConsoleEncoding::Utf8;
        }

        [[nodiscard]] Result<void> OutputEncoding(ConsoleEncoding) noexcept override
        {
            return Unsupported();
        }

    private:
        ConsoleColor m_Foreground = ConsoleColor::Gray;
        ConsoleColor m_Background = ConsoleColor::Black;
        bool m_CursorVisible = true;
        bool m_TreatControlCAsInput = false;
        std::string m_Title;
    };

    std::unique_ptr<ConsoleBackend> MakePlatformConsoleBackend()
    {
        return std::make_unique<AnsiConsoleBackend>();
    }
}

#endif

// ===== src/platform/NullConsoleBackend.cpp =====
#if !defined(_WIN32) && !(defined(__unix__) || defined(__APPLE__))

namespace wio::runtime::console::detail
{
    namespace
    {
        [[nodiscard]] ConsoleError UnsupportedError() noexcept
        {
            return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Terminal);
        }
    }

    class NullConsoleBackend final : public ConsoleBackend
    {
    public:
        [[nodiscard]] Result<ConsoleCapabilities> Capabilities() noexcept override
        {
            return ConsoleCapabilities {};
        }

        [[nodiscard]] Result<ConsoleColor> ForegroundColor() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> ForegroundColor(ConsoleColor) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<ConsoleColor> BackgroundColor() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> BackgroundColor(ConsoleColor) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> ResetColor() noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<CursorPosition> GetCursorPosition() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> SetCursorPosition(int, int) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<bool> CursorVisible() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> CursorVisible(bool) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<int> CursorSize() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> CursorSize(int) noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<ConsoleSize> BufferSize() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> SetBufferSize(int, int) noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<ConsoleRect> WindowRect() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> SetWindowPosition(int, int) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> SetWindowSize(int, int) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<ConsoleSize> LargestWindowSize() noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<void> Clear() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> MoveBufferArea(const MoveBufferAreaOptions&) noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<ConsoleKeyInfo> ReadKey(bool) override { return UnsupportedError(); }
        [[nodiscard]] Result<bool> KeyAvailable() noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<void> Beep() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> Beep(int, int) noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<std::string> Title() override { return UnsupportedError(); }
        [[nodiscard]] Result<void> Title(std::string_view) override { return UnsupportedError(); }

        [[nodiscard]] Result<bool> CapsLock() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<bool> NumberLock() noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<bool> TreatControlCAsInput() noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<void> TreatControlCAsInput(bool) noexcept override { return UnsupportedError(); }

        [[nodiscard]] Result<bool> IsInputRedirected() noexcept override { return true; }
        [[nodiscard]] Result<bool> IsOutputRedirected() noexcept override { return true; }
        [[nodiscard]] Result<bool> IsErrorRedirected() noexcept override { return true; }

        [[nodiscard]] Result<ConsoleEncoding> InputEncoding() noexcept override { return ConsoleEncoding::Unknown; }
        [[nodiscard]] Result<void> InputEncoding(ConsoleEncoding) noexcept override { return UnsupportedError(); }
        [[nodiscard]] Result<ConsoleEncoding> OutputEncoding() noexcept override { return ConsoleEncoding::Unknown; }
        [[nodiscard]] Result<void> OutputEncoding(ConsoleEncoding) noexcept override { return UnsupportedError(); }
    };

    std::unique_ptr<ConsoleBackend> MakePlatformConsoleBackend()
    {
        return std::make_unique<NullConsoleBackend>();
    }
}

#endif

// ===== src/platform/WindowsConsoleBackend.cpp =====
#if defined(_WIN32)



namespace wio::runtime::console::detail
{
    namespace
    {
        [[nodiscard]] ConsoleError Win32Error(const ConsoleStatus status) noexcept
        {
            return MakeConsoleError(
                status,
                ConsoleErrorDomain::Win32,
                static_cast<int>(::GetLastError())
            );
        }

        [[nodiscard]] WORD ForegroundMask() noexcept
        {
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        }

        [[nodiscard]] WORD BackgroundMask() noexcept
        {
            return BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
        }

        [[nodiscard]] WORD ToWin32Foreground(const ConsoleColor color) noexcept
        {
            switch (color)
            {
            case ConsoleColor::Black: return 0;
            case ConsoleColor::DarkBlue: return FOREGROUND_BLUE;
            case ConsoleColor::DarkGreen: return FOREGROUND_GREEN;
            case ConsoleColor::DarkCyan: return FOREGROUND_GREEN | FOREGROUND_BLUE;
            case ConsoleColor::DarkRed: return FOREGROUND_RED;
            case ConsoleColor::DarkMagenta: return FOREGROUND_RED | FOREGROUND_BLUE;
            case ConsoleColor::DarkYellow: return FOREGROUND_RED | FOREGROUND_GREEN;
            case ConsoleColor::Gray: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            case ConsoleColor::DarkGray: return FOREGROUND_INTENSITY;
            case ConsoleColor::Blue: return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case ConsoleColor::Green: return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case ConsoleColor::Cyan: return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case ConsoleColor::Red: return FOREGROUND_RED | FOREGROUND_INTENSITY;
            case ConsoleColor::Magenta: return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case ConsoleColor::Yellow: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case ConsoleColor::White: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            default: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // NOLINT(clang-diagnostic-covered-switch-default)
            }
        }

        [[nodiscard]] WORD ToWin32Background(const ConsoleColor color) noexcept
        {
            return static_cast<WORD>(ToWin32Foreground(color) << 4);
        }

        [[nodiscard]] ConsoleColor FromWin32Foreground(const WORD attr) noexcept
        {
            switch (attr & ForegroundMask())
            {
            case 0: return ConsoleColor::Black;
            case FOREGROUND_BLUE: return ConsoleColor::DarkBlue;
            case FOREGROUND_GREEN: return ConsoleColor::DarkGreen;
            case FOREGROUND_GREEN | FOREGROUND_BLUE: return ConsoleColor::DarkCyan;
            case FOREGROUND_RED: return ConsoleColor::DarkRed;
            case FOREGROUND_RED | FOREGROUND_BLUE: return ConsoleColor::DarkMagenta;
            case FOREGROUND_RED | FOREGROUND_GREEN: return ConsoleColor::DarkYellow;
            case FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE: return ConsoleColor::Gray;
            case FOREGROUND_INTENSITY: return ConsoleColor::DarkGray;
            case FOREGROUND_BLUE | FOREGROUND_INTENSITY: return ConsoleColor::Blue;
            case FOREGROUND_GREEN | FOREGROUND_INTENSITY: return ConsoleColor::Green;
            case FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY: return ConsoleColor::Cyan;
            case FOREGROUND_RED | FOREGROUND_INTENSITY: return ConsoleColor::Red;
            case FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY: return ConsoleColor::Magenta;
            case FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY: return ConsoleColor::Yellow;
            case FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY: return ConsoleColor::White;
            default: return ConsoleColor::Gray;
            }
        }

        [[nodiscard]] ConsoleColor FromWin32Background(const WORD attr) noexcept
        {
            return FromWin32Foreground(static_cast<WORD>((attr & BackgroundMask()) >> 4));
        }

        [[nodiscard]] Result<std::wstring> Utf8ToWide(const std::string_view value)
        {
            if (value.empty())
                return std::wstring {};

            const int required = ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0
            );

            if (required <= 0)
                return Win32Error(ConsoleStatus::EncodingError);

            std::wstring output(static_cast<std::size_t>(required), L'\0');
            const int converted = ::MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                output.data(),
                required
            );

            if (converted != required)
                return Win32Error(ConsoleStatus::EncodingError);

            return output;
        }

        [[nodiscard]] Result<std::string> WideToUtf8(const std::wstring_view value)
        {
            if (value.empty())
                return std::string {};

            const int required = ::WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );

            if (required <= 0)
                return Win32Error(ConsoleStatus::EncodingError);

            std::string output(static_cast<std::size_t>(required), '\0');
            const int converted = ::WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                value.data(),
                static_cast<int>(value.size()),
                output.data(),
                required,
                nullptr,
                nullptr
            );

            if (converted != required)
                return Win32Error(ConsoleStatus::EncodingError);

            return output;
        }

        [[nodiscard]] ConsoleModifiers FromControlKeyState(const DWORD state) noexcept
        {
            ConsoleModifiers modifiers = ConsoleModifiers::None;

            if ((state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0)
                modifiers |= ConsoleModifiers::Alt;

            if ((state & SHIFT_PRESSED) != 0)
                modifiers |= ConsoleModifiers::Shift;

            if ((state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0)
                modifiers |= ConsoleModifiers::Control;

            return modifiers;
        }
    }

    class WindowsConsoleBackend final : public ConsoleBackend
    {
    public:
        WindowsConsoleBackend() noexcept
            : m_Input(::GetStdHandle(STD_INPUT_HANDLE))
            , m_Output(::GetStdHandle(STD_OUTPUT_HANDLE))
            , m_Error(::GetStdHandle(STD_ERROR_HANDLE))
        {
            CONSOLE_SCREEN_BUFFER_INFO info {};
            if (::GetConsoleScreenBufferInfo(m_Output, &info) != FALSE)
                m_DefaultAttributes = info.wAttributes;
        }

        [[nodiscard]] Result<ConsoleCapabilities> Capabilities() noexcept override
        {
            const bool outConsole = IsConsole(m_Output);
            const bool inConsole = IsConsole(m_Input);

            return ConsoleCapabilities {
                .Colors = outConsole,
                .CursorPosition = outConsole,
                .CursorVisibility = outConsole,
                .CursorSize = outConsole,
                .BufferSize = outConsole,
                .WindowPosition = outConsole,
                .WindowSizeGet = outConsole,
                .WindowSizeSet = outConsole,
                .LargestWindowSize = outConsole,
                .MoveBufferArea = outConsole,
                .KeyAvailable = inConsole,
                .ReadKey = inConsole,
                .Title = true,
                .Beep = true,
                .BeepFrequency = true,
                .KeyboardToggleState = true,
                .TreatControlCAsInput = inConsole,
                .EncodingSet = true
            };
        }

        [[nodiscard]] Result<ConsoleColor> ForegroundColor() noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();
            return FromWin32Foreground(info.Value().wAttributes);
        }

        [[nodiscard]] Result<void> ForegroundColor(const ConsoleColor color) noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();

            const WORD attr = static_cast<WORD>(
                (info.Value().wAttributes & ~ForegroundMask()) |
                ToWin32Foreground(color)
            );

            if (::SetConsoleTextAttribute(m_Output, attr) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<ConsoleColor> BackgroundColor() noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();
            return FromWin32Background(info.Value().wAttributes);
        }

        [[nodiscard]] Result<void> BackgroundColor(const ConsoleColor color) noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();

            const WORD attr = static_cast<WORD>(
                (info.Value().wAttributes & ~BackgroundMask()) |
                ToWin32Background(color)
            );

            if (::SetConsoleTextAttribute(m_Output, attr) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<void> ResetColor() noexcept override
        {
            if (::SetConsoleTextAttribute(m_Output, m_DefaultAttributes) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<CursorPosition> GetCursorPosition() noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();

            return CursorPosition {
                .Left = info.Value().dwCursorPosition.X,
                .Top = info.Value().dwCursorPosition.Y
            };
        }

        [[nodiscard]] Result<void> SetCursorPosition(const int left, const int top) noexcept override
        {
            COORD coord {
                .X = static_cast<SHORT>(left),
                .Y = static_cast<SHORT>(top)
            };

            if (::SetConsoleCursorPosition(m_Output, coord) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<bool> CursorVisible() noexcept override
        {
            CONSOLE_CURSOR_INFO info {};
            if (::GetConsoleCursorInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return info.bVisible != FALSE;
        }

        [[nodiscard]] Result<void> CursorVisible(const bool visible) noexcept override
        {
            CONSOLE_CURSOR_INFO info {};
            if (::GetConsoleCursorInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            info.bVisible = visible ? TRUE : FALSE;

            if (::SetConsoleCursorInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<int> CursorSize() noexcept override
        {
            CONSOLE_CURSOR_INFO info {};
            if (::GetConsoleCursorInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return static_cast<int>(info.dwSize);
        }

        [[nodiscard]] Result<void> CursorSize(const int size) noexcept override
        {
            CONSOLE_CURSOR_INFO info {};
            if (::GetConsoleCursorInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            info.dwSize = static_cast<DWORD>(size);

            if (::SetConsoleCursorInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<ConsoleSize> BufferSize() noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();

            return ConsoleSize {
                .Width = info.Value().dwSize.X,
                .Height = info.Value().dwSize.Y
            };
        }

        [[nodiscard]] Result<void> SetBufferSize(const int width, const int height) noexcept override
        {
            COORD size {
                .X = static_cast<SHORT>(width),
                .Y = static_cast<SHORT>(height)
            };

            if (::SetConsoleScreenBufferSize(m_Output, size) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<ConsoleRect> WindowRect() noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();

            const auto& window = info.Value().srWindow;
            return ConsoleRect {
                .Left = window.Left,
                .Top = window.Top,
                .Width = window.Right - window.Left + 1,
                .Height = window.Bottom - window.Top + 1
            };
        }

        [[nodiscard]] Result<void> SetWindowPosition(const int left, const int top) noexcept override
        {
            auto rect = WindowRect();
            if (!rect)
                return rect.Error();

            SMALL_RECT window {
                .Left = static_cast<SHORT>(left),
                .Top = static_cast<SHORT>(top),
                .Right = static_cast<SHORT>(left + rect.Value().Width - 1),
                .Bottom = static_cast<SHORT>(top + rect.Value().Height - 1)
            };

            if (::SetConsoleWindowInfo(m_Output, TRUE, &window) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<void> SetWindowSize(const int width, const int height) noexcept override
        {
            auto rect = WindowRect();
            if (!rect)
                return rect.Error();

            SMALL_RECT window {
                .Left = static_cast<SHORT>(rect.Value().Left),
                .Top = static_cast<SHORT>(rect.Value().Top),
                .Right = static_cast<SHORT>(rect.Value().Left + width - 1),
                .Bottom = static_cast<SHORT>(rect.Value().Top + height - 1)
            };

            if (::SetConsoleWindowInfo(m_Output, TRUE, &window) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<ConsoleSize> LargestWindowSize() noexcept override
        {
            const COORD size = ::GetLargestConsoleWindowSize(m_Output);
            if (size.X == 0 && size.Y == 0)
                return Win32Error(ConsoleStatus::PlatformError);

            return ConsoleSize {
                .Width = size.X,
                .Height = size.Y
            };
        }

        [[nodiscard]] Result<void> Clear() noexcept override
        {
            auto info = Info();
            if (!info)
                return info.Error();

            const DWORD cellCount = static_cast<DWORD>(info.Value().dwSize.X) * static_cast<DWORD>(info.Value().dwSize.Y);
            DWORD written = 0;
            COORD home { .X = 0, .Y = 0 };

            if (::FillConsoleOutputCharacterW(m_Output, L' ', cellCount, home, &written) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            if (::FillConsoleOutputAttribute(m_Output, info.Value().wAttributes, cellCount, home, &written) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            if (::SetConsoleCursorPosition(m_Output, home) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<void> MoveBufferArea(const MoveBufferAreaOptions& options) noexcept override
        {
            COORD bufferSize {
                .X = static_cast<SHORT>(options.SourceWidth),
                .Y = static_cast<SHORT>(options.SourceHeight)
            };
            COORD bufferCoord { .X = 0, .Y = 0 };
            SMALL_RECT readRegion {
                .Left = static_cast<SHORT>(options.SourceLeft),
                .Top = static_cast<SHORT>(options.SourceTop),
                .Right = static_cast<SHORT>(options.SourceLeft + options.SourceWidth - 1),
                .Bottom = static_cast<SHORT>(options.SourceTop + options.SourceHeight - 1)
            };

            std::vector<CHAR_INFO> data;
            try
            {
                data.resize((static_cast<std::size_t>(options.SourceWidth) * options.SourceHeight));
            }
            catch (const std::bad_alloc&)
            {
                return MakeConsoleError(ConsoleStatus::IoError, ConsoleErrorDomain::Win32);
            }
            catch (...)
            {
                return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Win32);
            }

            if (::ReadConsoleOutputW(m_Output, data.data(), bufferSize, bufferCoord, &readRegion) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            SMALL_RECT writeRegion {
                .Left = static_cast<SHORT>(options.TargetLeft),
                .Top = static_cast<SHORT>(options.TargetTop),
                .Right = static_cast<SHORT>(options.TargetLeft + options.SourceWidth - 1),
                .Bottom = static_cast<SHORT>(options.TargetTop + options.SourceHeight - 1)
            };

            if (::WriteConsoleOutputW(m_Output, data.data(), bufferSize, bufferCoord, &writeRegion) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            CHAR_INFO fill;
            fill.Char.UnicodeChar = static_cast<WCHAR>(options.Fill.Character);
            fill.Attributes = static_cast<WORD>(ToWin32Foreground(options.Fill.Foreground) | ToWin32Background(options.Fill.Background));

            std::vector<CHAR_INFO> fillData;
            try
            {
                fillData.assign((static_cast<size_t>(options.SourceWidth) * options.SourceHeight), fill);
            }
            catch (const std::bad_alloc&)
            {
                return MakeConsoleError(ConsoleStatus::IoError, ConsoleErrorDomain::Win32);
            }
            catch (...)
            {
                return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Win32);
            }

            if (::WriteConsoleOutputW(m_Output, fillData.data(), bufferSize, bufferCoord, &readRegion) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<ConsoleKeyInfo> ReadKey(const bool intercept) override
        {
            while (true)
            {
                INPUT_RECORD record {};
                DWORD read = 0;

                if (::ReadConsoleInputW(m_Input, &record, 1, &read) == FALSE)
                    return Win32Error(ConsoleStatus::PlatformError);

                if (read == 0 || record.EventType != KEY_EVENT)
                    continue;

                const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
                if (key.bKeyDown == FALSE)
                    continue;

                WCHAR chars[2] {};
                std::size_t charCount = 0;
                char32_t keyChar = U'\0';

                const WCHAR first = key.uChar.UnicodeChar;
                if (first != L'\0')
                {
                    chars[charCount++] = first;

                    if (first >= 0xD800 && first <= 0xDBFF)
                    {
                        while (true)
                        {
                            INPUT_RECORD nextRecord {};
                            DWORD nextRead = 0;
                            if (::ReadConsoleInputW(m_Input, &nextRecord, 1, &nextRead) == FALSE)
                                return Win32Error(ConsoleStatus::PlatformError);

                            if (nextRead == 0 || nextRecord.EventType != KEY_EVENT)
                                continue;

                            const KEY_EVENT_RECORD& nextKey = nextRecord.Event.KeyEvent;
                            if (nextKey.bKeyDown == FALSE)
                                continue;

                            const WCHAR low = nextKey.uChar.UnicodeChar;
                            if (low < 0xDC00 || low > 0xDFFF)
                                return Win32Error(ConsoleStatus::EncodingError);

                            chars[charCount++] = low;
                            keyChar = static_cast<char32_t>(
                                0x10000U +
                                ((static_cast<char32_t>(first) - 0xD800U) << 10U) +
                                (static_cast<char32_t>(low) - 0xDC00U)
                            );
                            break;
                        }
                    }
                    else if (first >= 0xDC00 && first <= 0xDFFF)
                    {
                        return Win32Error(ConsoleStatus::EncodingError);
                    }
                    else
                    {
                        keyChar = static_cast<char32_t>(first);
                    }
                }

                if (!intercept && charCount > 0)
                {
                    DWORD written = 0;
                    (void)::WriteConsoleW(
                        m_Output,
                        chars,
                        static_cast<DWORD>(charCount),
                        &written,
                        nullptr
                    );
                }

                return ConsoleKeyInfo {
                    .KeyChar = keyChar,
                    .Key = static_cast<ConsoleKey>(key.wVirtualKeyCode),
                    .Modifiers = FromControlKeyState(key.dwControlKeyState)
                };
            }
        }

        [[nodiscard]] Result<bool> KeyAvailable() noexcept override
        {
            DWORD count = 0;
            if (::GetNumberOfConsoleInputEvents(m_Input, &count) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            if (count == 0)
                return false;

            std::vector<INPUT_RECORD> records;
            try
            {
                records.resize(count);
            }
            catch (const std::bad_alloc&)
            {
                return MakeConsoleError(ConsoleStatus::IoError, ConsoleErrorDomain::Win32);
            }
            catch (...)
            {
                return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Win32);
            }

            DWORD read = 0;
            if (::PeekConsoleInputW(m_Input, records.data(), count, &read) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            for (DWORD i = 0; i < read; ++i)
            {
                if (records[i].EventType == KEY_EVENT && records[i].Event.KeyEvent.bKeyDown != FALSE)
                    return true;
            }

            return false;
        }

        [[nodiscard]] Result<void> Beep() noexcept override
        {
            if (::MessageBeep(MB_OK) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);
            return {};
        }

        [[nodiscard]] Result<void> Beep(const int frequency, const int durationMs) noexcept override
        {
            if (::Beep(static_cast<DWORD>(frequency), static_cast<DWORD>(durationMs)) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);
            return {};
        }

        [[nodiscard]] Result<std::string> Title() override
        {
            std::wstring buffer(32768, L'\0');
            const DWORD size = ::GetConsoleTitleW(buffer.data(), static_cast<DWORD>(buffer.size()));
            if (size == 0 && ::GetLastError() != ERROR_SUCCESS)
                return Win32Error(ConsoleStatus::PlatformError);

            buffer.resize(size);
            return WideToUtf8(buffer);
        }

        [[nodiscard]] Result<void> Title(const std::string_view value) override
        {
            auto wide = Utf8ToWide(value);
            if (!wide)
                return wide.Error();

            if (::SetConsoleTitleW(wide.Value().c_str()) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);
            return {};
        }

        [[nodiscard]] Result<bool> CapsLock() noexcept override
        {
            return (::GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        }

        [[nodiscard]] Result<bool> NumberLock() noexcept override
        {
            return (::GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
        }

        [[nodiscard]] Result<bool> TreatControlCAsInput() noexcept override
        {
            DWORD mode = 0;
            if (::GetConsoleMode(m_Input, &mode) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);
            return (mode & ENABLE_PROCESSED_INPUT) == 0;
        }

        [[nodiscard]] Result<void> TreatControlCAsInput(const bool value) noexcept override
        {
            DWORD mode = 0;
            if (::GetConsoleMode(m_Input, &mode) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            if (value)
                mode &= ~ENABLE_PROCESSED_INPUT;
            else
                mode |= ENABLE_PROCESSED_INPUT;

            if (::SetConsoleMode(m_Input, mode) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return {};
        }

        [[nodiscard]] Result<bool> IsInputRedirected() noexcept override
        {
            return !IsConsole(m_Input);
        }

        [[nodiscard]] Result<bool> IsOutputRedirected() noexcept override
        {
            return !IsConsole(m_Output);
        }

        [[nodiscard]] Result<bool> IsErrorRedirected() noexcept override
        {
            return !IsConsole(m_Error);
        }

        [[nodiscard]] Result<ConsoleEncoding> InputEncoding() noexcept override
        {
            const UINT cp = ::GetConsoleCP();
            return cp == CP_UTF8 ? ConsoleEncoding::Utf8 : ConsoleEncoding::SystemDefault;
        }

        [[nodiscard]] Result<void> InputEncoding(const ConsoleEncoding encoding) noexcept override
        {
            UINT cp;
            if (encoding == ConsoleEncoding::Utf8)
                cp = CP_UTF8;
            else if (encoding == ConsoleEncoding::SystemDefault)
                cp = CP_ACP;
            else
                return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Win32);

            if (::SetConsoleCP(cp) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);
            return {};
        }

        [[nodiscard]] Result<ConsoleEncoding> OutputEncoding() noexcept override
        {
            const UINT cp = ::GetConsoleOutputCP();
            return cp == CP_UTF8 ? ConsoleEncoding::Utf8 : ConsoleEncoding::SystemDefault;
        }

        [[nodiscard]] Result<void> OutputEncoding(const ConsoleEncoding encoding) noexcept override
        {
            UINT cp;
            if (encoding == ConsoleEncoding::Utf8)
                cp = CP_UTF8;
            else if (encoding == ConsoleEncoding::SystemDefault)
                cp = CP_ACP;
            else
                return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Win32);

            if (::SetConsoleOutputCP(cp) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);
            return {};
        }

    private:
        [[nodiscard]] static bool IsConsole(HANDLE handle) noexcept
        {
            DWORD mode = 0;
            return handle != nullptr && handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode) != FALSE;
        }

        [[nodiscard]] Result<CONSOLE_SCREEN_BUFFER_INFO> Info() const noexcept
        {
            CONSOLE_SCREEN_BUFFER_INFO info {};
            if (::GetConsoleScreenBufferInfo(m_Output, &info) == FALSE)
                return Win32Error(ConsoleStatus::PlatformError);

            return info;
        }

    private:
        HANDLE m_Input = nullptr;
        HANDLE m_Output = nullptr;
        HANDLE m_Error = nullptr;
        WORD m_DefaultAttributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    };

    std::unique_ptr<ConsoleBackend> MakePlatformConsoleBackend()
    {
        return std::make_unique<WindowsConsoleBackend>();
    }
}

#endif
