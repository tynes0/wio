#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace wio::sdk
{
    namespace values_detail
    {
        [[nodiscard]] inline bool decodeUtf8CodePoint(
            const std::string_view input,
            std::size_t& offset,
            std::uint32_t& codePoint) noexcept
        {
            if (offset >= input.size())
                return false;

            const auto first = static_cast<std::uint8_t>(input[offset]);
            std::size_t width = 0u;
            std::uint32_t value = 0u;
            std::uint32_t minimum = 0u;

            if (first <= 0x7fu)
            {
                width = 1u;
                value = first;
            }
            else if ((first & 0xe0u) == 0xc0u)
            {
                width = 2u;
                value = first & 0x1fu;
                minimum = 0x80u;
            }
            else if ((first & 0xf0u) == 0xe0u)
            {
                width = 3u;
                value = first & 0x0fu;
                minimum = 0x800u;
            }
            else if ((first & 0xf8u) == 0xf0u)
            {
                width = 4u;
                value = first & 0x07u;
                minimum = 0x10000u;
            }
            else
            {
                return false;
            }

            if (offset + width > input.size())
                return false;

            for (std::size_t index = 1u; index < width; ++index)
            {
                const auto continuation = static_cast<std::uint8_t>(input[offset + index]);
                if ((continuation & 0xc0u) != 0x80u)
                    return false;
                value = (value << 6u) | (continuation & 0x3fu);
            }

            if (value < minimum || value > 0x10ffffu || (value >= 0xd800u && value <= 0xdfffu))
                return false;

            offset += width;
            codePoint = value;
            return true;
        }

        [[nodiscard]] inline bool isValidUtf8(const std::string_view input) noexcept
        {
            std::size_t offset = 0u;
            while (offset < input.size())
            {
                std::uint32_t ignored = 0u;
                if (!decodeUtf8CodePoint(input, offset, ignored))
                    return false;
            }
            return true;
        }

        [[nodiscard]] inline std::vector<std::size_t> utf8Offsets(const std::string_view input)
        {
            std::vector<std::size_t> offsets;
            offsets.reserve(input.size() + 1u);
            std::size_t offset = 0u;
            offsets.push_back(0u);
            while (offset < input.size())
            {
                std::uint32_t ignored = 0u;
                if (!decodeUtf8CodePoint(input, offset, ignored))
                    throw std::invalid_argument("Wio SDK text contains invalid UTF-8.");
                offsets.push_back(offset);
            }
            return offsets;
        }
    }

    // Host-side counterpart of Wio's immutable, Unicode-semantic `text` value.
    // Storage is owned validated UTF-8; indexing and slicing use code points.
    class WioText
    {
    public:
        WioText() = default;

        [[nodiscard]] static WioText from_utf8(std::string value)
        {
            if (!values_detail::isValidUtf8(value))
                throw std::invalid_argument("Cannot construct Wio text from invalid UTF-8.");
            return WioText(std::move(value), ValidatedTag{});
        }

        [[nodiscard]] static std::optional<WioText> try_from_utf8(std::string value) noexcept
        {
            if (!values_detail::isValidUtf8(value))
                return std::nullopt;
            return WioText(std::move(value), ValidatedTag{});
        }

        [[nodiscard]] const std::string& utf8() const noexcept { return value_; }
        [[nodiscard]] std::string_view view() const noexcept { return value_; }
        [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
        [[nodiscard]] std::size_t byte_count() const noexcept { return value_.size(); }

        [[nodiscard]] std::size_t code_point_count() const
        {
            const auto offsets = values_detail::utf8Offsets(value_);
            return offsets.empty() ? 0u : offsets.size() - 1u;
        }

        [[nodiscard]] std::vector<std::uint32_t> code_points() const
        {
            std::vector<std::uint32_t> result;
            std::size_t offset = 0u;
            while (offset < value_.size())
            {
                std::uint32_t codePoint = 0u;
                if (!values_detail::decodeUtf8CodePoint(value_, offset, codePoint))
                    throw std::logic_error("Wio SDK text invariant was violated.");
                result.push_back(codePoint);
            }
            return result;
        }

        [[nodiscard]] WioText at(const std::size_t index) const
        {
            return slice(index, 1u);
        }

        [[nodiscard]] WioText slice(const std::size_t start) const
        {
            const auto count = code_point_count();
            return start >= count ? WioText{} : slice(start, count - start);
        }

        [[nodiscard]] WioText slice(const std::size_t start, const std::size_t count) const
        {
            const auto offsets = values_detail::utf8Offsets(value_);
            const std::size_t codePointCount = offsets.empty() ? 0u : offsets.size() - 1u;
            if (start >= codePointCount || count == 0u)
                return {};
            const std::size_t end = start + std::min(count, codePointCount - start);
            return WioText(value_.substr(offsets[start], offsets[end] - offsets[start]), ValidatedTag{});
        }

        [[nodiscard]] bool contains(const WioText& other) const noexcept
        {
            return value_.find(other.value_) != std::string::npos;
        }

        [[nodiscard]] bool starts_with(const WioText& other) const noexcept
        {
            return value_.starts_with(other.value_);
        }

        [[nodiscard]] bool ends_with(const WioText& other) const noexcept
        {
            return value_.ends_with(other.value_);
        }

        WioText& operator+=(const WioText& other)
        {
            value_ += other.value_;
            return *this;
        }

        [[nodiscard]] friend WioText operator+(WioText left, const WioText& right)
        {
            left += right;
            return left;
        }

        [[nodiscard]] friend bool operator==(const WioText&, const WioText&) = default;

    private:
        struct ValidatedTag {};
        explicit WioText(std::string value, ValidatedTag) : value_(std::move(value)) {}
        std::string value_{};
    };

    template <typename T>
    class WioOption
    {
    public:
        WioOption() = default;
        WioOption(std::nullopt_t) noexcept {}
        WioOption(T value) : value_(std::move(value)) {}

        [[nodiscard]] static WioOption some(T value) { return WioOption(std::move(value)); }
        [[nodiscard]] static WioOption none() noexcept { return {}; }
        [[nodiscard]] bool is_some() const noexcept { return value_.has_value(); }
        [[nodiscard]] bool is_none() const noexcept { return !value_.has_value(); }
        [[nodiscard]] explicit operator bool() const noexcept { return is_some(); }
        [[nodiscard]] T& value() & { return value_.value(); }
        [[nodiscard]] const T& value() const & { return value_.value(); }
        [[nodiscard]] T& unwrap() & { return value(); }
        [[nodiscard]] const T& unwrap() const & { return value(); }
        [[nodiscard]] T value_or(T fallback) const { return value_.value_or(std::move(fallback)); }
        [[nodiscard]] T take() && { return std::move(value_).value(); }
        [[nodiscard]] const std::optional<T>& raw() const noexcept { return value_; }

        template <typename F>
        [[nodiscard]] auto map(F&& transform) const -> WioOption<std::decay_t<std::invoke_result_t<F, const T&>>>
        {
            using Result = std::decay_t<std::invoke_result_t<F, const T&>>;
            if (is_none())
                return WioOption<Result>::none();
            return WioOption<Result>::some(std::invoke(std::forward<F>(transform), *value_));
        }

        template <typename F>
        [[nodiscard]] WioOption filter(F&& predicate) const
        {
            return is_some() && std::invoke(std::forward<F>(predicate), *value_) ? *this : none();
        }

        template <typename F>
        [[nodiscard]] auto and_then(F&& transform) const -> std::decay_t<std::invoke_result_t<F, const T&>>
        {
            using Result = std::decay_t<std::invoke_result_t<F, const T&>>;
            return is_some() ? std::invoke(std::forward<F>(transform), *value_) : Result::none();
        }

        template <typename F>
        [[nodiscard]] WioOption or_else(F&& fallback) const
        {
            return is_some() ? *this : std::invoke(std::forward<F>(fallback));
        }

        template <typename F>
        [[nodiscard]] WioOption inspect(F&& action) const
        {
            if (is_some())
                std::invoke(std::forward<F>(action), *value_);
            return *this;
        }

        [[nodiscard]] std::vector<T> to_array() const
        {
            return is_some() ? std::vector<T>{ *value_ } : std::vector<T>{};
        }

        template <typename F>
        void for_each(F&& action) const
        {
            if (is_some())
                std::invoke(std::forward<F>(action), *value_);
        }

        template <typename U>
        [[nodiscard]] WioOption<std::pair<T, U>> zip(const WioOption<U>& other) const
        {
            if (is_none() || other.is_none())
                return WioOption<std::pair<T, U>>::none();
            return WioOption<std::pair<T, U>>::some({ value(), other.value() });
        }

    private:
        std::optional<T> value_{};
    };

    enum class WioResultDomain : std::int32_t
    {
        None = 0,
        Console = 1,
        Io = 2,
        FileSystem = 3,
        Runtime = 4,
        Custom = 5
    };

    struct WioResultError
    {
        WioResultDomain domain = WioResultDomain::None;
        std::int32_t code = 0;
        std::int64_t native_code = 0;
        std::string message{};
    };

    struct WioUnit
    {
        [[nodiscard]] friend constexpr bool operator==(WioUnit, WioUnit) noexcept = default;
    };

    template <typename T>
    class WioResult
    {
    public:
        [[nodiscard]] static WioResult ok(T value) { return WioResult(std::move(value)); }
        [[nodiscard]] static WioResult error(WioResultError value) { return WioResult(std::move(value)); }
        [[nodiscard]] bool is_ok() const noexcept { return std::holds_alternative<T>(value_); }
        [[nodiscard]] bool is_error() const noexcept { return !is_ok(); }
        [[nodiscard]] bool has_value() const noexcept { return is_ok(); }
        [[nodiscard]] explicit operator bool() const noexcept { return is_ok(); }
        [[nodiscard]] T& value() & { return std::get<T>(value_); }
        [[nodiscard]] const T& value() const & { return std::get<T>(value_); }
        [[nodiscard]] T& unwrap() & { return value(); }
        [[nodiscard]] const T& unwrap() const & { return value(); }
        [[nodiscard]] const WioResultError& error_value() const & { return std::get<WioResultError>(value_); }
        [[nodiscard]] T value_or(T fallback) const { return is_ok() ? std::get<T>(value_) : std::move(fallback); }
        [[nodiscard]] T take_value() && { return std::get<T>(std::move(value_)); }

        template <typename F>
        [[nodiscard]] auto map(F&& transform) const -> WioResult<std::decay_t<std::invoke_result_t<F, const T&>>>
        {
            using Result = std::decay_t<std::invoke_result_t<F, const T&>>;
            if (is_error())
                return WioResult<Result>::error(error_value());
            return WioResult<Result>::ok(std::invoke(std::forward<F>(transform), value()));
        }

        template <typename F>
        [[nodiscard]] auto and_then(F&& transform) const -> std::decay_t<std::invoke_result_t<F, const T&>>
        {
            using Result = std::decay_t<std::invoke_result_t<F, const T&>>;
            if (is_error())
                return Result::error(error_value());
            return std::invoke(std::forward<F>(transform), value());
        }

        template <typename F>
        [[nodiscard]] WioResult map_error(F&& transform) const
        {
            return is_ok() ? *this : error(std::invoke(std::forward<F>(transform), error_value()));
        }

        template <typename F>
        [[nodiscard]] WioResult or_else(F&& fallback) const
        {
            return is_ok() ? *this : std::invoke(std::forward<F>(fallback), error_value());
        }

        template <typename F>
        [[nodiscard]] WioResult inspect(F&& action) const
        {
            if (is_ok())
                std::invoke(std::forward<F>(action), value());
            return *this;
        }

        template <typename F>
        [[nodiscard]] WioResult inspect_error(F&& action) const
        {
            if (is_error())
                std::invoke(std::forward<F>(action), error_value());
            return *this;
        }

        [[nodiscard]] WioOption<T> to_option() const
        {
            return is_ok() ? WioOption<T>::some(value()) : WioOption<T>::none();
        }

    private:
        explicit WioResult(T value) : value_(std::move(value)) {}
        explicit WioResult(WioResultError value) : value_(std::move(value)) {}
        std::variant<T, WioResultError> value_;
    };

    using WioUnitResult = WioResult<WioUnit>;

    template <typename T>
    using WioNullable = std::optional<T>;

    template <typename... T>
    using WioTuple = std::tuple<T...>;

    template <typename T>
    class WioQueue
    {
    public:
        WioQueue() = default;
        WioQueue(std::initializer_list<T> values) : values_(values) {}
        [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
        [[nodiscard]] std::size_t count() const noexcept { return values_.size(); }
        void push(T value) { values_.push_back(std::move(value)); }
        void enqueue(T value) { push(std::move(value)); }
        [[nodiscard]] T& front() { return peek(); }
        [[nodiscard]] const T& front() const { return peek(); }
        [[nodiscard]] T& back()
        {
            if (empty()) throw std::out_of_range("Wio SDK queue is empty.");
            return values_.back();
        }
        [[nodiscard]] const T& back() const
        {
            if (empty()) throw std::out_of_range("Wio SDK queue is empty.");
            return values_.back();
        }
        [[nodiscard]] WioOption<T> first() const { return empty() ? WioOption<T>::none() : WioOption<T>::some(values_.front()); }
        [[nodiscard]] WioOption<T> last() const { return empty() ? WioOption<T>::none() : WioOption<T>::some(values_.back()); }
        [[nodiscard]] T& peek()
        {
            if (empty()) throw std::out_of_range("Wio SDK queue is empty.");
            return values_.front();
        }
        [[nodiscard]] const T& peek() const
        {
            if (empty()) throw std::out_of_range("Wio SDK queue is empty.");
            return values_.front();
        }
        [[nodiscard]] bool contains(const T& value) const
        {
            return std::find(values_.begin(), values_.end(), value) != values_.end();
        }
        [[nodiscard]] bool remove(const T& value)
        {
            const auto found = std::find(values_.begin(), values_.end(), value);
            if (found == values_.end()) return false;
            values_.erase(found);
            return true;
        }
        [[nodiscard]] T pop()
        {
            if (empty())
                throw std::out_of_range("Wio SDK queue is empty.");
            T value = std::move(values_.front());
            values_.pop_front();
            return value;
        }
        [[nodiscard]] T dequeue() { return pop(); }
        void clear() noexcept { values_.clear(); }
        [[nodiscard]] std::vector<T> to_array() const { return { values_.begin(), values_.end() }; }
        [[nodiscard]] WioQueue clone() const { return *this; }

    private:
        std::deque<T> values_{};
    };

    template <typename T>
    class WioUnorderedSet
    {
    public:
        WioUnorderedSet() = default;
        WioUnorderedSet(std::initializer_list<T> values) : values_(values) {}
        [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
        [[nodiscard]] std::size_t count() const noexcept { return values_.size(); }
        [[nodiscard]] bool add(T value) { return values_.insert(std::move(value)).second; }
        [[nodiscard]] bool insert(T value) { return add(std::move(value)); }
        [[nodiscard]] bool contains(const T& value) const { return values_.contains(value); }
        [[nodiscard]] bool remove(const T& value) { return values_.erase(value) != 0u; }
        void clear() noexcept { values_.clear(); }
        [[nodiscard]] std::vector<T> values() const { return { values_.begin(), values_.end() }; }
        [[nodiscard]] const std::unordered_set<T>& raw() const noexcept { return values_; }

    private:
        std::unordered_set<T> values_{};
    };

    template <typename T>
    class WioOrderedSet
    {
    public:
        WioOrderedSet() = default;
        WioOrderedSet(std::initializer_list<T> values) : values_(values) {}
        [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
        [[nodiscard]] std::size_t count() const noexcept { return values_.size(); }
        [[nodiscard]] bool add(T value) { return values_.insert(std::move(value)).second; }
        [[nodiscard]] bool insert(T value) { return add(std::move(value)); }
        [[nodiscard]] bool contains(const T& value) const { return values_.contains(value); }
        [[nodiscard]] bool remove(const T& value) { return values_.erase(value) != 0u; }
        void clear() noexcept { values_.clear(); }
        [[nodiscard]] const T& first() const
        {
            if (empty()) throw std::out_of_range("Wio SDK ordered set is empty.");
            return *values_.begin();
        }
        [[nodiscard]] const T& last() const
        {
            if (empty()) throw std::out_of_range("Wio SDK ordered set is empty.");
            return *values_.rbegin();
        }
        [[nodiscard]] std::vector<T> values() const { return { values_.begin(), values_.end() }; }
        [[nodiscard]] const std::set<T>& raw() const noexcept { return values_; }

    private:
        std::set<T> values_{};
    };

    class WioSpanRange
    {
    public:
        constexpr WioSpanRange() noexcept = default;
        constexpr WioSpanRange(const std::size_t start, const std::size_t count) noexcept
            : start_(start), count_(count)
        {
        }

        [[nodiscard]] static constexpr WioSpanRange full(const std::size_t count) noexcept
        {
            return WioSpanRange(0u, count);
        }

        [[nodiscard]] constexpr std::size_t start() const noexcept { return start_; }
        [[nodiscard]] constexpr std::size_t count() const noexcept { return count_; }
        [[nodiscard]] constexpr std::size_t end() const noexcept { return start_ + count_; }
        [[nodiscard]] constexpr bool empty() const noexcept { return count_ == 0u; }

        [[nodiscard]] constexpr WioSpanRange clamped(const std::size_t sourceCount) const noexcept
        {
            if (start_ >= sourceCount)
                return WioSpanRange(sourceCount, 0u);
            return WioSpanRange(start_, std::min(count_, sourceCount - start_));
        }

        [[nodiscard]] constexpr WioSpanRange slice(const std::size_t start,
                                                   const std::size_t count) const noexcept
        {
            if (start >= count_)
                return WioSpanRange(end(), 0u);
            return WioSpanRange(start_ + start, std::min(count, count_ - start));
        }

        [[nodiscard]] friend constexpr bool operator==(const WioSpanRange&, const WioSpanRange&) noexcept = default;

    private:
        std::size_t start_ = 0u;
        std::size_t count_ = 0u;
    };

    template <typename T>
    class WioSpan
    {
    public:
        WioSpan() = default;
        explicit WioSpan(std::span<T> values) noexcept : values_(values) {}
        WioSpan(std::span<T> values, const WioSpanRange range) noexcept
            : values_(apply_range(values, range))
        {
        }
        WioSpan(T* data, const std::size_t count) noexcept : values_(data, count) {}
        WioSpan(T* data, const std::size_t count, const WioSpanRange range) noexcept
            : WioSpan(std::span<T>(data, count), range)
        {
        }
        template <std::size_t N>
        WioSpan(T (&values)[N]) noexcept : values_(values) {}
        template <std::size_t N>
        WioSpan(std::array<std::remove_const_t<T>, N>& values) noexcept : values_(values) {}
        [[nodiscard]] bool empty() const noexcept { return values_.empty(); }
        [[nodiscard]] std::size_t count() const noexcept { return values_.size(); }
        [[nodiscard]] T* data() const noexcept { return values_.data(); }
        [[nodiscard]] T& operator[](const std::size_t index) const { return values_[index]; }
        [[nodiscard]] T& at(const std::size_t index) const
        {
            if (index >= values_.size())
                throw std::out_of_range("Wio SDK span index is out of range.");
            return values_[index];
        }
        [[nodiscard]] WioSpan slice(const std::size_t start, const std::size_t count) const noexcept
        {
            if (start >= values_.size())
                return {};
            return WioSpan(values_.subspan(start, std::min(count, values_.size() - start)));
        }
        [[nodiscard]] WioSpan slice(const WioSpanRange range) const noexcept
        {
            return WioSpan(values_, range);
        }
        [[nodiscard]] WioOption<std::remove_const_t<T>> first() const
        {
            return empty() ? WioOption<std::remove_const_t<T>>::none()
                           : WioOption<std::remove_const_t<T>>::some(values_.front());
        }
        [[nodiscard]] WioOption<std::remove_const_t<T>> last() const
        {
            return empty() ? WioOption<std::remove_const_t<T>>::none()
                           : WioOption<std::remove_const_t<T>>::some(values_.back());
        }
        [[nodiscard]] std::span<T> raw() const noexcept { return values_; }

    private:
        [[nodiscard]] static std::span<T> apply_range(std::span<T> values,
                                                      const WioSpanRange range) noexcept
        {
            const auto safeRange = range.clamped(values.size());
            return values.subspan(safeRange.start(), safeRange.count());
        }

        std::span<T> values_{};
    };

    class WioByteBuffer
    {
    public:
        WioByteBuffer() = default;
        explicit WioByteBuffer(const std::size_t capacity) { data_.reserve(capacity); }
        explicit WioByteBuffer(std::vector<std::byte> values) : data_(std::move(values)) {}
        [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
        [[nodiscard]] std::size_t count() const noexcept { return data_.size(); }
        [[nodiscard]] std::size_t capacity() const noexcept { return data_.capacity(); }
        [[nodiscard]] std::size_t position() const noexcept { return position_; }
        [[nodiscard]] std::size_t remaining() const noexcept { return position_ >= data_.size() ? 0u : data_.size() - position_; }
        void reserve(const std::size_t capacity) { data_.reserve(capacity); }
        void rewind() noexcept { position_ = 0u; }
        [[nodiscard]] bool seek(const std::size_t position) noexcept
        {
            if (position > data_.size())
                return false;
            position_ = position;
            return true;
        }
        void clear() noexcept { data_.clear(); position_ = 0u; }
        void write(const std::byte value)
        {
            if (position_ < data_.size()) data_[position_] = value;
            else data_.push_back(value);
            ++position_;
        }
        void write(const std::span<const std::byte> values) { for (const auto value : values) write(value); }
        void write_u16_le(const std::uint16_t value) { write_integer_le(value); }
        void write_u32_le(const std::uint32_t value) { write_integer_le(value); }
        void write_u64_le(const std::uint64_t value) { write_integer_le(value); }
        [[nodiscard]] WioOption<std::byte> try_read() noexcept
        {
            if (position_ >= data_.size()) return WioOption<std::byte>::none();
            return WioOption<std::byte>::some(data_[position_++]);
        }
        [[nodiscard]] std::byte read()
        {
            auto value = try_read();
            if (value.is_none()) throw std::out_of_range("Wio SDK byte buffer reached its end.");
            return std::move(value).take();
        }
        [[nodiscard]] WioOption<std::uint16_t> try_read_u16_le() noexcept { return try_read_integer_le<std::uint16_t>(); }
        [[nodiscard]] WioOption<std::uint32_t> try_read_u32_le() noexcept { return try_read_integer_le<std::uint32_t>(); }
        [[nodiscard]] WioOption<std::uint64_t> try_read_u64_le() noexcept { return try_read_integer_le<std::uint64_t>(); }
        [[nodiscard]] std::byte at(const std::size_t index) const
        {
            if (index >= data_.size()) throw std::out_of_range("Wio SDK byte buffer index is out of range.");
            return data_[index];
        }
        [[nodiscard]] WioOption<std::byte> get(const std::size_t index) const noexcept
        {
            return index < data_.size() ? WioOption<std::byte>::some(data_[index]) : WioOption<std::byte>::none();
        }
        [[nodiscard]] bool set(const std::size_t index, const std::byte value) noexcept
        {
            if (index >= data_.size()) return false;
            data_[index] = value;
            return true;
        }
        [[nodiscard]] std::vector<std::byte> slice(const std::size_t start, const std::size_t count) const
        {
            if (start >= data_.size()) return {};
            const std::size_t actualCount = std::min(count, data_.size() - start);
            return { data_.begin() + static_cast<std::ptrdiff_t>(start),
                     data_.begin() + static_cast<std::ptrdiff_t>(start + actualCount) };
        }
        [[nodiscard]] WioByteBuffer clone() const { return *this; }
        [[nodiscard]] const std::vector<std::byte>& data() const noexcept { return data_; }
        [[nodiscard]] std::vector<std::byte> take() && noexcept { return std::move(data_); }

    private:
        template <typename T>
        void write_integer_le(T value)
        {
            for (std::size_t index = 0u; index < sizeof(T); ++index)
                write(static_cast<std::byte>((value >> (index * 8u)) & static_cast<T>(0xffu)));
        }

        template <typename T>
        [[nodiscard]] WioOption<T> try_read_integer_le() noexcept
        {
            if (remaining() < sizeof(T)) return WioOption<T>::none();
            T value = 0;
            for (std::size_t index = 0u; index < sizeof(T); ++index)
                value |= static_cast<T>(std::to_integer<std::uint8_t>(data_[position_++])) << (index * 8u);
            return WioOption<T>::some(value);
        }

        std::vector<std::byte> data_{};
        std::size_t position_ = 0u;
    };

    struct WioPoolHandle
    {
        std::size_t index = 0u;
        std::uint32_t generation = 0u;
        [[nodiscard]] bool valid() const noexcept { return generation != 0u; }
        [[nodiscard]] friend bool operator==(const WioPoolHandle&, const WioPoolHandle&) = default;
    };

    class WioBytePool
    {
    public:
        explicit WioBytePool(const std::size_t blockCapacity) : blockCapacity_(blockCapacity) {}
        [[nodiscard]] std::size_t count() const noexcept { return active_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
        [[nodiscard]] std::size_t block_capacity() const noexcept { return blockCapacity_; }
        [[nodiscard]] WioPoolHandle rent()
        {
            std::size_t index = 0u;
            if (!free_.empty())
            {
                index = free_.back();
                free_.pop_back();
                slots_[index].used = true;
                slots_[index].buffer.clear();
            }
            else
            {
                index = slots_.size();
                slots_.push_back(Slot{ WioByteBuffer(blockCapacity_), 1u, true });
            }
            ++active_;
            return { index, slots_[index].generation };
        }
        [[nodiscard]] bool owns(const WioPoolHandle handle) const noexcept
        {
            return handle.index < slots_.size() && slots_[handle.index].used &&
                slots_[handle.index].generation == handle.generation;
        }
        [[nodiscard]] WioByteBuffer& at(const WioPoolHandle handle)
        {
            if (!owns(handle)) throw std::invalid_argument("Wio SDK byte pool received a stale or foreign handle.");
            return slots_[handle.index].buffer;
        }
        [[nodiscard]] const WioByteBuffer& at(const WioPoolHandle handle) const
        {
            if (!owns(handle)) throw std::invalid_argument("Wio SDK byte pool received a stale or foreign handle.");
            return slots_[handle.index].buffer;
        }
        [[nodiscard]] bool release(const WioPoolHandle handle) noexcept
        {
            if (!owns(handle)) return false;
            auto& slot = slots_[handle.index];
            slot.buffer.clear();
            slot.used = false;
            if (++slot.generation == 0u) slot.generation = 1u;
            free_.push_back(handle.index);
            --active_;
            return true;
        }
        [[nodiscard]] WioOption<WioByteBuffer> get(const WioPoolHandle handle) const
        {
            return owns(handle) ? WioOption<WioByteBuffer>::some(slots_[handle.index].buffer)
                                : WioOption<WioByteBuffer>::none();
        }

    private:
        struct Slot { WioByteBuffer buffer; std::uint32_t generation; bool used; };
        std::vector<Slot> slots_{};
        std::vector<std::size_t> free_{};
        std::size_t blockCapacity_ = 0u;
        std::size_t active_ = 0u;
    };

    template <typename T>
    class WioPool
    {
    public:
        [[nodiscard]] std::size_t count() const noexcept { return active_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }

        [[nodiscard]] WioPoolHandle rent(T value)
        {
            std::size_t index = 0u;
            if (!free_.empty())
            {
                index = free_.back();
                free_.pop_back();
                slots_[index].value = std::move(value);
                slots_[index].used = true;
            }
            else
            {
                index = slots_.size();
                slots_.push_back(Slot{ std::move(value), 1u, true });
            }
            ++active_;
            return { index, slots_[index].generation };
        }

        [[nodiscard]] bool owns(const WioPoolHandle handle) const noexcept
        {
            return handle.index < slots_.size() && slots_[handle.index].used &&
                slots_[handle.index].generation == handle.generation;
        }

        [[nodiscard]] T& at(const WioPoolHandle handle)
        {
            if (!owns(handle))
                throw std::invalid_argument("Wio SDK pool received a stale or foreign handle.");
            return *slots_[handle.index].value;
        }

        [[nodiscard]] const T& at(const WioPoolHandle handle) const
        {
            if (!owns(handle))
                throw std::invalid_argument("Wio SDK pool received a stale or foreign handle.");
            return *slots_[handle.index].value;
        }

        [[nodiscard]] WioOption<T> get(const WioPoolHandle handle) const
        {
            return owns(handle) ? WioOption<T>::some(*slots_[handle.index].value) : WioOption<T>::none();
        }

        [[nodiscard]] bool set(const WioPoolHandle handle, T value)
        {
            if (!owns(handle))
                return false;
            slots_[handle.index].value = std::move(value);
            return true;
        }

        [[nodiscard]] bool release(const WioPoolHandle handle) noexcept
        {
            if (!owns(handle))
                return false;
            auto& slot = slots_[handle.index];
            slot.value.reset();
            slot.used = false;
            if (++slot.generation == 0u)
                slot.generation = 1u;
            free_.push_back(handle.index);
            --active_;
            return true;
        }

    private:
        struct Slot
        {
            std::optional<T> value;
            std::uint32_t generation;
            bool used;
        };

        std::vector<Slot> slots_{};
        std::vector<std::size_t> free_{};
        std::size_t active_ = 0u;
    };

    template <typename T>
    class WioBox
    {
    public:
        WioBox() = default;
        explicit WioBox(T value) : value_(std::make_unique<T>(std::move(value))) {}
        WioBox(const WioBox& other) : value_(other ? std::make_unique<T>(*other.value_) : nullptr) {}
        WioBox& operator=(const WioBox& other)
        {
            if (this != &other) value_ = other ? std::make_unique<T>(*other.value_) : nullptr;
            return *this;
        }
        WioBox(WioBox&&) noexcept = default;
        WioBox& operator=(WioBox&&) noexcept = default;
        [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }
        [[nodiscard]] T& value()
        {
            if (!value_) throw std::logic_error("Wio SDK Box is empty.");
            return *value_;
        }
        [[nodiscard]] const T& value() const
        {
            if (!value_) throw std::logic_error("Wio SDK Box is empty.");
            return *value_;
        }
        [[nodiscard]] T* get() noexcept { return value_.get(); }
        [[nodiscard]] const T* get() const noexcept { return value_.get(); }

    private:
        std::unique_ptr<T> value_{};
    };

    class WioAny
    {
    public:
        WioAny() = default;
        template <typename T> explicit WioAny(T value) : value_(std::move(value)) {}
        [[nodiscard]] bool empty() const noexcept { return !value_.has_value(); }
        [[nodiscard]] const std::type_info& type() const noexcept { return value_.type(); }
        template <typename T> [[nodiscard]] bool is() const noexcept { return std::any_cast<T>(&value_) != nullptr; }
        template <typename T> [[nodiscard]] T& as() { return std::any_cast<T&>(value_); }
        template <typename T> [[nodiscard]] const T& as() const { return std::any_cast<const T&>(value_); }
        void reset() noexcept { value_.reset(); }

    private:
        std::any value_{};
    };
}

namespace wio
{
    using text = sdk::WioText;
}

template <>
struct std::hash<wio::sdk::WioText>
{
    [[nodiscard]] std::size_t operator()(const wio::sdk::WioText& value) const noexcept
    {
        return std::hash<std::string_view>{}(value.view());
    }
};
