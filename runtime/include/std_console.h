#pragma once

#include "text.h"

#include <cstdio>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace wio::runtime::console
{
        enum class ConsoleStatus : std::uint8_t
        {
            Ok = 0,
            Unsupported,
            Unavailable,
            InvalidArgument,
            InvalidRange,
            NullArgument,
            InputRedirected,
            OutputRedirected,
            ErrorRedirected,
            IoError,
            PartialIo,
            EndOfFile,
            EncodingError,
            FormatError,
            PermissionDenied,
            PlatformError,
            UnknownError
        };

        enum class ConsoleErrorDomain : std::uint8_t
        {
            None = 0,
            Generic,
            Stdio,
            Win32,
            Posix,
            Terminal,
            Formatting
        };

        struct ConsoleError
        {
            ConsoleStatus Status = ConsoleStatus::Ok;
            ConsoleErrorDomain Domain = ConsoleErrorDomain::None;
            int NativeCode = 0;
            const char* File = nullptr;
            int Line = 0;

            [[nodiscard]] constexpr bool Ok() const noexcept
            {
                return Status == ConsoleStatus::Ok;
            }

            [[nodiscard]] constexpr explicit operator bool() const noexcept
            {
                return Ok();
            }
        };

        [[nodiscard]] constexpr ConsoleError MakeConsoleError(
            const ConsoleStatus status,
            const ConsoleErrorDomain domain = ConsoleErrorDomain::Generic,
            const int nativeCode = 0,
            const char* file = nullptr,
            const int line = 0) noexcept
        {
            return ConsoleError {
                .Status = status,
                .Domain = domain,
                .NativeCode = nativeCode,
                .File = file,
                .Line = line
            };
        }

        [[nodiscard]] constexpr std::string_view ToString(const ConsoleStatus status) noexcept
        {
            switch (status)
            {
            case ConsoleStatus::Ok: return "Ok";
            case ConsoleStatus::Unsupported: return "Unsupported";
            case ConsoleStatus::Unavailable: return "Unavailable";
            case ConsoleStatus::InvalidArgument: return "InvalidArgument";
            case ConsoleStatus::InvalidRange: return "InvalidRange";
            case ConsoleStatus::NullArgument: return "NullArgument";
            case ConsoleStatus::InputRedirected: return "InputRedirected";
            case ConsoleStatus::OutputRedirected: return "OutputRedirected";
            case ConsoleStatus::ErrorRedirected: return "ErrorRedirected";
            case ConsoleStatus::IoError: return "IoError";
            case ConsoleStatus::PartialIo: return "PartialIo";
            case ConsoleStatus::EndOfFile: return "EndOfFile";
            case ConsoleStatus::EncodingError: return "EncodingError";
            case ConsoleStatus::FormatError: return "FormatError";
            case ConsoleStatus::PermissionDenied: return "PermissionDenied";
            case ConsoleStatus::PlatformError: return "PlatformError";
            case ConsoleStatus::UnknownError: return "UnknownError";
            }
            return "UnknownError";
        }

        [[nodiscard]] constexpr std::string_view ToString(const ConsoleErrorDomain domain) noexcept
        {
            switch (domain)
            {
            case ConsoleErrorDomain::None: return "None";
            case ConsoleErrorDomain::Generic: return "Generic";
            case ConsoleErrorDomain::Stdio: return "Stdio";
            case ConsoleErrorDomain::Win32: return "Win32";
            case ConsoleErrorDomain::Posix: return "Posix";
            case ConsoleErrorDomain::Terminal: return "Terminal";
            case ConsoleErrorDomain::Formatting: return "Formatting";
            }
            return "Generic";
        }

        namespace detail_result
        {
            template <typename T>
            union ResultStorage // NOLINT(cppcoreguidelines-special-member-functions)
            {
                constexpr ResultStorage() noexcept {}
                ~ResultStorage() {}

                T Value;
            };
        }

        template <typename T>
        class Result
        {
        public:
            static_assert(!std::is_same_v<T, void>, "Use Result<void> specialization.");

            Result(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
            {
                Construct(value);
            }

            Result(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
            {
                Construct(std::move(value));
            }

            Result(const ConsoleError& error) noexcept
                : m_Error(error.Ok() ? MakeConsoleError(ConsoleStatus::UnknownError) : error)
                , m_HasValue(false)
            {
            }

            Result(const Result& other) noexcept(std::is_nothrow_copy_constructible_v<T>)
                : m_Error(other.m_Error)
            {
                if (other.m_HasValue)
                    Construct(other.Value());
            }

            Result(Result&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
                : m_Error(other.m_Error)
            {
                if (other.m_HasValue)
                    Construct(std::move(other).Value());
            }

            ~Result()
            {
                Destroy();
            }

            Result& operator=(const Result& other) noexcept(
                std::is_nothrow_copy_constructible_v<T> &&
                std::is_nothrow_copy_assignable_v<T>)
            {
                if (this == &other)
                    return *this;

                if (m_HasValue && other.m_HasValue)
                {
                    Value() = other.Value();
                }
                else
                {
                    Destroy();
                    if (other.m_HasValue)
                        Construct(other.Value());
                }

                m_Error = other.m_Error;
                return *this;
            }

            Result& operator=(Result&& other) noexcept(
                std::is_nothrow_move_constructible_v<T> &&
                std::is_nothrow_move_assignable_v<T>)
            {
                if (this == &other)
                    return *this;

                if (m_HasValue && other.m_HasValue)
                {
                    Value() = std::move(other).Value();
                }
                else
                {
                    Destroy();
                    if (other.m_HasValue)
                        Construct(std::move(other).Value());
                }

                m_Error = other.m_Error;
                return *this;
            }

            [[nodiscard]] bool Ok() const noexcept
            {
                return m_HasValue && m_Error.Ok();
            }

            [[nodiscard]] bool HasValue() const noexcept
            {
                return m_HasValue;
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return Ok();
            }

            [[nodiscard]] const T& Value() const& noexcept
            {
                return *std::launder(std::addressof(m_Storage.Value));
            }

            [[nodiscard]] T& Value() & noexcept
            {
                return *std::launder(std::addressof(m_Storage.Value));
            }

            [[nodiscard]] T&& Value() && noexcept
            {
                return std::move(*std::launder(std::addressof(m_Storage.Value)));
            }

            [[nodiscard]] const T* operator->() const noexcept
            {
                return std::addressof(Value());
            }

            [[nodiscard]] T* operator->() noexcept
            {
                return std::addressof(Value());
            }

            [[nodiscard]] ConsoleError Error() const noexcept
            {
                return m_Error;
            }

            [[nodiscard]] T ValueOr(T fallback) const&
            {
                return m_HasValue ? Value() : std::move(fallback);
            }

            [[nodiscard]] T ValueOr(T fallback) &&
            {
                return m_HasValue ? std::move(*this).Value() : std::move(fallback);
            }

        private:
            template <typename... Args>
            void Construct(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            {
                ::new (static_cast<void*>(std::addressof(m_Storage.Value))) T(std::forward<Args>(args)...);
                m_HasValue = true;
                m_Error = {};
            }

            void Destroy() noexcept
            {
                if (m_HasValue)
                {
                    std::launder(std::addressof(m_Storage.Value))->~T();
                    m_HasValue = false;
                }
            }

        private:
            detail_result::ResultStorage<T> m_Storage;
            ConsoleError m_Error {};
            bool m_HasValue = false;
        };

        template <>
        class Result<void>
        {
        public:
            constexpr Result() noexcept = default;

            constexpr Result(const ConsoleError& error) noexcept
                : m_Error(error.Ok() ? MakeConsoleError(ConsoleStatus::UnknownError) : error)
            {
            }

            [[nodiscard]] constexpr bool Ok() const noexcept
            {
                return m_Error.Ok();
            }

            [[nodiscard]] constexpr bool HasValue() const noexcept
            {
                return Ok();
            }

            [[nodiscard]] constexpr explicit operator bool() const noexcept
            {
                return Ok();
            }

            [[nodiscard]] constexpr ConsoleError Error() const noexcept
            {
                return m_Error;
            }

        private:
            ConsoleError m_Error {};
        };

        [[nodiscard]] constexpr Result<void> Ok() noexcept
        {
            return {};
        }

        template <typename T>
        [[nodiscard]] Result<T> Ok(T value) noexcept(noexcept(Result<T>(std::move(value))))
        {
            return Result<T>(std::move(value));
        }

        enum class ConsoleColor : std::uint8_t
        {
            Black = 0,
            DarkBlue = 1,
            DarkGreen = 2,
            DarkCyan = 3,
            DarkRed = 4,
            DarkMagenta = 5,
            DarkYellow = 6,
            Gray = 7,
            DarkGray = 8,
            Blue = 9,
            Green = 10,
            Cyan = 11,
            Red = 12,
            Magenta = 13,
            Yellow = 14,
            White = 15
        };

        enum class ConsoleEncoding : std::uint8_t
        {
            Unknown = 0,
            Utf8,
            Utf16,
            SystemDefault
        };

        struct CursorPosition
        {
            int Left = 0;
            int Top = 0;
        };

        struct ConsoleSize
        {
            int Width = 0;
            int Height = 0;
        };

        struct ConsoleRect
        {
            int Left = 0;
            int Top = 0;
            int Width = 0;
            int Height = 0;
        };

        struct ConsoleCell
        {
            char32_t Character = U' ';
            ConsoleColor Foreground = ConsoleColor::Gray;
            ConsoleColor Background = ConsoleColor::Black;
        };

        struct MoveBufferAreaOptions
        {
            int SourceLeft = 0;
            int SourceTop = 0;
            int SourceWidth = 0;
            int SourceHeight = 0;
            int TargetLeft = 0;
            int TargetTop = 0;
            ConsoleCell Fill {};
        };

        struct IoCount
        {
            std::size_t Requested = 0;
            std::size_t Processed = 0;
        };

        class TextReader;
        class TextWriter;

        struct StandardStream
        {
            FILE* File = nullptr;
            bool Redirected = false;
            std::size_t BufferSize = 0;
            std::shared_ptr<TextReader> Reader;
            std::shared_ptr<TextWriter> Writer;
        };

        // Text-backed decimal value for formatting/display.
        // This is intentionally not named Decimal because it is not an arbitrary-precision
        // decimal arithmetic type; formatting parses Text when a numeric format is requested.
        struct TextDecimal
        {
            std::string Text;

            TextDecimal() = default;

            explicit TextDecimal(std::string text)
                : Text(std::move(text))
            {
            }
        };

        using Decimal [[deprecated("Use TextDecimal. Decimal is a text-backed compatibility alias, not an arithmetic decimal type.")]] = TextDecimal;

        class ConsoleObject // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
            virtual ~ConsoleObject() = default;
            [[nodiscard]] virtual std::string ToConsoleString() const = 0;
        };

        struct ConsoleCapabilities
        {
            bool Colors = false;
            bool CursorPosition = false;
            bool CursorVisibility = false;
            bool CursorSize = false;
            bool BufferSize = false;
            bool WindowPosition = false;
            bool WindowSizeGet = false;
            bool WindowSizeSet = false;
            bool LargestWindowSize = false;
            bool MoveBufferArea = false;
            bool KeyAvailable = false;
            bool ReadKey = false;
            bool Title = false;
            bool Beep = false;
            bool BeepFrequency = false;
            bool KeyboardToggleState = false;
            bool TreatControlCAsInput = false;
            bool EncodingSet = false;
        };

        enum class ConsoleKey : std::int32_t // NOLINT(performance-enum-size)
        {
            None = 0,
            Backspace = 8,
            Tab = 9,
            Clear = 12,
            Enter = 13,
            Pause = 19,
            Escape = 27,
            Spacebar = 32,
            PageUp = 33,
            PageDown = 34,
            End = 35,
            Home = 36,
            LeftArrow = 37,
            UpArrow = 38,
            RightArrow = 39,
            DownArrow = 40,
            Select = 41,
            Print = 42,
            Execute = 43,
            PrintScreen = 44,
            Insert = 45,
            Delete = 46,
            Help = 47,
            D0 = 48,
            D1 = 49,
            D2 = 50,
            D3 = 51,
            D4 = 52,
            D5 = 53,
            D6 = 54,
            D7 = 55,
            D8 = 56,
            D9 = 57,
            A = 65,
            B = 66,
            C = 67,
            D = 68,
            E = 69,
            F = 70,
            G = 71,
            H = 72,
            I = 73,
            J = 74,
            K = 75,
            L = 76,
            M = 77,
            N = 78,
            O = 79,
            P = 80,
            Q = 81,
            R = 82,
            S = 83,
            T = 84,
            U = 85,
            V = 86,
            W = 87,
            X = 88,
            Y = 89,
            Z = 90,
            LeftWindows = 91,
            RightWindows = 92,
            Applications = 93,
            Sleep = 95,
            NumPad0 = 96,
            NumPad1 = 97,
            NumPad2 = 98,
            NumPad3 = 99,
            NumPad4 = 100,
            NumPad5 = 101,
            NumPad6 = 102,
            NumPad7 = 103,
            NumPad8 = 104,
            NumPad9 = 105,
            Multiply = 106,
            Add = 107,
            Separator = 108,
            Subtract = 109,
            Decimal = 110,
            Divide = 111,
            F1 = 112,
            F2 = 113,
            F3 = 114,
            F4 = 115,
            F5 = 116,
            F6 = 117,
            F7 = 118,
            F8 = 119,
            F9 = 120,
            F10 = 121,
            F11 = 122,
            F12 = 123,
            F13 = 124,
            F14 = 125,
            F15 = 126,
            F16 = 127,
            F17 = 128,
            F18 = 129,
            F19 = 130,
            F20 = 131,
            F21 = 132,
            F22 = 133,
            F23 = 134,
            F24 = 135,
            BrowserBack = 166,
            BrowserForward = 167,
            BrowserRefresh = 168,
            BrowserStop = 169,
            BrowserSearch = 170,
            BrowserFavorites = 171,
            BrowserHome = 172,
            VolumeMute = 173,
            VolumeDown = 174,
            VolumeUp = 175,
            MediaNext = 176,
            MediaPrevious = 177,
            MediaStop = 178,
            MediaPlay = 179,
            LaunchMail = 180,
            LaunchMediaSelect = 181,
            LaunchApp1 = 182,
            LaunchApp2 = 183,
            Oem1 = 186,
            OemPlus = 187,
            OemComma = 188,
            OemMinus = 189,
            OemPeriod = 190,
            Oem2 = 191,
            Oem3 = 192,
            Oem4 = 219,
            Oem5 = 220,
            Oem6 = 221,
            Oem7 = 222,
            Oem8 = 223,
            Oem102 = 226,
            Process = 229,
            Packet = 231,
            Attention = 246,
            CrSel = 247,
            ExSel = 248,
            EraseEndOfFile = 249,
            Play = 250,
            Zoom = 251,
            NoName = 252,
            Pa1 = 253,
            OemClear = 254
        };

        enum class ConsoleModifiers : std::uint8_t
        {
            None = 0,
            Alt = 1,
            Shift = 2,
            Control = 4
        };

        [[nodiscard]] constexpr ConsoleModifiers operator|(
            const ConsoleModifiers lhs,
            const ConsoleModifiers rhs) noexcept
        {
            return static_cast<ConsoleModifiers>(
                static_cast<std::uint8_t>(lhs) |
                static_cast<std::uint8_t>(rhs)
            );
        }

        [[nodiscard]] constexpr ConsoleModifiers operator&(
            const ConsoleModifiers lhs,
            const ConsoleModifiers rhs) noexcept
        {
            return static_cast<ConsoleModifiers>(
                static_cast<std::uint8_t>(lhs) &
                static_cast<std::uint8_t>(rhs)
            );
        }

        constexpr ConsoleModifiers& operator|=(
            ConsoleModifiers& lhs,
            const ConsoleModifiers rhs) noexcept
        {
            lhs = lhs | rhs;
            return lhs;
        }

        [[nodiscard]] constexpr bool HasModifier(
            const ConsoleModifiers value,
            const ConsoleModifiers flag) noexcept
        {
            return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
        }

        struct ConsoleKeyInfo
        {
            char32_t KeyChar = U'\0';
            ConsoleKey Key = ConsoleKey::None;
            ConsoleModifiers Modifiers = ConsoleModifiers::None;
        };

        enum class ConsoleSpecialKey : std::int32_t // NOLINT(performance-enum-size)
        {
            ControlC = 0,
            ControlBreak = 1
        };

        class TextWriter // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
            virtual ~TextWriter() = default;

            [[nodiscard]] virtual Result<IoCount> Write(std::string_view text) noexcept = 0;
            [[nodiscard]] virtual Result<void> Flush() noexcept = 0;
        };

        using TextWriterPtr = std::shared_ptr<TextWriter>;

        class TextWriterView final
        {
        public:
            TextWriterView() = default;

            TextWriterView(TextWriterPtr writer, std::recursive_mutex* mutex) noexcept
                : m_Writer(std::move(writer))
                , m_Mutex(mutex)
            {
            }

            [[nodiscard]] Result<IoCount> Write(const std::string_view text) const noexcept
            {
                if (!m_Writer || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Writer->Write(text);
            }

            [[nodiscard]] Result<void> Flush() const noexcept
            {
                if (!m_Writer || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Writer->Flush();
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return m_Writer != nullptr && m_Mutex != nullptr;
            }

        private:
            TextWriterPtr m_Writer;
            std::recursive_mutex* m_Mutex = nullptr;
        };

        class LockedTextWriterView final // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
            LockedTextWriterView() = default;

            LockedTextWriterView(TextWriterPtr writer, std::unique_lock<std::recursive_mutex>&& lock) noexcept
                : m_Writer(std::move(writer))
                , m_Lock(std::move(lock))
            {
            }

            LockedTextWriterView(const LockedTextWriterView&) = delete;
            LockedTextWriterView& operator=(const LockedTextWriterView&) = delete;
            LockedTextWriterView(LockedTextWriterView&&) noexcept = default;
            LockedTextWriterView& operator=(LockedTextWriterView&&) noexcept = default;

            [[nodiscard]] Result<IoCount> Write(const std::string_view text) const noexcept
            {
                if (!m_Writer || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Writer->Write(text);
            }

            [[nodiscard]] Result<void> Flush() const noexcept
            {
                if (!m_Writer || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Writer->Flush();
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return m_Writer != nullptr && m_Lock.owns_lock();
            }

        private:
            TextWriterPtr m_Writer;
            std::unique_lock<std::recursive_mutex> m_Lock;
        };

        class TextReader // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
            virtual ~TextReader() = default;

            [[nodiscard]] virtual Result<int> Read() noexcept = 0;
            [[nodiscard]] virtual Result<std::string> ReadLine() = 0;
            [[nodiscard]] virtual Result<std::string> ReadCount(std::size_t count) = 0;
            [[nodiscard]] virtual Result<std::string> ReadUntil(char delimiter, bool includeDelimiter = false) = 0;

            [[nodiscard]] virtual Result<std::string> ReadWord()
            {
                std::string value;
                bool started = false;

                while (true)
                {
                    auto next = Read();
                    if (!next)
                    {
                        if (next.Error().Status == ConsoleStatus::EndOfFile && started)
                            return value;

                        return next.Error();
                    }

                    const auto ch = static_cast<unsigned char>(next.Value());
                    const bool space =
                        ch == ' ' ||
                        ch == '\t' ||
                        ch == '\n' ||
                        ch == '\r' ||
                        ch == '\f' ||
                        ch == '\v';

                    if (!started)
                    {
                        if (space)
                            continue;

                        started = true;
                        value.push_back(static_cast<char>(ch));
                        continue;
                    }

                    if (space)
                        return value;

                    value.push_back(static_cast<char>(ch));
                }
            }
        };

        using TextReaderPtr = std::shared_ptr<TextReader>;

        class TextReaderView final
        {
        public:
            TextReaderView() = default;

            TextReaderView(TextReaderPtr reader, std::recursive_mutex* mutex) noexcept
                : m_Reader(std::move(reader))
                , m_Mutex(mutex)
            {
            }

            [[nodiscard]] Result<int> Read() const noexcept
            {
                if (!m_Reader || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Reader->Read();
            }

            [[nodiscard]] Result<std::string> ReadLine() const
            {
                if (!m_Reader || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Reader->ReadLine();
            }

            [[nodiscard]] Result<std::string> ReadCount(const std::size_t count) const
            {
                if (!m_Reader || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Reader->ReadCount(count);
            }

            [[nodiscard]] Result<std::string> ReadUntil(
                const char delimiter,
                const bool includeDelimiter = false) const
            {
                if (!m_Reader || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Reader->ReadUntil(delimiter, includeDelimiter);
            }

            [[nodiscard]] Result<std::string> ReadWord() const
            {
                if (!m_Reader || m_Mutex == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                auto guard = std::scoped_lock(*m_Mutex);
                return m_Reader->ReadWord();
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return m_Reader != nullptr && m_Mutex != nullptr;
            }

        private:
            TextReaderPtr m_Reader;
            std::recursive_mutex* m_Mutex = nullptr;
        };

        class LockedTextReaderView final // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
            LockedTextReaderView() = default;

            LockedTextReaderView(TextReaderPtr reader, std::unique_lock<std::recursive_mutex>&& lock) noexcept
                : m_Reader(std::move(reader))
                , m_Lock(std::move(lock))
            {
            }

            LockedTextReaderView(const LockedTextReaderView&) = delete;
            LockedTextReaderView& operator=(const LockedTextReaderView&) = delete;
            LockedTextReaderView(LockedTextReaderView&&) noexcept = default;
            LockedTextReaderView& operator=(LockedTextReaderView&&) noexcept = default;

            [[nodiscard]] Result<int> Read() const noexcept
            {
                if (!m_Reader || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Reader->Read();
            }

            [[nodiscard]] Result<std::string> ReadLine() const
            {
                if (!m_Reader || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Reader->ReadLine();
            }

            [[nodiscard]] Result<std::string> ReadCount(const std::size_t count) const
            {
                if (!m_Reader || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Reader->ReadCount(count);
            }

            [[nodiscard]] Result<std::string> ReadUntil(const char delimiter, const bool includeDelimiter = false) const
            {
                if (!m_Reader || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Reader->ReadUntil(delimiter, includeDelimiter);
            }

            [[nodiscard]] Result<std::string> ReadWord() const
            {
                if (!m_Reader || !m_Lock.owns_lock())
                    return MakeConsoleError(ConsoleStatus::NullArgument);

                return m_Reader->ReadWord();
            }

            [[nodiscard]] explicit operator bool() const noexcept
            {
                return m_Reader != nullptr && m_Lock.owns_lock();
            }

        private:
            TextReaderPtr m_Reader;
            std::unique_lock<std::recursive_mutex> m_Lock;
        };

    namespace detail
    {
            [[nodiscard]] constexpr IoCount SumIoCount(const IoCount left, const IoCount right) noexcept
            {
                return IoCount {
                    .Requested = left.Requested + right.Requested,
                    .Processed = left.Processed + right.Processed
                };
            }

            namespace format_detail
            {
                struct FormatItem final
                {
                    std::size_t Index = 0;
                    int Alignment = 0;
                    bool HasAlignment = false;
                    std::string_view Format;
                };

                template <std::size_t Size = 1024>
                class StackText final
                {
                public:
                    [[nodiscard]] char* Data() noexcept { return m_Buffer.data(); }
                    [[nodiscard]] const char* Data() const noexcept { return m_Buffer.data(); }
                    [[nodiscard]] char* End() noexcept { return m_Buffer.data() + m_Buffer.size(); }
                    [[nodiscard]] constexpr std::size_t Capacity() const noexcept { return Size; } // NOLINT(readability-convert-member-functions-to-static)
                    [[nodiscard]] std::size_t SizeValue() const noexcept { return m_Size; }
                    [[nodiscard]] bool Empty() const noexcept { return m_Size == 0; }

                    [[nodiscard]] std::string_view View() const noexcept
                    {
                        return std::string_view(m_Buffer.data(), m_Size);
                    }

                    void Resize(const std::size_t size) noexcept
                    {
                        m_Size = size <= Size ? size : Size;
                    }

                    [[nodiscard]] Result<void> Push(const char ch) noexcept
                    {
                        if (m_Size >= Size)
                            return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                        m_Buffer[m_Size++] = ch;
                        return {};
                    }

                    [[nodiscard]] Result<void> Append(const std::string_view text) noexcept
                    {
                        if (text.size() > Size - m_Size)
                            return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                        for (char ch : text)
                            m_Buffer[m_Size++] = ch;

                        return {};
                    }

                    void Clear() noexcept
                    {
                        m_Size = 0;
                    }

                private:
                    std::array<char, Size> m_Buffer {};
                    std::size_t m_Size = 0;
                };

                [[nodiscard]] constexpr bool IsDigit(const char ch) noexcept
                {
                    return ch >= '0' && ch <= '9';
                }

                [[nodiscard]] constexpr char ToUpperAscii(const char ch) noexcept
                {
                    return ch >= 'a' && ch <= 'z'
                        ? static_cast<char>(ch - 'a' + 'A')
                        : ch;
                }

                [[nodiscard]] constexpr bool IsPlaceholder(const char ch) noexcept
                {
                    return ch == '0' || ch == '#';
                }

                [[nodiscard]] constexpr bool IsStandardNumericKind(const char ch) noexcept
                {
                    const char k = ToUpperAscii(ch);
                    return k == 'G' || k == 'D' || k == 'X' || k == 'B' ||
                           k == 'F' || k == 'E' || k == 'N' || k == 'P' || k == 'C';
                }

                [[nodiscard]] constexpr bool LooksLikeStandardNumericSpecifier(const std::string_view spec) noexcept
                {
                    if (spec.empty() || !IsStandardNumericKind(spec[0]))
                        return false;

                    for (std::size_t i = 1; i < spec.size(); ++i)
                        if (!IsDigit(spec[i]))
                            return false;

                    return true;
                }

                [[nodiscard]] inline Result<unsigned> ParsePrecision(
                    const std::string_view spec,
                    const std::size_t start) noexcept
                {
                    if (start >= spec.size())
                        return 0U;

                    unsigned value = 0;
                    for (std::size_t i = start; i < spec.size(); ++i)
                    {
                        if (!IsDigit(spec[i]))
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                        const unsigned digit = static_cast<unsigned>(spec[i] - '0');
                        if (value > (std::numeric_limits<unsigned>::max() - digit) / 10U)
                            return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                        value = value * 10U + digit;
                    }

                    return value;
                }

                template <typename Sink>
                [[nodiscard]] Result<IoCount> WriteAll(Sink& sink, const std::string_view text)
                {
                    if (text.empty())
                        return IoCount {};

                    return sink.Write(text);
                }

                template <typename Sink>
                [[nodiscard]] Result<IoCount> WriteRepeated(Sink& sink, const char ch, std::size_t count)
                {
                    IoCount total {};
                    std::array<char, 64> block {};
                    block.fill(ch);

                    while (count > 0)
                    {
                        const std::size_t chunk = count < block.size() ? count : block.size();
                        auto result = sink.Write(std::string_view(block.data(), chunk));
                        if (!result)
                            return result;

                        total = SumIoCount(total, result.Value());
                        count -= chunk;
                    }

                    return total;
                }

                template <typename Sink>
                [[nodiscard]] Result<IoCount> WriteAligned(Sink& sink, const std::string_view text, const int alignment)
                {
                    if (alignment == 0)
                        return WriteAll(sink, text);

                    const std::size_t width = alignment < 0
                        ? static_cast<std::size_t>(-alignment)
                        : static_cast<std::size_t>(alignment);

                    if (width <= text.size())
                        return WriteAll(sink, text);

                    const std::size_t pad = width - text.size();
                    IoCount total {};

                    if (alignment > 0)
                    {
                        auto left = WriteRepeated(sink, ' ', pad);
                        if (!left)
                            return left;
                        total = SumIoCount(total, left.Value());
                    }

                    auto body = WriteAll(sink, text);
                    if (!body)
                        return body;
                    total = SumIoCount(total, body.Value());

                    if (alignment < 0)
                    {
                        auto right = WriteRepeated(sink, ' ', pad);
                        if (!right)
                            return right;
                        total = SumIoCount(total, right.Value());
                    }

                    return total;
                }

                [[nodiscard]] inline Result<FormatItem> ParseFormatItem(
                    const std::string_view format,
                    std::size_t& cursor) noexcept
                {
                    std::size_t i = cursor + 1;
                    if (i >= format.size() || !IsDigit(format[i]))
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                    FormatItem item {};
                    while (i < format.size() && IsDigit(format[i]))
                    {
                        const std::size_t digit = static_cast<std::size_t>(format[i] - '0');
                        if (item.Index > (std::numeric_limits<std::size_t>::max() - digit) / 10U)
                            return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                        item.Index = item.Index * 10U + digit;
                        ++i;
                    }

                    while (i < format.size() && format[i] == ' ')
                        ++i;

                    if (i < format.size() && format[i] == ',')
                    {
                        ++i;
                        while (i < format.size() && format[i] == ' ')
                            ++i;

                        item.HasAlignment = true;
                        int sign = 1;
                        if (i < format.size() && format[i] == '-')
                        {
                            sign = -1;
                            ++i;
                        }
                        else if (i < format.size() && format[i] == '+')
                        {
                            ++i;
                        }

                        if (i >= format.size() || !IsDigit(format[i]))
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                        int value = 0;
                        while (i < format.size() && IsDigit(format[i]))
                        {
                            const int digit = format[i] - '0';
                            if (value > (std::numeric_limits<int>::max() - digit) / 10)
                                return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                            value = value * 10 + digit;
                            ++i;
                        }

                        item.Alignment = value * sign;
                        while (i < format.size() && format[i] == ' ')
                            ++i;
                    }

                    if (i < format.size() && format[i] == ':')
                    {
                        const std::size_t start = ++i;
                        bool inSingleQuote = false;
                        bool inDoubleQuote = false;
                        bool escaped = false;

                        while (i < format.size())
                        {
                            const char ch = format[i];
                            if (escaped)
                            {
                                escaped = false;
                                ++i;
                                continue;
                            }

                            if (ch == '\\')
                            {
                                escaped = true;
                                ++i;
                                continue;
                            }

                            if (ch == '\'' && !inDoubleQuote)
                            {
                                inSingleQuote = !inSingleQuote;
                                ++i;
                                continue;
                            }

                            if (ch == '"' && !inSingleQuote)
                            {
                                inDoubleQuote = !inDoubleQuote;
                                ++i;
                                continue;
                            }

                            if (ch == '}' && !inSingleQuote && !inDoubleQuote)
                                break;

                            ++i;
                        }

                        if (i >= format.size() || inSingleQuote || inDoubleQuote || escaped)
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                        item.Format = format.substr(start, i - start);
                    }

                    while (i < format.size() && format[i] == ' ')
                        ++i;

                    if (i >= format.size() || format[i] != '}')
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                    cursor = i;
                    return item;
                }

                template <typename UInt>
                [[nodiscard]] Result<std::string_view> UnsignedToBase(
                    UInt value,
                    const unsigned base,
                    const bool upper,
                    const unsigned minDigits,
                    StackText<1024>& storage) noexcept
                {
                    static_assert(std::is_unsigned_v<UInt>);

                    char temp[128] {};
                    std::size_t size = 0;
                    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

                    do
                    {
                        temp[size++] = digits[value % base];
                        value = static_cast<UInt>(value / base);
                    }
                    while (value != 0 && size < sizeof(temp));

                    while (size < minDigits && size < sizeof(temp))
                        temp[size++] = '0';

                    if (size > storage.Capacity())
                        return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                    for (std::size_t i = 0; i < size; ++i)
                        storage.Data()[i] = temp[size - i - 1];

                    storage.Resize(size);
                    return storage.View();
                }

                template <typename T>
                [[nodiscard]] constexpr auto UnsignedMagnitude(const T value) noexcept
                {
                    using Unsigned = std::make_unsigned_t<T>;
                    if constexpr (std::is_signed_v<T>)
                    {
                        return value < 0
                            ? static_cast<Unsigned>(~static_cast<Unsigned>(value) + 1U)
                            : static_cast<Unsigned>(value);
                    }
                    else
                    {
                        return value;
                    }
                }

                template <typename T>
                [[nodiscard]] constexpr auto UnsignedBitPattern(const T value) noexcept
                {
                    using Unsigned = std::make_unsigned_t<T>;
                    return static_cast<Unsigned>(value);
                }

                [[nodiscard]] inline Result<void> AppendLiteral(
                    StackText<1024>& storage,
                    const std::string_view text,
                    std::size_t& i) noexcept
                {
                    const char ch = text[i];
                    if (ch == '\\')
                    {
                        ++i;
                        if (i >= text.size())
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);
                        return storage.Push(text[i]);
                    }

                    if (ch == '\'' || ch == '"')
                    {
                        const char quote = ch;
                        ++i;
                        while (i < text.size() && text[i] != quote)
                        {
                            auto pushed = storage.Push(text[i]);
                            if (!pushed)
                                return pushed;
                            ++i;
                        }

                        if (i >= text.size())
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                        return {};
                    }

                    return storage.Push(ch);
                }

                [[nodiscard]] inline Result<void> AppendGroupedDigits(
                    StackText<1024>& storage,
                    const std::string_view digits,
                    const bool group) noexcept
                {
                    if (!group)
                        return storage.Append(digits);

                    for (std::size_t i = 0; i < digits.size(); ++i)
                    {
                        if (i != 0 && (digits.size() - i) % 3U == 0U)
                        {
                            auto comma = storage.Push(',');
                            if (!comma)
                                return comma;
                        }

                        auto pushed = storage.Push(digits[i]);
                        if (!pushed)
                            return pushed;
                    }

                    return {};
                }

                [[nodiscard]] inline Result<std::string_view> FormatFixedAbs(
                    const long double value,
                    const unsigned precision,
                    StackText<1024>& storage) noexcept
                {
                    double narrowed = static_cast<double>(value);
                    const auto [ptr, ec] = std::to_chars(
                        storage.Data(),
                        storage.End(),
                        narrowed,
                        std::chars_format::fixed,
                        static_cast<int>(precision)
                    );

                    if (ec != std::errc {})
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                    storage.Resize(static_cast<std::size_t>(ptr - storage.Data()));
                    return storage.View();
                }

                struct CustomSection final
                {
                    std::string_view Text;
                    bool IsExplicit = false;
                };

                [[nodiscard]] inline CustomSection SelectCustomSection(
                    const std::string_view spec,
                    const bool negative,
                    const bool zero) noexcept
                {
                    std::size_t starts[3] { 0, 0, 0 };
                    std::size_t sizes[3] { spec.size(), 0, 0 };
                    std::size_t section = 0;
                    bool inSingleQuote = false;
                    bool inDoubleQuote = false;
                    bool escaped = false;
                    std::size_t start = 0;

                    for (std::size_t i = 0; i < spec.size(); ++i)
                    {
                        const char ch = spec[i];
                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }
                        if (ch == '\\')
                        {
                            escaped = true;
                            continue;
                        }
                        if (ch == '\'' && !inDoubleQuote)
                        {
                            inSingleQuote = !inSingleQuote;
                            continue;
                        }
                        if (ch == '"' && !inSingleQuote)
                        {
                            inDoubleQuote = !inDoubleQuote;
                            continue;
                        }
                        if (ch == ';' && !inSingleQuote && !inDoubleQuote)
                        {
                            if (section < 3)
                            {
                                starts[section] = start;
                                sizes[section] = i - start;
                            }
                            ++section;
                            start = i + 1;
                        }
                    }

                    if (section < 3)
                    {
                        starts[section] = start;
                        sizes[section] = spec.size() - start;
                    }

                    if (zero && section >= 2)
                        return {
                            .Text = spec.substr(starts[2], sizes[2]),
                            .IsExplicit = true
                        };
                    if (negative && section >= 1)
                        return {
                            .Text = spec.substr(starts[1], sizes[1]),
                            .IsExplicit = true
                        };
                    return {
                        .Text = spec.substr(starts[0], sizes[0]),
                        .IsExplicit = true
                    };
                }

                [[nodiscard]] inline bool HasAnyPlaceholder(const std::string_view text) noexcept
                {
                    bool inSingleQuote = false;
                    bool inDoubleQuote = false;
                    bool escaped = false;
                    for (char ch : text)
                    {
                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }
                        if (ch == '\\')
                        {
                            escaped = true;
                            continue;
                        }
                        if (ch == '\'' && !inDoubleQuote)
                        {
                            inSingleQuote = !inSingleQuote;
                            continue;
                        }
                        if (ch == '"' && !inSingleQuote)
                        {
                            inDoubleQuote = !inDoubleQuote;
                            continue;
                        }
                        if (!inSingleQuote && !inDoubleQuote && IsPlaceholder(ch))
                            return true;
                    }
                    return false;
                }

                [[nodiscard]] inline unsigned FractionPlaceholderCount(const std::string_view section) noexcept
                {
                    bool afterDecimal = false;
                    bool inSingleQuote = false;
                    bool inDoubleQuote = false;
                    bool escaped = false;
                    unsigned count = 0;
                    for (char ch : section)
                    {
                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }
                        if (ch == '\\')
                        {
                            escaped = true;
                            continue;
                        }
                        if (ch == '\'' && !inDoubleQuote)
                        {
                            inSingleQuote = !inSingleQuote;
                            continue;
                        }
                        if (ch == '"' && !inSingleQuote)
                        {
                            inDoubleQuote = !inDoubleQuote;
                            continue;
                        }
                        if (inSingleQuote || inDoubleQuote)
                            continue;
                        if (ch == '.')
                        {
                            afterDecimal = true;
                            continue;
                        }
                        if (afterDecimal && IsPlaceholder(ch))
                            ++count;
                    }
                    return count;
                }

                [[nodiscard]] inline long double ApplyCustomScale(
                    long double value,
                    const std::string_view section) noexcept
                {
                    bool inSingleQuote = false;
                    bool inDoubleQuote = false;
                    bool escaped = false;
                    for (char ch : section)
                    {
                        if (escaped)
                        {
                            escaped = false;
                            continue;
                        }
                        if (ch == '\\')
                        {
                            escaped = true;
                            continue;
                        }
                        if (ch == '\'' && !inDoubleQuote)
                        {
                            inSingleQuote = !inSingleQuote;
                            continue;
                        }
                        if (ch == '"' && !inSingleQuote)
                        {
                            inDoubleQuote = !inDoubleQuote;
                            continue;
                        }
                        if (inSingleQuote || inDoubleQuote)
                            continue;
                        if (ch == '%')
                            value *= 100.0L;
                    }
                    return value;
                }

                [[nodiscard]] inline Result<void> RenderPatternLiteral(
                    StackText<1024>& out,
                    const std::string_view text) noexcept
                {
                    for (std::size_t i = 0; i < text.size(); ++i)
                    {
                        auto pushed = AppendLiteral(out, text, i);
                        if (!pushed)
                            return pushed;
                    }
                    return {};
                }

                [[nodiscard]] inline Result<void> RenderIntegerPattern(
                    StackText<1024>& out,
                    const std::string_view pattern,
                    const std::string_view digits) noexcept
                {
                    const bool group = pattern.find(',') != std::string_view::npos;

                    bool containsNonGroupLiteral = false;
                    for (char ch : pattern)
                    {
                        if (!IsPlaceholder(ch) && ch != ',')
                        {
                            containsNonGroupLiteral = true;
                            break;
                        }
                    }

                    if (!containsNonGroupLiteral)
                    {
                        std::size_t minDigits = 0;
                        for (char ch : pattern)
                            if (ch == '0')
                                ++minDigits;

                        StackText<1024> padded;
                        if (digits.size() < minDigits)
                        {
                            for (std::size_t i = 0; i < minDigits - digits.size(); ++i)
                            {
                                auto pushed = padded.Push('0');
                                if (!pushed)
                                    return pushed;
                            }
                        }
                        auto appended = padded.Append(digits);
                        if (!appended)
                            return appended;

                        return AppendGroupedDigits(out, padded.View(), group);
                    }

                    StackText<1024> reversed;
                    std::size_t digitIndex = digits.size();
                    for (std::size_t offset = 0; offset < pattern.size(); ++offset)
                    {
                        const char ch = pattern[pattern.size() - 1U - offset];
                        if (ch == ',')
                            continue;

                        if (IsPlaceholder(ch))
                        {
                            if (digitIndex > 0)
                            {
                                auto pushed = reversed.Push(digits[--digitIndex]);
                                if (!pushed)
                                    return pushed;
                            }
                            else if (ch == '0')
                            {
                                auto pushed = reversed.Push('0');
                                if (!pushed)
                                    return pushed;
                            }
                        }
                        else
                        {
                            auto pushed = reversed.Push(ch);
                            if (!pushed)
                                return pushed;
                        }
                    }

                    while (digitIndex > 0)
                    {
                        auto pushed = reversed.Push(digits[--digitIndex]);
                        if (!pushed)
                            return pushed;
                    }

                    for (std::size_t i = 0; i < reversed.SizeValue(); ++i)
                    {
                        auto pushed = out.Push(reversed.View()[reversed.SizeValue() - 1U - i]);
                        if (!pushed)
                            return pushed;
                    }

                    return {};
                }

                [[nodiscard]] inline Result<void> RenderFractionPattern(
                    StackText<1024>& out,
                    const std::string_view pattern,
                    const std::string_view fractionDigits) noexcept
                {
                    StackText<1024> temp;
                    std::size_t digitIndex = 0;
                    std::size_t optionalTrim = 0;

                    for (char ch : pattern)
                    {
                        if (IsPlaceholder(ch))
                        {
                            const char digit = digitIndex < fractionDigits.size() ? fractionDigits[digitIndex++] : '0';
                            if (ch == '0' || digit != '0')
                            {
                                auto pushed = temp.Push(digit);
                                if (!pushed)
                                    return pushed;
                                optionalTrim = 0;
                            }
                            else
                            {
                                auto pushed = temp.Push('0');
                                if (!pushed)
                                    return pushed;
                                ++optionalTrim;
                            }
                        }
                        else if (ch == ',')
                        {
                            continue;
                        }
                        else
                        {
                            auto pushed = temp.Push(ch);
                            if (!pushed)
                                return pushed;
                            optionalTrim = 0;
                        }
                    }

                    std::size_t size = temp.SizeValue();
                    while (optionalTrim > 0 && size > 0)
                    {
                        --size;
                        --optionalTrim;
                    }
                    temp.Resize(size);

                    if (temp.Empty())
                        return {};

                    auto dot = out.Push('.');
                    if (!dot)
                        return dot;
                    return out.Append(temp.View());
                }

                [[nodiscard]] inline Result<void> RenderCustomNumber(
                    StackText<1024>& out,
                    long double value,
                    const std::string_view spec) noexcept
                {
                    const bool negative = value < 0.0L;
                    const bool zero = value == 0.0L;
                    const CustomSection selected = SelectCustomSection(spec, negative, zero);
                    std::string_view section = selected.Text;

                    if (!HasAnyPlaceholder(section))
                        return RenderPatternLiteral(out, section);

                    const bool implicitNegative = negative && spec.find(';') == std::string_view::npos;
                    value = ApplyCustomScale(std::fabs(value), section);

                    const unsigned precision = FractionPlaceholderCount(section);
                    StackText<1024> fixed;
                    auto fixedView = FormatFixedAbs(value, precision, fixed);
                    if (!fixedView)
                        return fixedView.Error();

                    std::string_view number = fixedView.Value();
                    const std::size_t dotPos = number.find('.');
                    std::string_view integerDigits = dotPos == std::string_view::npos ? number : number.substr(0, dotPos);
                    std::string_view fractionDigits = dotPos == std::string_view::npos ? std::string_view {} : number.substr(dotPos + 1);

                    std::size_t firstPlaceholder = std::string_view::npos;
                    std::size_t lastPlaceholder = std::string_view::npos;
                    for (std::size_t i = 0; i < section.size(); ++i)
                    {
                        if (IsPlaceholder(section[i]))
                        {
                            if (firstPlaceholder == std::string_view::npos)
                                firstPlaceholder = i;
                            lastPlaceholder = i;
                        }
                    }

                    if (implicitNegative)
                    {
                        auto pushed = out.Push('-');
                        if (!pushed)
                            return pushed;
                    }

                    auto prefix = RenderPatternLiteral(out, section.substr(0, firstPlaceholder));
                    if (!prefix)
                        return prefix;

                    std::string_view core = section.substr(firstPlaceholder, lastPlaceholder - firstPlaceholder + 1U);
                    const std::size_t decimal = core.find('.');
                    std::string_view intPattern = decimal == std::string_view::npos ? core : core.substr(0, decimal);
                    std::string_view fracPattern = decimal == std::string_view::npos ? std::string_view {} : core.substr(decimal + 1);

                    auto integer = RenderIntegerPattern(out, intPattern, integerDigits);
                    if (!integer)
                        return integer;

                    if (!fracPattern.empty())
                    {
                        auto fraction = RenderFractionPattern(out, fracPattern, fractionDigits);
                        if (!fraction)
                            return fraction;
                    }

                    auto suffix = RenderPatternLiteral(out, section.substr(lastPlaceholder + 1U));
                    if (!suffix)
                        return suffix;

                    return {};
                }

                template <typename T>
                [[nodiscard]] Result<std::string_view> FormatFloating(
                    T value,
                    std::string_view spec,
                    StackText<1024>& storage) noexcept;

                template <typename T>
                [[nodiscard]] Result<std::string_view> FormatIntegral(
                    const T value,
                    const std::string_view spec,
                    StackText<1024>& storage) noexcept
                {
                    static_assert(std::is_integral_v<T>);

                    if (spec.empty())
                    {
                        const auto [ptr, ec] = std::to_chars(storage.Data(), storage.End(), value);
                        if (ec != std::errc {})
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);
                        storage.Resize(static_cast<std::size_t>(ptr - storage.Data()));
                        return storage.View();
                    }

                    if (LooksLikeStandardNumericSpecifier(spec))
                    {
                        const char kind = ToUpperAscii(spec[0]);
                        auto precision = ParsePrecision(spec, 1);
                        if (!precision)
                            return precision.Error();

                        switch (kind)
                        {
                        case 'G':
                        case 'D':
                        {
                            char* begin = storage.Data();
                            char* out = begin;
                            if constexpr (std::is_signed_v<T>)
                            {
                                if (value < 0)
                                    *out++ = '-';
                            }

                            char temp[128] {};
                            auto mag = UnsignedMagnitude(value);
                            const auto [ptr, ec] = std::to_chars(temp, temp + sizeof(temp), mag);
                            if (ec != std::errc {})
                                return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                            const std::size_t digitCount = static_cast<std::size_t>(ptr - temp);
                            const std::size_t zeroCount = precision.Value() > digitCount ? precision.Value() - digitCount : 0;
                            if (static_cast<std::size_t>(out - begin) + zeroCount + digitCount > storage.Capacity())
                                return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                            for (std::size_t i = 0; i < zeroCount; ++i)
                                *out++ = '0';
                            for (std::size_t i = 0; i < digitCount; ++i)
                                *out++ = temp[i];

                            storage.Resize(static_cast<std::size_t>(out - begin));
                            return storage.View();
                        }
                        case 'X':
                            return UnsignedToBase(UnsignedBitPattern(value), 16, spec[0] == 'X', precision.Value(), storage);
                        case 'B':
                            return UnsignedToBase(UnsignedBitPattern(value), 2, true, precision.Value(), storage);
                        case 'F':
                        case 'N':
                        case 'P':
                        case 'C':
                        case 'E':
                            return FormatFloating(static_cast<long double>(value), spec, storage);
                        default:
                            break;
                        }
                    }

                    auto rendered = RenderCustomNumber(storage, static_cast<long double>(value), spec);
                    if (!rendered)
                        return rendered.Error();
                    return storage.View();
                }

                [[nodiscard]] inline Result<std::string_view> InsertInvariantGroupSeparators(
                    const std::string_view digits,
                    StackText<1024>& storage) noexcept
                {
                    bool negative = !digits.empty() && digits.front() == '-';
                    const std::size_t start = negative ? 1U : 0U;
                    const std::size_t digitCount = digits.size() - start;
                    const std::size_t commaCount = digitCount > 3 ? (digitCount - 1U) / 3U : 0U;

                    if (digits.size() + commaCount > storage.Capacity())
                        return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);

                    std::size_t out = 0;
                    if (negative)
                        storage.Data()[out++] = '-';

                    for (std::size_t i = 0; i < digitCount; ++i)
                    {
                        if (i != 0 && (digitCount - i) % 3U == 0U)
                            storage.Data()[out++] = ',';
                        storage.Data()[out++] = digits[start + i];
                    }

                    storage.Resize(out);
                    return storage.View();
                }

                template <typename T>
                [[nodiscard]] Result<std::string_view> FormatFloating(
                    const T value,
                    const std::string_view spec,
                    StackText<1024>& storage) noexcept
                {
                    static_assert(std::is_floating_point_v<T> || std::is_same_v<T, long double>);

                    if (!spec.empty() && !LooksLikeStandardNumericSpecifier(spec))
                    {
                        auto rendered = RenderCustomNumber(storage, static_cast<long double>(value), spec);
                        if (!rendered)
                            return rendered.Error();
                        return storage.View();
                    }

                    char kind = 'G';
                    unsigned precision = 0;
                    bool hasPrecision = false;

                    if (!spec.empty())
                    {
                        kind = ToUpperAscii(spec[0]);
                        auto parsed = ParsePrecision(spec, 1);
                        if (!parsed)
                            return parsed.Error();
                        precision = parsed.Value();
                        hasPrecision = spec.size() > 1;
                    }

                    std::chars_format fmt;
                    switch (kind)
                    {
                    case 'G':
                        fmt = std::chars_format::general;
                        break;
                    case 'F':
                    case 'N':
                    case 'C':
                    case 'P':
                        fmt = std::chars_format::fixed;
                        if (!hasPrecision)
                            precision = 2;
                        break;
                    case 'E':
                        fmt = std::chars_format::scientific;
                        if (!hasPrecision)
                            precision = 6;
                        break;
                    default:
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);
                    }

                    long double normalized = static_cast<long double>(value);
                    if (kind == 'P')
                        normalized *= 100.0L;

                    char* begin = storage.Data();
                    char* numericBegin = begin;
                    if (kind == 'C')
                        *numericBegin++ = '$';

                    double narrowed = static_cast<double>(normalized);
                    std::to_chars_result converted;
                    if (kind == 'G' && !hasPrecision)
                        converted = std::to_chars(numericBegin, storage.End(), narrowed, fmt);
                    else
                        converted = std::to_chars(numericBegin, storage.End(), narrowed, fmt, static_cast<int>(precision));

                    if (converted.ec != std::errc {})
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                    std::size_t size = static_cast<std::size_t>(converted.ptr - begin);
                    storage.Resize(size);

                    if (kind == 'P')
                    {
                        if (size + 1U > storage.Capacity())
                            return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);
                        storage.Data()[size++] = '%';
                        storage.Resize(size);
                    }

                    if (kind != 'N' && kind != 'C')
                        return storage.View();

                    StackText<1024> grouped;
                    StackText<1024> finalText;
                    std::string_view view = storage.View();
                    std::size_t prefix = 0;
                    if (!view.empty() && view[0] == '$')
                        prefix = 1;

                    std::size_t dot = prefix;
                    while (dot < view.size() && view[dot] != '.')
                        ++dot;

                    auto intGrouped = InsertInvariantGroupSeparators(view.substr(prefix, dot - prefix), grouped);
                    if (!intGrouped)
                        return intGrouped.Error();

                    if (prefix == 1)
                    {
                        auto pushed = finalText.Push('$');
                        if (!pushed)
                            return pushed.Error();
                    }
                    auto appended = finalText.Append(intGrouped.Value());
                    if (!appended)
                        return appended.Error();
                    appended = finalText.Append(view.substr(dot));
                    if (!appended)
                        return appended.Error();

                    storage.Clear();
                    appended = storage.Append(finalText.View());
                    if (!appended)
                        return appended.Error();
                    return storage.View();
                }

                [[nodiscard]] inline Result<std::string_view> FormatStringLike(
                    const std::string_view value,
                    const std::string_view spec) noexcept
                {
                    if (!spec.empty())
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                    return value;
                }

                [[nodiscard]] inline Result<std::string_view> FormatDateTime(
                    const std::tm& value,
                    const std::string_view spec,
                    StackText<1024>& storage) noexcept
                {
                    std::string_view pattern = spec.empty() ? "%Y-%m-%d %H:%M:%S" : spec;
                    StackText<256> translated;

                    for (std::size_t i = 0; i < pattern.size(); ++i)
                    {
                        const char ch = pattern[i];
                        auto append = [&](std::string_view text) -> Result<void> { return translated.Append(text); };

                        if (pattern.substr(i, 4) == "yyyy") { auto r = append("%Y"); if (!r) return r.Error(); i += 3; }
                        else if (pattern.substr(i, 2) == "yy") { auto r = append("%y"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "MM") { auto r = append("%m"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "dd") { auto r = append("%d"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "HH") { auto r = append("%H"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "hh") { auto r = append("%I"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "mm") { auto r = append("%M"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "ss") { auto r = append("%S"); if (!r) return r.Error(); i += 1; }
                        else if (pattern.substr(i, 2) == "tt") { auto r = append("%p"); if (!r) return r.Error(); i += 1; }
                        else
                        {
                            auto r = translated.Push(ch);
                            if (!r)
                                return r.Error();
                        }
                    }

                    auto nul = translated.Push('\0');
                    if (!nul)
                        return nul.Error();

                    const std::size_t count = std::strftime(storage.Data(), storage.Capacity(), translated.Data(), &value);
                    if (count == 0)
                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                    storage.Resize(count);
                    return storage.View();
                }

                template <typename Sink>
                [[nodiscard]] Result<IoCount> WriteFormattedView(
                    Sink& sink,
                    const std::string_view value,
                    const std::string_view spec,
                    const int alignment)
                {
                    auto formatted = FormatStringLike(value, spec);
                    if (!formatted)
                        return formatted.Error();

                    return WriteAligned(sink, formatted.Value(), alignment);
                }

                template <typename Sink, typename T>
                [[nodiscard]] Result<IoCount> WriteFormattedValue(
                    Sink& sink,
                    const T& value,
                    const std::string_view spec,
                    const int alignment)
                {
                    using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;

                    if constexpr (std::is_same_v<ValueType, bool>)
                    {
                        return WriteFormattedView(sink, value ? "True" : "False", spec, alignment);
                    }
                    else if constexpr (std::is_same_v<ValueType, char>)
                    {
                        const char ch = value;
                        return WriteFormattedView(sink, std::string_view(&ch, 1), spec, alignment);
                    }
                    else if constexpr (std::is_same_v<ValueType, const char*> || std::is_same_v<ValueType, char*>)
                    {
                        if (value == nullptr)
                            return MakeConsoleError(ConsoleStatus::NullArgument, ConsoleErrorDomain::Formatting);
                        return WriteFormattedView(sink, std::string_view(value), spec, alignment);
                    }
                    else if constexpr (std::is_same_v<ValueType, std::string>)
                    {
                        return WriteFormattedView(sink, std::string_view(value), spec, alignment);
                    }
                    else if constexpr (std::is_same_v<ValueType, std::string_view>)
                    {
                        return WriteFormattedView(sink, value, spec, alignment);
                    }
                    else if constexpr (std::is_integral_v<ValueType>)
                    {
                        StackText<1024> storage;
                        auto view = FormatIntegral(value, spec, storage);
                        if (!view)
                            return view.Error();
                        return WriteAligned(sink, view.Value(), alignment);
                    }
                    else if constexpr (std::is_floating_point_v<ValueType>)
                    {
                        StackText<1024> storage;
                        auto view = FormatFloating(value, spec, storage);
                        if (!view)
                            return view.Error();
                        return WriteAligned(sink, view.Value(), alignment);
                    }
                    else if constexpr (std::is_same_v<ValueType, TextDecimal>)
                    {
                        if (spec.empty())
                            return WriteFormattedView(sink, std::string_view(value.Text), spec, alignment);

                        char* end = nullptr;
                        const long double parsed = std::strtold(value.Text.c_str(), &end);
                        if (end == value.Text.c_str() || *end != '\0')
                            return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);

                        StackText<1024> storage;
                        auto view = FormatFloating(parsed, spec, storage);
                        if (!view)
                            return view.Error();
                        return WriteAligned(sink, view.Value(), alignment);
                    }
                    else if constexpr (std::is_same_v<ValueType, std::tm>)
                    {
                        StackText<1024> storage;
                        auto view = FormatDateTime(value, spec, storage);
                        if (!view)
                            return view.Error();
                        return WriteAligned(sink, view.Value(), alignment);
                    }
                    else if constexpr (std::is_base_of_v<ConsoleObject, ValueType>)
                    {
                        try
                        {
                            std::string text = value.ToConsoleString();
                            return WriteFormattedView(sink, std::string_view(text), spec, alignment);
                        }
                        catch (...)
                        {
                            return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Formatting);
                        }
                    }
                    else
                    {
                        return MakeConsoleError(ConsoleStatus::Unsupported, ConsoleErrorDomain::Formatting);
                    }
                }

                template <std::size_t Index, typename Sink, typename Tuple>
                [[nodiscard]] Result<IoCount> WriteArgByIndex(
                    Sink& sink,
                    Tuple& args,
                    const std::size_t requestedIndex,
                    const std::string_view spec,
                    const int alignment)
                {
                    if constexpr (Index >= std::tuple_size_v<std::remove_reference_t<Tuple>>)
                    {
                        (void)sink;
                        (void)args;
                        (void)requestedIndex;
                        (void)spec;
                        (void)alignment;
                        return MakeConsoleError(ConsoleStatus::InvalidRange, ConsoleErrorDomain::Formatting);
                    }
                    else
                    {
                        if (requestedIndex == Index)
                            return WriteFormattedValue(sink, std::get<Index>(args), spec, alignment);

                        return WriteArgByIndex<Index + 1>(sink, args, requestedIndex, spec, alignment);
                    }
                }
            }

            template <typename Sink, typename T>
            [[nodiscard]] Result<IoCount> WriteConsoleValueTo(Sink& sink, const T& value)
            {
                return format_detail::WriteFormattedValue(sink, value, {}, 0);
            }

            template <typename Sink, typename... Args>
            [[nodiscard]] Result<IoCount> FormatCompositeTo(
                Sink& sink,
                const std::string_view format,
                const Args&... args)
            {
                auto tuple = std::forward_as_tuple(args...);
                IoCount total {};
                std::size_t literalStart = 0;

                auto flushLiteral = [&](const std::size_t end) -> Result<void>
                {
                    if (end <= literalStart)
                        return {};

                    auto written = format_detail::WriteAll(sink, format.substr(literalStart, end - literalStart));
                    if (!written)
                        return written.Error();

                    total = SumIoCount(total, written.Value());
                    return {};
                };

                for (std::size_t i = 0; i < format.size(); ++i)
                {
                    const char ch = format[i];

                    if (ch == '{')
                    {
                        if (i + 1 < format.size() && format[i + 1] == '{')
                        {
                            auto flushed = flushLiteral(i);
                            if (!flushed)
                                return flushed.Error();

                            auto literal = format_detail::WriteAll(sink, "{");
                            if (!literal)
                                return literal;

                            total = SumIoCount(total, literal.Value());
                            ++i;
                            literalStart = i + 1;
                            continue;
                        }

                        auto flushed = flushLiteral(i);
                        if (!flushed)
                            return flushed.Error();

                        auto item = format_detail::ParseFormatItem(format, i);
                        if (!item)
                            return item.Error();

                        auto formatted = format_detail::WriteArgByIndex<0>(
                            sink,
                            tuple,
                            item.Value().Index,
                            item.Value().Format,
                            item.Value().HasAlignment ? item.Value().Alignment : 0
                        );

                        if (!formatted)
                            return formatted;

                        total = SumIoCount(total, formatted.Value());
                        literalStart = i + 1;
                        continue;
                    }

                    if (ch == '}')
                    {
                        if (i + 1 < format.size() && format[i + 1] == '}')
                        {
                            auto flushed = flushLiteral(i);
                            if (!flushed)
                                return flushed.Error();

                            auto literal = format_detail::WriteAll(sink, "}");
                            if (!literal)
                                return literal;

                            total = SumIoCount(total, literal.Value());
                            ++i;
                            literalStart = i + 1;
                            continue;
                        }

                        return MakeConsoleError(ConsoleStatus::FormatError, ConsoleErrorDomain::Formatting);
                    }
                }

                auto flushed = flushLiteral(format.size());
                if (!flushed)
                    return flushed.Error();

                return total;
            }

            class StringFormatSink final
            {
            public:
                explicit StringFormatSink(std::string& output) noexcept
                    : m_Output(output)
                {
                }

                [[nodiscard]] Result<IoCount> Write(const std::string_view text) noexcept // NOLINT(readability-make-member-function-const)
                {
                    try
                    {
                        m_Output.append(text.data(), text.size());
                    }
                    catch (const std::bad_alloc&)
                    {
                        return MakeConsoleError(ConsoleStatus::IoError, ConsoleErrorDomain::Formatting);
                    }
                    catch (...)
                    {
                        return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Formatting);
                    }

                    return IoCount { .Requested = text.size(), .Processed = text.size() };
                }

            private:
                std::string& m_Output; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members) TODO: Are we sure about this?
            };

            template <typename... Args>
            [[nodiscard]] Result<std::string> FormatComposite(const std::string_view format, const Args&... args)
            {
                std::string output;
                try
                {
                    output.reserve(format.size());
                }
                catch (const std::bad_alloc&)
                {
                    return MakeConsoleError(ConsoleStatus::IoError, ConsoleErrorDomain::Formatting);
                }
                catch (...)
                {
                    return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Formatting);
                }

                StringFormatSink sink(output);
                auto result = FormatCompositeTo(sink, format, args...);
                if (!result)
                    return result.Error();
                return output;
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const bool value)
            {
                return std::string(value ? "True" : "False");
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const char value)
            {
                return std::string(1, value);
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const char* value)
            {
                if (value == nullptr)
                    return MakeConsoleError(ConsoleStatus::NullArgument, ConsoleErrorDomain::Formatting);

                return std::string(value);
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(char* value)
            {
                return ToConsoleString(static_cast<const char*>(value));
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const std::string& value)
            {
                return value;
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const std::string_view value)
            {
                return std::string(value);
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const TextDecimal& value)
            {
                return value.Text;
            }

            [[nodiscard]] inline Result<std::string> ToConsoleString(const ConsoleObject& value)
            {
                try
                {
                    return value.ToConsoleString();
                }
                catch (...)
                {
                    return MakeConsoleError(ConsoleStatus::UnknownError, ConsoleErrorDomain::Formatting);
                }
            }

            template <std::size_t Size>
            [[nodiscard]] Result<std::string> ToConsoleString(const char (&value)[Size])
            {
                static_assert(Size > 0);
                return std::string(value, value + Size - 1);
            }

            template <typename T>
            [[nodiscard]] Result<std::string> ToConsoleString(const T& value)
            {
                std::string output;
                StringFormatSink sink(output);
                auto result = WriteConsoleValueTo(sink, value);
                if (!result)
                    return result.Error();
                return output;
            }
    }

    class ScopedConsoleLock;

    class ScopedConsoleLock final
        {
        public:
            ScopedConsoleLock() = default;
            ~ScopedConsoleLock() = default;

            ScopedConsoleLock(const ScopedConsoleLock&) = delete;
            ScopedConsoleLock& operator=(const ScopedConsoleLock&) = delete;

            ScopedConsoleLock(ScopedConsoleLock&&) noexcept = default;
            ScopedConsoleLock& operator=(ScopedConsoleLock&&) noexcept = default;

        private:
            friend ScopedConsoleLock Lock();

            explicit ScopedConsoleLock(std::unique_lock<std::recursive_mutex>&& lock) noexcept
                : m_Lock(std::move(lock))
            {
            }

        private:
            std::unique_lock<std::recursive_mutex> m_Lock;
        };

        [[nodiscard]] ScopedConsoleLock Lock();
        [[nodiscard]] Result<ConsoleCapabilities> Capabilities() noexcept;

        [[nodiscard]] Result<ConsoleColor> BackgroundColor() noexcept;
        Result<void> BackgroundColor(ConsoleColor color) noexcept;

        [[nodiscard]] Result<int> BufferHeight() noexcept;
        Result<void> BufferHeight(int value) noexcept;

        [[nodiscard]] Result<int> BufferWidth() noexcept;
        Result<void> BufferWidth(int value) noexcept;

        [[nodiscard]] Result<bool> CapsLock() noexcept;

        [[nodiscard]] Result<int> CursorLeft() noexcept;
        Result<void> CursorLeft(int value) noexcept;

        [[nodiscard]] Result<int> CursorSize() noexcept;
        Result<void> CursorSize(int value) noexcept;

        [[nodiscard]] Result<int> CursorTop() noexcept;
        Result<void> CursorTop(int value) noexcept;

        [[nodiscard]] Result<bool> CursorVisible() noexcept;
        Result<void> CursorVisible(bool value) noexcept;

        [[nodiscard]] Result<TextWriterView> Error() noexcept;
        [[nodiscard]] Result<LockedTextWriterView> LockedError();

        [[nodiscard]] Result<ConsoleColor> ForegroundColor() noexcept;
        Result<void> ForegroundColor(ConsoleColor color) noexcept;

        [[nodiscard]] Result<TextReaderView> In() noexcept;
        [[nodiscard]] Result<LockedTextReaderView> LockedIn();

        [[nodiscard]] Result<ConsoleEncoding> InputEncoding() noexcept;
        Result<void> InputEncoding(ConsoleEncoding encoding) noexcept;

        [[nodiscard]] Result<bool> IsErrorRedirected() noexcept;
        [[nodiscard]] Result<bool> IsInputRedirected() noexcept;
        [[nodiscard]] Result<bool> IsOutputRedirected() noexcept;

        [[nodiscard]] Result<bool> KeyAvailable() noexcept;

        [[nodiscard]] Result<int> LargestWindowHeight() noexcept;
        [[nodiscard]] Result<int> LargestWindowWidth() noexcept;

        [[nodiscard]] Result<bool> NumberLock() noexcept;

        [[nodiscard]] Result<std::string> NewLine();
        Result<void> NewLine(std::string_view value);

        [[nodiscard]] Result<TextWriterView> Out() noexcept;
        [[nodiscard]] Result<LockedTextWriterView> LockedOut();

        [[nodiscard]] Result<ConsoleEncoding> OutputEncoding() noexcept;
        Result<void> OutputEncoding(ConsoleEncoding encoding) noexcept;

        [[nodiscard]] Result<std::string> Title();
        Result<void> Title(std::string_view value);

        [[nodiscard]] Result<bool> TreatControlCAsInput() noexcept;
        Result<void> TreatControlCAsInput(bool value) noexcept;

        [[nodiscard]] Result<int> WindowHeight() noexcept;
        Result<void> WindowHeight(int value) noexcept;

        [[nodiscard]] Result<int> WindowLeft() noexcept;
        Result<void> WindowLeft(int value) noexcept;

        [[nodiscard]] Result<int> WindowTop() noexcept;
        Result<void> WindowTop(int value) noexcept;

        [[nodiscard]] Result<int> WindowWidth() noexcept;
        Result<void> WindowWidth(int value) noexcept;

        [[nodiscard]] Result<void> Beep() noexcept;
        Result<void> Beep(int frequency, int durationMs) noexcept;

        [[nodiscard]] Result<void> Clear() noexcept;

        Result<CursorPosition> GetCursorPosition() noexcept;

        Result<void> MoveBufferArea(
            int sourceLeft,
            int sourceTop,
            int sourceWidth,
            int sourceHeight,
            int targetLeft,
            int targetTop) noexcept;

        Result<void> MoveBufferArea(
            int sourceLeft,
            int sourceTop,
            int sourceWidth,
            int sourceHeight,
            int targetLeft,
            int targetTop,
            char32_t sourceChar,
            ConsoleColor sourceForeColor,
            ConsoleColor sourceBackColor) noexcept;

        Result<void> MoveBufferArea(const MoveBufferAreaOptions& options) noexcept;

        [[nodiscard]] Result<StandardStream> OpenStandardError(std::size_t bufferSize = 0) noexcept;
        [[nodiscard]] Result<StandardStream> OpenStandardInput(std::size_t bufferSize = 0) noexcept;
        [[nodiscard]] Result<StandardStream> OpenStandardOutput(std::size_t bufferSize = 0) noexcept;

        [[nodiscard]] Result<int> Read() noexcept;
        [[nodiscard]] Result<std::string> ReadCount(std::size_t count);
        [[nodiscard]] Result<int> ReadHidden() noexcept;
        [[nodiscard]] Result<ConsoleKeyInfo> ReadKey();
        [[nodiscard]] Result<ConsoleKeyInfo> ReadKey(bool intercept);
        [[nodiscard]] Result<std::string> ReadLine();
        [[nodiscard]] Result<std::string> ReadUntil(char delimiter, bool includeDelimiter = false);
        [[nodiscard]] Result<std::string> ReadWord();

        [[nodiscard]] Result<void> ResetColor() noexcept;

        Result<void> SetBufferSize(int width, int height) noexcept;
        Result<void> SetCursorPosition(int left, int top) noexcept;
        Result<void> SetError(TextWriterPtr writer) noexcept;
        Result<void> SetIn(TextReaderPtr reader) noexcept;
        Result<void> SetOut(TextWriterPtr writer) noexcept;
        Result<void> SetWindowPosition(int left, int top) noexcept;
        Result<void> SetWindowSize(int width, int height) noexcept;

        [[nodiscard]] Result<void> FlushError() noexcept;
        [[nodiscard]] Result<void> FlushOut() noexcept;

        Result<IoCount> Write(std::string_view value) noexcept;
        Result<IoCount> Write(const char* value) noexcept;
        Result<IoCount> Write(char value) noexcept;
        Result<IoCount> Write(bool value) noexcept;
        Result<IoCount> Write(const char* buffer, int index, int count) noexcept;
        Result<IoCount> Write(std::string_view value, std::size_t index, std::size_t count) noexcept;
        Result<IoCount> Write(std::span<const char> value) noexcept;
        Result<IoCount> Write(std::span<const char> value, std::size_t index, std::size_t count) noexcept;
        Result<IoCount> Write(const TextDecimal& value) noexcept;
        Result<IoCount> Write(const ConsoleObject& value) noexcept;

        template <typename T>
        Result<IoCount> Write(const T& value);

        Result<IoCount> WriteLine() noexcept;
        Result<IoCount> WriteLine(std::string_view value) noexcept;
        Result<IoCount> WriteLine(const char* value) noexcept;
        Result<IoCount> WriteLine(char value) noexcept;
        Result<IoCount> WriteLine(bool value) noexcept;
        Result<IoCount> WriteLine(const char* buffer, int index, int count) noexcept;
        Result<IoCount> WriteLine(std::string_view value, std::size_t index, std::size_t count) noexcept;
        Result<IoCount> WriteLine(std::span<const char> value) noexcept;
        Result<IoCount> WriteLine(std::span<const char> value, std::size_t index, std::size_t count) noexcept;
        Result<IoCount> WriteLine(const TextDecimal& value) noexcept;
        Result<IoCount> WriteLine(const ConsoleObject& value) noexcept;

        template <typename T>
        Result<IoCount> WriteLine(const T& value);

        template <typename... Args>
        Result<IoCount> WriteFormat(std::string_view format, const Args&... args);

        template <typename... Args>
        Result<IoCount> WriteLineFormat(std::string_view format, const Args&... args);

        Result<IoCount> WriteError(std::string_view value) noexcept;
        Result<IoCount> WriteErrorLine() noexcept;
        Result<IoCount> WriteErrorLine(std::string_view value) noexcept;

        template <typename T>
        Result<IoCount> Write(const T& value)
        {
            auto out = LockedOut();
            if (!out)
                return out.Error();

            return detail::WriteConsoleValueTo(out.Value(), value);
        }

        template <typename T>
        Result<IoCount> WriteLine(const T& value)
        {
            auto out = LockedOut();
            if (!out)
                return out.Error();

            auto first = detail::WriteConsoleValueTo(out.Value(), value);
            if (!first)
                return first;

            auto line = NewLine();
            if (!line)
                return line.Error();

            auto second = out.Value().Write(line.Value());
            if (!second)
                return second;

            return detail::SumIoCount(first.Value(), second.Value());
        }

        template <typename... Args>
        Result<IoCount> WriteFormat(
            const std::string_view format,
            const Args&... args)
        {
            auto out = LockedOut();
            if (!out)
                return out.Error();

            return detail::FormatCompositeTo(out.Value(), format, args...);
        }

        template <typename... Args>
        Result<IoCount> WriteLineFormat(
            const std::string_view format,
            const Args&... args)
        {
            auto out = LockedOut();
            if (!out)
                return out.Error();

            auto first = detail::FormatCompositeTo(out.Value(), format, args...);
            if (!first)
                return first;

            auto line = NewLine();
            if (!line)
                return line.Error();

            auto second = out.Value().Write(line.Value());
            if (!second)
                return second;

            return detail::SumIoCount(first.Value(), second.Value());
        }
}

namespace wio::runtime::std_console
{
    using StatusCode = std::uint8_t;
    using ErrorDomainCode = std::uint8_t;
    using ColorCode = std::uint8_t;
    using EncodingCode = std::uint8_t;
    using KeyCode = std::int32_t;
    using ModifierMask = std::uint8_t;
    using SpecialKeyCode = std::int32_t;
    using Status = console::ConsoleStatus;
    using ErrorDomain = console::ConsoleErrorDomain;
    using Color = console::ConsoleColor;
    using Encoding = console::ConsoleEncoding;
    using Key = console::ConsoleKey;
    using Modifiers = console::ConsoleModifiers;
    using SpecialKey = console::ConsoleSpecialKey;

    namespace detail
    {
        void StoreLastError(const console::ConsoleError& error) noexcept;
        void ClearStoredLastError() noexcept;
    }

    void ClearLastError() noexcept;
    [[nodiscard]] StatusCode LastStatus() noexcept;
    [[nodiscard]] ErrorDomainCode LastErrorDomain() noexcept;
    [[nodiscard]] int LastNativeCode() noexcept;
    [[nodiscard]] int LastErrorLine() noexcept;
    [[nodiscard]] std::string LastErrorFile();
    [[nodiscard]] std::string StatusName(StatusCode status);
    [[nodiscard]] std::string ErrorDomainName(ErrorDomainCode domain);
    [[nodiscard]] inline Status LastStatusValue() noexcept
    {
        return static_cast<Status>(LastStatus());
    }

    [[nodiscard]] inline ErrorDomain LastErrorDomainValue() noexcept
    {
        return static_cast<ErrorDomain>(LastErrorDomain());
    }

    [[nodiscard]] inline int StatusValue(const Status status) noexcept
    {
        return static_cast<int>(status);
    }

    [[nodiscard]] inline int ErrorDomainValue(const ErrorDomain domain) noexcept
    {
        return static_cast<int>(domain);
    }

    [[nodiscard]] inline std::string StatusName(const Status status)
    {
        return StatusName(static_cast<StatusCode>(status));
    }

    [[nodiscard]] inline std::string ErrorDomainName(const ErrorDomain domain)
    {
        return ErrorDomainName(static_cast<ErrorDomainCode>(domain));
    }

    std::int32_t WriteValue(bool value);
    std::int32_t WriteValue(char value);
    std::int32_t WriteValue(std::int8_t value);
    std::int32_t WriteValue(std::int16_t value);
    std::int32_t WriteValue(std::int32_t value);
    std::int32_t WriteValue(std::int64_t value);
    std::int32_t WriteValue(std::uint8_t value);
    std::int32_t WriteValue(std::uint16_t value);
    std::int32_t WriteValue(std::uint32_t value);
    std::int32_t WriteValue(std::uint64_t value);
    std::int32_t WriteValue(float value);
    std::int32_t WriteValue(double value);
    std::int32_t WriteValue(const char* value);
    std::int32_t WriteValue(char* value);
    std::int32_t WriteValue(const std::string& value);
    std::int32_t WriteValue(std::string_view value);
    std::int32_t WriteValue(const wio::runtime::Text& value);

    std::int32_t WriteLine() noexcept;
    std::int32_t WriteLineValue(bool value);
    std::int32_t WriteLineValue(char value);
    std::int32_t WriteLineValue(std::int8_t value);
    std::int32_t WriteLineValue(std::int16_t value);
    std::int32_t WriteLineValue(std::int32_t value);
    std::int32_t WriteLineValue(std::int64_t value);
    std::int32_t WriteLineValue(std::uint8_t value);
    std::int32_t WriteLineValue(std::uint16_t value);
    std::int32_t WriteLineValue(std::uint32_t value);
    std::int32_t WriteLineValue(std::uint64_t value);
    std::int32_t WriteLineValue(float value);
    std::int32_t WriteLineValue(double value);
    std::int32_t WriteLineValue(const char* value);
    std::int32_t WriteLineValue(char* value);
    std::int32_t WriteLineValue(const std::string& value);
    std::int32_t WriteLineValue(std::string_view value);
    std::int32_t WriteLineValue(const wio::runtime::Text& value);
    std::int32_t WriteSegment(std::string_view value, std::size_t index, std::size_t count);
    std::int32_t WriteBuffer(const char* buffer, int index, int count) noexcept;
    std::int32_t WriteTextDecimal(std::string_view value);
    std::int32_t WriteLineTextDecimal(std::string_view value);
    std::int32_t WriteErrorText(std::string_view value) noexcept;
    std::int32_t WriteErrorLine() noexcept;
    std::int32_t WriteErrorLineText(std::string_view value) noexcept;

    template <typename... Args>
    [[nodiscard]] std::string Format(std::string_view format, const Args&... args);

    template <typename... Args>
    [[nodiscard]] StatusCode WriteFormat(std::string_view format, const Args&... args)
    {
        auto formatted = Format(format, args...);
        if (LastStatus() != 0)
            return LastStatus();

        return static_cast<StatusCode>(WriteValue(formatted));
    }

    template <typename... Args>
    [[nodiscard]] StatusCode WriteLineFormat(std::string_view format, const Args&... args)
    {
        auto formatted = Format(format, args...);
        if (LastStatus() != 0)
            return LastStatus();

        return static_cast<StatusCode>(WriteLineValue(formatted));
    }

    template <typename... Args>
    [[nodiscard]] std::string Format(std::string_view format, const Args&... args)
    {
        auto result = console::detail::FormatComposite(format, args...);
        if (!result)
        {
            detail::StoreLastError(result.Error());
            return {};
        }

        detail::ClearStoredLastError();
        return std::move(result).Value();
    }

    std::string Input();
    std::string Input(const std::string& prompt);
    std::string InputN(std::size_t count);
    char InputChar(bool isHidden);
    char InputChar();
    std::string InputWord();
    std::string InputUntil(char delimiter, bool includeDelimiter);
    std::string InputUntil(char delimiter);

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
        bool& encodingSet) noexcept;

    [[nodiscard]] StatusCode GetBackgroundColor(ColorCode& color) noexcept;
    [[nodiscard]] StatusCode SetBackgroundColor(ColorCode color) noexcept;
    [[nodiscard]] StatusCode GetForegroundColor(ColorCode& color) noexcept;
    [[nodiscard]] StatusCode SetForegroundColor(ColorCode color) noexcept;
    [[nodiscard]] inline StatusCode GetBackgroundColor(Color& color) noexcept
    {
        ColorCode raw = 0;
        const StatusCode status = GetBackgroundColor(raw);
        color = static_cast<Color>(raw);
        return status;
    }

    [[nodiscard]] inline StatusCode SetBackgroundColor(const Color color) noexcept
    {
        return SetBackgroundColor(static_cast<ColorCode>(color));
    }

    [[nodiscard]] inline StatusCode GetForegroundColor(Color& color) noexcept
    {
        ColorCode raw = 0;
        const StatusCode status = GetForegroundColor(raw);
        color = static_cast<Color>(raw);
        return status;
    }

    [[nodiscard]] inline StatusCode SetForegroundColor(const Color color) noexcept
    {
        return SetForegroundColor(static_cast<ColorCode>(color));
    }

    [[nodiscard]] StatusCode GetBufferHeight(int& value) noexcept;
    [[nodiscard]] StatusCode SetBufferHeight(int value) noexcept;
    [[nodiscard]] StatusCode GetBufferWidth(int& value) noexcept;
    [[nodiscard]] StatusCode SetBufferWidth(int value) noexcept;
    [[nodiscard]] StatusCode GetCapsLock(bool& value) noexcept;
    [[nodiscard]] StatusCode GetCursorLeft(int& value) noexcept;
    [[nodiscard]] StatusCode SetCursorLeft(int value) noexcept;
    [[nodiscard]] StatusCode GetCursorSize(int& value) noexcept;
    [[nodiscard]] StatusCode SetCursorSize(int value) noexcept;
    [[nodiscard]] StatusCode GetCursorTop(int& value) noexcept;
    [[nodiscard]] StatusCode SetCursorTop(int value) noexcept;
    [[nodiscard]] StatusCode GetCursorVisible(bool& value) noexcept;
    [[nodiscard]] StatusCode SetCursorVisible(bool value) noexcept;
    [[nodiscard]] StatusCode GetInputEncoding(EncodingCode& encoding) noexcept;
    [[nodiscard]] StatusCode SetInputEncoding(EncodingCode encoding) noexcept;
    [[nodiscard]] StatusCode GetOutputEncoding(EncodingCode& encoding) noexcept;
    [[nodiscard]] StatusCode SetOutputEncoding(EncodingCode encoding) noexcept;
    [[nodiscard]] inline StatusCode GetInputEncoding(Encoding& encoding) noexcept
    {
        EncodingCode raw = 0;
        const StatusCode status = GetInputEncoding(raw);
        encoding = static_cast<Encoding>(raw);
        return status;
    }

    [[nodiscard]] inline StatusCode SetInputEncoding(const Encoding encoding) noexcept
    {
        return SetInputEncoding(static_cast<EncodingCode>(encoding));
    }

    [[nodiscard]] inline StatusCode GetOutputEncoding(Encoding& encoding) noexcept
    {
        EncodingCode raw = 0;
        const StatusCode status = GetOutputEncoding(raw);
        encoding = static_cast<Encoding>(raw);
        return status;
    }

    [[nodiscard]] inline StatusCode SetOutputEncoding(const Encoding encoding) noexcept
    {
        return SetOutputEncoding(static_cast<EncodingCode>(encoding));
    }

    [[nodiscard]] StatusCode GetIsErrorRedirected(bool& value) noexcept;
    [[nodiscard]] StatusCode GetIsInputRedirected(bool& value) noexcept;
    [[nodiscard]] StatusCode GetIsOutputRedirected(bool& value) noexcept;
    [[nodiscard]] StatusCode GetKeyAvailable(bool& value) noexcept;
    [[nodiscard]] StatusCode GetLargestWindowHeight(int& value) noexcept;
    [[nodiscard]] StatusCode GetLargestWindowWidth(int& value) noexcept;
    [[nodiscard]] StatusCode GetNumberLock(bool& value) noexcept;
    [[nodiscard]] StatusCode GetNewLine(std::string& value);
    [[nodiscard]] StatusCode SetNewLine(std::string_view value);
    [[nodiscard]] StatusCode GetTitle(std::string& value);
    [[nodiscard]] StatusCode SetTitle(std::string_view value);
    [[nodiscard]] StatusCode GetTreatControlCAsInput(bool& value) noexcept;
    [[nodiscard]] StatusCode SetTreatControlCAsInput(bool value) noexcept;
    [[nodiscard]] StatusCode GetWindowHeight(int& value) noexcept;
    [[nodiscard]] StatusCode SetWindowHeight(int value) noexcept;
    [[nodiscard]] StatusCode GetWindowLeft(int& value) noexcept;
    [[nodiscard]] StatusCode SetWindowLeft(int value) noexcept;
    [[nodiscard]] StatusCode GetWindowTop(int& value) noexcept;
    [[nodiscard]] StatusCode SetWindowTop(int value) noexcept;
    [[nodiscard]] StatusCode GetWindowWidth(int& value) noexcept;
    [[nodiscard]] StatusCode SetWindowWidth(int value) noexcept;

    [[nodiscard]] StatusCode Beep() noexcept;
    [[nodiscard]] StatusCode Beep(int frequency, int durationMs) noexcept;
    [[nodiscard]] StatusCode Clear() noexcept;
    [[nodiscard]] StatusCode GetCursorPosition(int& left, int& top) noexcept;
    [[nodiscard]] StatusCode MoveBufferArea(
        int sourceLeft,
        int sourceTop,
        int sourceWidth,
        int sourceHeight,
        int targetLeft,
        int targetTop) noexcept;
    [[nodiscard]] StatusCode MoveBufferArea(
        int sourceLeft,
        int sourceTop,
        int sourceWidth,
        int sourceHeight,
        int targetLeft,
        int targetTop,
        std::int32_t sourceChar,
        ColorCode sourceForeColor,
        ColorCode sourceBackColor) noexcept;
    [[nodiscard]] inline StatusCode MoveBufferArea(
        const int sourceLeft,
        const int sourceTop,
        const int sourceWidth,
        const int sourceHeight,
        const int targetLeft,
        const int targetTop,
        const std::int32_t sourceChar,
        const Color sourceForeColor,
        const Color sourceBackColor) noexcept
    {
        return MoveBufferArea(
            sourceLeft,
            sourceTop,
            sourceWidth,
            sourceHeight,
            targetLeft,
            targetTop,
            sourceChar,
            static_cast<ColorCode>(sourceForeColor),
            static_cast<ColorCode>(sourceBackColor)
        );
    }

    [[nodiscard]] StatusCode OpenStandardError(
        std::size_t bufferSize,
        bool& redirected,
        std::size_t& actualBufferSize,
        bool& hasReader,
        bool& hasWriter,
        bool& isOpen) noexcept;
    [[nodiscard]] StatusCode OpenStandardInput(
        std::size_t bufferSize,
        bool& redirected,
        std::size_t& actualBufferSize,
        bool& hasReader,
        bool& hasWriter,
        bool& isOpen) noexcept;
    [[nodiscard]] StatusCode OpenStandardOutput(
        std::size_t bufferSize,
        bool& redirected,
        std::size_t& actualBufferSize,
        bool& hasReader,
        bool& hasWriter,
        bool& isOpen) noexcept;

    [[nodiscard]] StatusCode Read(int& value) noexcept;
    [[nodiscard]] StatusCode ReadCount(std::size_t count, std::string& value);
    [[nodiscard]] StatusCode ReadHidden(int& value) noexcept;
    [[nodiscard]] StatusCode ReadKey(std::int32_t& keyChar, KeyCode& key, ModifierMask& modifiers);
    [[nodiscard]] StatusCode ReadKey(bool intercept, std::int32_t& keyChar, KeyCode& key, ModifierMask& modifiers);
    [[nodiscard]] inline StatusCode ReadKey(std::int32_t& keyChar, Key& key, Modifiers& modifiers)
    {
        KeyCode rawKey = 0;
        ModifierMask rawModifiers = 0;
        const StatusCode status = ReadKey(keyChar, rawKey, rawModifiers);
        key = static_cast<Key>(rawKey);
        modifiers = static_cast<Modifiers>(rawModifiers);
        return status;
    }

    [[nodiscard]] inline StatusCode ReadKey(const bool intercept, std::int32_t& keyChar, Key& key, Modifiers& modifiers)
    {
        KeyCode rawKey = 0;
        ModifierMask rawModifiers = 0;
        const StatusCode status = ReadKey(intercept, keyChar, rawKey, rawModifiers);
        key = static_cast<Key>(rawKey);
        modifiers = static_cast<Modifiers>(rawModifiers);
        return status;
    }

    [[nodiscard]] StatusCode ReadLine(std::string& value);
    [[nodiscard]] StatusCode ReadUntil(char delimiter, bool includeDelimiter, std::string& value);
    [[nodiscard]] StatusCode ReadWord(std::string& value);

    [[nodiscard]] StatusCode ResetColor() noexcept;
    [[nodiscard]] StatusCode SetBufferSize(int width, int height) noexcept;
    [[nodiscard]] StatusCode SetCursorPosition(int left, int top) noexcept;
    [[nodiscard]] StatusCode SetWindowPosition(int left, int top) noexcept;
    [[nodiscard]] StatusCode SetWindowSize(int width, int height) noexcept;
    [[nodiscard]] StatusCode FlushError() noexcept;
    [[nodiscard]] StatusCode FlushOut() noexcept;

    namespace detail
    {
        [[nodiscard]] inline StatusCode NullArgumentStatus() noexcept
        {
            return static_cast<StatusCode>(console::ConsoleStatus::NullArgument);
        }

        template <typename... Ptrs>
        [[nodiscard]] inline bool EnsureOutParams(Ptrs*... ptrs) noexcept
        {
            if (((ptrs != nullptr) && ...))
                return true;

            StoreLastError(console::MakeConsoleError(console::ConsoleStatus::NullArgument));
            return false;
        }
    }

    [[nodiscard]] inline StatusCode Capabilities(
        bool* colors,
        bool* cursorPosition,
        bool* cursorVisibility,
        bool* cursorSize,
        bool* bufferSize,
        bool* windowPosition,
        bool* windowSizeGet,
        bool* windowSizeSet,
        bool* largestWindowSize,
        bool* moveBufferArea,
        bool* keyAvailable,
        bool* readKey,
        bool* title,
        bool* beep,
        bool* beepFrequency,
        bool* keyboardToggleState,
        bool* treatControlCAsInput,
        bool* encodingSet) noexcept
    {
        if (!detail::EnsureOutParams(
                colors,
                cursorPosition,
                cursorVisibility,
                cursorSize,
                bufferSize,
                windowPosition,
                windowSizeGet,
                windowSizeSet,
                largestWindowSize,
                moveBufferArea,
                keyAvailable,
                readKey,
                title,
                beep,
                beepFrequency,
                keyboardToggleState,
                treatControlCAsInput,
                encodingSet))
        {
            return detail::NullArgumentStatus();
        }

        return Capabilities(
            *colors,
            *cursorPosition,
            *cursorVisibility,
            *cursorSize,
            *bufferSize,
            *windowPosition,
            *windowSizeGet,
            *windowSizeSet,
            *largestWindowSize,
            *moveBufferArea,
            *keyAvailable,
            *readKey,
            *title,
            *beep,
            *beepFrequency,
            *keyboardToggleState,
            *treatControlCAsInput,
            *encodingSet);
    }

    [[nodiscard]] inline StatusCode GetBackgroundColor(ColorCode* color) noexcept
    {
        return detail::EnsureOutParams(color) ? GetBackgroundColor(*color) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetForegroundColor(ColorCode* color) noexcept
    {
        return detail::EnsureOutParams(color) ? GetForegroundColor(*color) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetBufferHeight(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetBufferHeight(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetBufferWidth(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetBufferWidth(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetCapsLock(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetCapsLock(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetCursorLeft(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetCursorLeft(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetCursorSize(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetCursorSize(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetCursorTop(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetCursorTop(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetCursorVisible(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetCursorVisible(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetInputEncoding(EncodingCode* encoding) noexcept
    {
        return detail::EnsureOutParams(encoding) ? GetInputEncoding(*encoding) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetOutputEncoding(EncodingCode* encoding) noexcept
    {
        return detail::EnsureOutParams(encoding) ? GetOutputEncoding(*encoding) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetIsErrorRedirected(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetIsErrorRedirected(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetIsInputRedirected(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetIsInputRedirected(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetIsOutputRedirected(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetIsOutputRedirected(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetKeyAvailable(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetKeyAvailable(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetLargestWindowHeight(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetLargestWindowHeight(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetLargestWindowWidth(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetLargestWindowWidth(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetNumberLock(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetNumberLock(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetNewLine(std::string* value)
    {
        return detail::EnsureOutParams(value) ? GetNewLine(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetTitle(std::string* value)
    {
        return detail::EnsureOutParams(value) ? GetTitle(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetTreatControlCAsInput(bool* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetTreatControlCAsInput(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetWindowHeight(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetWindowHeight(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetWindowLeft(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetWindowLeft(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetWindowTop(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetWindowTop(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetWindowWidth(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? GetWindowWidth(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode GetCursorPosition(int* left, int* top) noexcept
    {
        return detail::EnsureOutParams(left, top) ? GetCursorPosition(*left, *top) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode OpenStandardError(
        std::size_t bufferSize,
        bool* redirected,
        std::size_t* actualBufferSize,
        bool* hasReader,
        bool* hasWriter,
        bool* isOpen) noexcept
    {
        return detail::EnsureOutParams(redirected, actualBufferSize, hasReader, hasWriter, isOpen)
            ? OpenStandardError(bufferSize, *redirected, *actualBufferSize, *hasReader, *hasWriter, *isOpen)
            : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode OpenStandardInput(
        std::size_t bufferSize,
        bool* redirected,
        std::size_t* actualBufferSize,
        bool* hasReader,
        bool* hasWriter,
        bool* isOpen) noexcept
    {
        return detail::EnsureOutParams(redirected, actualBufferSize, hasReader, hasWriter, isOpen)
            ? OpenStandardInput(bufferSize, *redirected, *actualBufferSize, *hasReader, *hasWriter, *isOpen)
            : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode OpenStandardOutput(
        std::size_t bufferSize,
        bool* redirected,
        std::size_t* actualBufferSize,
        bool* hasReader,
        bool* hasWriter,
        bool* isOpen) noexcept
    {
        return detail::EnsureOutParams(redirected, actualBufferSize, hasReader, hasWriter, isOpen)
            ? OpenStandardOutput(bufferSize, *redirected, *actualBufferSize, *hasReader, *hasWriter, *isOpen)
            : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode Read(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? Read(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadCount(std::size_t count, std::string* value)
    {
        return detail::EnsureOutParams(value) ? ReadCount(count, *value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadHidden(int* value) noexcept
    {
        return detail::EnsureOutParams(value) ? ReadHidden(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadKey(std::int32_t* keyChar, KeyCode* key, ModifierMask* modifiers)
    {
        return detail::EnsureOutParams(keyChar, key, modifiers)
            ? ReadKey(*keyChar, *key, *modifiers)
            : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadKey(bool intercept, std::int32_t* keyChar, KeyCode* key, ModifierMask* modifiers)
    {
        return detail::EnsureOutParams(keyChar, key, modifiers)
            ? ReadKey(intercept, *keyChar, *key, *modifiers)
            : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadLine(std::string* value)
    {
        return detail::EnsureOutParams(value) ? ReadLine(*value) : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadUntil(char delimiter, bool includeDelimiter, std::string* value)
    {
        return detail::EnsureOutParams(value)
            ? ReadUntil(delimiter, includeDelimiter, *value)
            : detail::NullArgumentStatus();
    }

    [[nodiscard]] inline StatusCode ReadWord(std::string* value)
    {
        return detail::EnsureOutParams(value) ? ReadWord(*value) : detail::NullArgumentStatus();
    }
}
