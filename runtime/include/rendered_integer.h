#pragma once

#include "integer_format.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

namespace wio::runtime::detail
{
    // Compiler-private rendered bytes, not a deferred formatter or Wio value.
    // The classic decimal spelling needs at most 20 bytes. Custom facets may
    // produce arbitrary text (including NUL), so retain an owning cold fallback.
    class RenderedInteger final
    {
        static constexpr std::size_t capacity = sizeof(std::string) > 24 ? sizeof(std::string) : 24;
        static constexpr std::uint8_t fallback = 255;
        struct Digits
        {
            // Only [0, length_) is ever read. Do not zero a full scratch array
            // at function entry when the first format will overwrite its bytes.
            Digits() noexcept
            {
            }

            char bytes[capacity];
        };
        union Storage
        {
            Storage() noexcept : digits{}
            {
            }

            ~Storage()
            {
            }

            Digits digits;
            std::string text;
        } storage_;
        std::uint8_t length_ = 0;

    public:
        RenderedInteger() noexcept
        {
        }
        RenderedInteger(const RenderedInteger&) = delete;
        RenderedInteger& operator=(const RenderedInteger&) = delete;

        ~RenderedInteger()
        {
            if (length_ == fallback)
                std::destroy_at(&storage_.text);
        }

        template <class T>
            requires(std::is_integral_v<T> && sizeof(T) <= 8 && !std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
                     !std::is_same_v<T, signed char> && !std::is_same_v<T, unsigned char>)
        void format(const T value)
        {
            // The locale is observed NOW, not when a later interpolation reads
            // these bytes. Keep the old rendered value intact if formatting fails.
            if (std::locale() != std::locale::classic())
            {
                std::string rendered;
                AppendIntegerStream(rendered, value);
                clear();
                std::construct_at(&storage_.text, std::move(rendered));
                length_ = fallback;
                return;
            }

            char buffer[std::numeric_limits<T>::digits10 + 3];
            static_assert(sizeof(buffer) <= capacity);
            const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
            if (converted.ec != std::errc{})
                throw std::logic_error("Wio integer formatting buffer is insufficient");
            const auto length = static_cast<std::uint8_t>(converted.ptr - buffer);
            clear();
            std::memcpy(storage_.digits.bytes, buffer, length);
            length_ = length;
        }

        void clear() noexcept
        {
            if (length_ == fallback)
            {
                std::destroy_at(&storage_.text);
                ::new (static_cast<void*>(std::addressof(storage_.digits))) Digits;
            }

            length_ = 0;
        }

        [[nodiscard]] std::string_view view() const noexcept
        {
            return length_ == fallback ? std::string_view{storage_.text}
                                       : std::string_view{storage_.digits.bytes, length_};
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return view().size();
        }
    };
} // namespace wio::runtime::detail
