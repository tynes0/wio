#pragma once

#include "exception.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wio::intrinsics
{
    namespace detail
    {
        [[noreturn]] inline void throwRuntimeError(const std::string& message)
        {
            throw runtime::RuntimeException(message);
        }

        template <typename TContainer>
        inline void ensureNotEmpty(const TContainer& container, std::string_view operation)
        {
            if (container.empty())
                throwRuntimeError("Intrinsic '" + std::string(operation) + "' requires a non-empty receiver.");
        }

        template <typename TContainer>
        inline void ensureIndexInRange(const TContainer& container, const std::size_t index, std::string_view operation)
        {
            if (index >= container.size())
            {
                throwRuntimeError(
                    "Intrinsic '" + std::string(operation) + "' index " + std::to_string(index) +
                    " is out of range for size " + std::to_string(container.size()) + "."
                );
            }
        }

        inline std::ptrdiff_t toSignedIndex(const std::size_t index)
        {
            return static_cast<std::ptrdiff_t>(index);
        }

        template <typename TContainer>
        inline auto clampSliceStart(const TContainer& container, std::size_t start) -> std::size_t
        {
            return std::min(start, container.size());
        }

        template <typename TContainer>
        inline auto clampSliceCount(const TContainer& container, const std::size_t start, const std::size_t count) -> std::size_t
        {
            const std::size_t clampedStart = clampSliceStart(container, start);
            return std::min(count, container.size() - clampedStart);
        }

        inline char lowerChar(char value)
        {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        }

        inline char upperChar(char value)
        {
            return static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
        }

        inline bool isWhitespace(char value)
        {
            return std::isspace(static_cast<unsigned char>(value)) != 0;
        }

        template <typename TOut, typename TValue>
        inline void assignOutValue(TOut&& outValue, const TValue& value)
        {
            if constexpr (std::is_pointer_v<std::remove_reference_t<TOut>>)
            {
                if (outValue != nullptr)
                    *outValue = value;
            }
            else
            {
                outValue = value;
            }
        }

        inline std::string trimStartCopy(const std::string& value)
        {
            std::size_t start = 0;
            while (start < value.size() && isWhitespace(value[start]))
                ++start;
            return value.substr(start);
        }

        inline std::string trimEndCopy(const std::string& value)
        {
            if (value.empty())
                return value;

            std::size_t end = value.size();
            while (end > 0 && isWhitespace(value[end - 1]))
                --end;
            return value.substr(0, end);
        }

        inline std::string trimCopy(const std::string& value)
        {
            return trimEndCopy(trimStartCopy(value));
        }

        inline std::string replaceAllCopy(std::string value, const std::string& oldValue, const std::string& newValue)
        {
            if (oldValue.empty())
                return value;

            std::size_t position = 0;
            while ((position = value.find(oldValue, position)) != std::string::npos)
            {
                value.replace(position, oldValue.size(), newValue);
                position += newValue.size();
            }

            return value;
        }

        inline std::string replaceFirstCopy(std::string value, const std::string& oldValue, const std::string& newValue)
        {
            if (oldValue.empty())
                return value;

            if (const std::size_t position = value.find(oldValue); position != std::string::npos)
                value.replace(position, oldValue.size(), newValue);

            return value;
        }
    }

    template <typename TContainer>
    inline std::size_t Count(const TContainer& container)
    {
        return container.size();
    }

    template <typename TContainer>
    inline bool Empty(const TContainer& container)
    {
        return container.empty();
    }

    template <typename TContainer>
    inline std::size_t ArrayCapacity(const TContainer& container)
    {
        if constexpr (requires { container.capacity(); })
            return container.capacity();
        else
            return container.size();
    }

    template <typename TContainer, typename TValue>
    inline bool ArrayContains(const TContainer& container, const TValue& value)
    {
        return std::find(container.begin(), container.end(), value) != container.end();
    }

    template <typename TContainer, typename TValue>
    inline std::ptrdiff_t ArrayIndexOf(const TContainer& container, const TValue& value)
    {
        if (const auto it = std::find(container.begin(), container.end(), value); it != container.end())
            return detail::toSignedIndex(static_cast<std::size_t>(std::distance(container.begin(), it)));
        return -1;
    }

    template <typename TContainer, typename TValue>
    inline std::ptrdiff_t ArrayLastIndexOf(const TContainer& container, const TValue& value)
    {
        for (std::size_t index = container.size(); index > 0; --index)
        {
            if (container[index - 1] == value)
                return detail::toSignedIndex(index - 1);
        }
        return -1;
    }

    template <typename TContainer>
    inline auto ArrayFirst(const TContainer& container) -> typename TContainer::value_type
    {
        detail::ensureNotEmpty(container, "First");
        return container.front();
    }

    template <typename TContainer>
    inline auto ArrayLast(const TContainer& container) -> typename TContainer::value_type
    {
        detail::ensureNotEmpty(container, "Last");
        return container.back();
    }

    template <typename TContainer, typename TIndex, typename TFallback>
    inline auto ArrayGetOr(const TContainer& container, const TIndex index, TFallback&& fallback) -> typename TContainer::value_type
    {
        const std::size_t normalizedIndex = static_cast<std::size_t>(index);
        if (normalizedIndex < container.size())
            return container[normalizedIndex];
        return static_cast<typename TContainer::value_type>(std::forward<TFallback>(fallback));
    }

    template <typename TContainer>
    inline TContainer ArrayClone(const TContainer& container)
    {
        return container;
    }

    template <typename TContainer, typename TIndex, typename TCount>
    inline auto ArraySlice(const TContainer& container, const TIndex start, const TCount count)
        -> std::vector<typename TContainer::value_type>
    {
        const std::size_t normalizedStart = detail::clampSliceStart(container, static_cast<std::size_t>(start));
        const std::size_t normalizedCount = detail::clampSliceCount(container, normalizedStart, static_cast<std::size_t>(count));

        return std::vector<typename TContainer::value_type>(
            container.begin() + static_cast<typename TContainer::difference_type>(normalizedStart),
            container.begin() + static_cast<typename TContainer::difference_type>(normalizedStart + normalizedCount)
        );
    }

    template <typename TContainer, typename TCount>
    inline auto ArrayTake(const TContainer& container, const TCount count) -> std::vector<typename TContainer::value_type>
    {
        return ArraySlice(container, 0, static_cast<std::size_t>(count));
    }

    template <typename TContainer, typename TCount>
    inline auto ArraySkip(const TContainer& container, const TCount count) -> std::vector<typename TContainer::value_type>
    {
        const std::size_t normalizedStart = detail::clampSliceStart(container, static_cast<std::size_t>(count));
        return std::vector<typename TContainer::value_type>(
            container.begin() + static_cast<typename TContainer::difference_type>(normalizedStart),
            container.end()
        );
    }

    template <typename TContainer, typename TOther>
    inline auto ArrayConcat(const TContainer& container, const TOther& other) -> std::vector<typename TContainer::value_type>
    {
        std::vector<typename TContainer::value_type> result;
        result.reserve(container.size() + other.size());
        result.insert(result.end(), container.begin(), container.end());
        result.insert(result.end(), other.begin(), other.end());
        return result;
    }

    template <typename TContainer>
    inline TContainer ArrayReversed(TContainer container)
    {
        std::reverse(container.begin(), container.end());
        return container;
    }

    template <typename TContainer>
    inline void ArrayReverse(TContainer& container)
    {
        std::reverse(container.begin(), container.end());
    }

    template <typename TContainer, typename TValue>
    inline void ArrayPush(TContainer& container, TValue&& value)
    {
        container.push_back(std::forward<TValue>(value));
    }

    template <typename TContainer, typename TValue>
    inline void ArrayPushFront(TContainer& container, TValue&& value)
    {
        container.insert(container.begin(), std::forward<TValue>(value));
    }

    template <typename TContainer>
    inline auto ArrayPop(TContainer& container) -> typename TContainer::value_type
    {
        detail::ensureNotEmpty(container, "Pop");
        auto value = std::move(container.back());
        container.pop_back();
        return value;
    }

    template <typename TContainer>
    inline auto ArrayPopFront(TContainer& container) -> typename TContainer::value_type
    {
        detail::ensureNotEmpty(container, "PopFront");
        auto value = std::move(container.front());
        container.erase(container.begin());
        return value;
    }

    template <typename TContainer, typename TIndex, typename TValue>
    inline void ArrayInsert(TContainer& container, const TIndex index, TValue&& value)
    {
        const std::size_t normalizedIndex = static_cast<std::size_t>(index);
        if (normalizedIndex > container.size())
            detail::throwRuntimeError("Intrinsic 'Insert' index is out of range.");

        container.insert(
            container.begin() + static_cast<typename TContainer::difference_type>(normalizedIndex),
            std::forward<TValue>(value)
        );
    }

    template <typename TContainer, typename TIndex>
    inline void ArrayRemoveAt(TContainer& container, const TIndex index)
    {
        const std::size_t normalizedIndex = static_cast<std::size_t>(index);
        detail::ensureIndexInRange(container, normalizedIndex, "RemoveAt");
        container.erase(container.begin() + static_cast<typename TContainer::difference_type>(normalizedIndex));
    }

    template <typename TContainer, typename TValue>
    inline bool ArrayRemove(TContainer& container, const TValue& value)
    {
        if (const auto it = std::find(container.begin(), container.end(), value); it != container.end())
        {
            container.erase(it);
            return true;
        }
        return false;
    }

    template <typename TContainer>
    inline void ArrayClear(TContainer& container)
    {
        container.clear();
    }

    template <typename TContainer, typename TOther>
    inline void ArrayExtend(TContainer& container, const TOther& other)
    {
        container.insert(container.end(), other.begin(), other.end());
    }

    template <typename TContainer, typename TCapacity>
    inline void ArrayReserve(TContainer& container, const TCapacity capacity)
    {
        container.reserve(static_cast<std::size_t>(capacity));
    }

    template <typename TContainer>
    inline void ArrayShrinkToFit(TContainer& container)
    {
        container.shrink_to_fit();
    }

    template <typename TContainer, typename TValue>
    inline void ArrayFill(TContainer& container, const TValue& value)
    {
        std::fill(container.begin(), container.end(), value);
    }

    template <typename TContainer>
    inline void ArraySort(TContainer& container)
    {
        std::sort(container.begin(), container.end());
    }

    template <typename TContainer>
    inline TContainer ArraySorted(TContainer container)
    {
        std::sort(container.begin(), container.end());
        return container;
    }

    template <typename TContainer>
    inline std::string ArrayJoin(const TContainer& container, const std::string& separator)
    {
        std::string result;
        bool isFirst = true;

        for (const auto& value : container)
        {
            if (!isFirst)
                result += separator;
            result += value;
            isFirst = false;
        }

        return result;
    }

    template <typename TMap, typename TKey>
    inline bool DictContainsKey(const TMap& values, const TKey& key)
    {
        return values.find(key) != values.end();
    }

    template <typename TMap, typename TValue>
    inline bool DictContainsValue(const TMap& values, const TValue& value)
    {
        return std::find_if(values.begin(), values.end(), [&](const auto& entry)
        {
            return entry.second == value;
        }) != values.end();
    }

    template <typename TMap, typename TKey>
    inline auto DictGet(const TMap& values, const TKey& key) -> typename TMap::mapped_type
    {
        if (auto it = values.find(key); it != values.end())
            return it->second;

        detail::throwRuntimeError("Intrinsic 'Get' could not find the requested key.");
    }

    template <typename TMap, typename TKey, typename TFallback>
    inline auto DictGetOr(const TMap& values, const TKey& key, TFallback&& fallback) -> typename TMap::mapped_type
    {
        if (auto it = values.find(key); it != values.end())
            return it->second;
        return static_cast<typename TMap::mapped_type>(std::forward<TFallback>(fallback));
    }

    template <typename TMap, typename TKey, typename TOut>
    inline bool DictTryGet(const TMap& values, const TKey& key, TOut&& outValue)
    {
        if (auto it = values.find(key); it != values.end())
        {
            detail::assignOutValue(std::forward<TOut>(outValue), it->second);
            return true;
        }

        return false;
    }

    template <typename TMap, typename TKey, typename TValue>
    inline void DictSet(TMap& values, const TKey& key, TValue&& value)
    {
        values[key] = std::forward<TValue>(value);
    }

    template <typename TMap, typename TKey, typename TFallback>
    inline auto DictGetOrAdd(TMap& values, const TKey& key, TFallback&& fallback) -> typename TMap::mapped_type
    {
        if (auto it = values.find(key); it != values.end())
            return it->second;

        auto [it, inserted] = values.emplace(key, static_cast<typename TMap::mapped_type>(std::forward<TFallback>(fallback)));
        (void)inserted;
        return it->second;
    }

    template <typename TMap>
    inline auto DictKeys(const TMap& values) -> std::vector<typename TMap::key_type>
    {
        std::vector<typename TMap::key_type> result;
        result.reserve(values.size());
        for (const auto& [key, value] : values)
        {
            (void)value;
            result.push_back(key);
        }
        return result;
    }

    template <typename TMap>
    inline auto DictValues(const TMap& values) -> std::vector<typename TMap::mapped_type>
    {
        std::vector<typename TMap::mapped_type> result;
        result.reserve(values.size());
        for (const auto& [key, value] : values)
        {
            (void)key;
            result.push_back(value);
        }
        return result;
    }

    template <typename TMap>
    inline TMap DictClone(const TMap& values)
    {
        return values;
    }

    template <typename TMap, typename TOther>
    inline TMap DictMerge(const TMap& values, const TOther& other)
    {
        TMap result = values;
        for (const auto& [key, value] : other)
            result[key] = value;
        return result;
    }

    template <typename TMap, typename TOther>
    inline void DictExtend(TMap& values, const TOther& other)
    {
        for (const auto& [key, value] : other)
            values[key] = value;
    }

    template <typename TMap>
    inline void DictClear(TMap& values)
    {
        values.clear();
    }

    template <typename TMap, typename TKey>
    inline bool DictRemove(TMap& values, const TKey& key)
    {
        return values.erase(key) > 0;
    }

    template <typename TTree>
    inline auto TreeFirstKey(const TTree& values) -> typename TTree::key_type
    {
        detail::ensureNotEmpty(values, "FirstKey");
        return values.begin()->first;
    }

    template <typename TTree>
    inline auto TreeFirstValue(const TTree& values) -> typename TTree::mapped_type
    {
        detail::ensureNotEmpty(values, "FirstValue");
        return values.begin()->second;
    }

    template <typename TTree>
    inline auto TreeLastKey(const TTree& values) -> typename TTree::key_type
    {
        detail::ensureNotEmpty(values, "LastKey");
        return std::prev(values.end())->first;
    }

    template <typename TTree>
    inline auto TreeLastValue(const TTree& values) -> typename TTree::mapped_type
    {
        detail::ensureNotEmpty(values, "LastValue");
        return std::prev(values.end())->second;
    }

    template <typename TTree, typename TKey, typename TFallback>
    inline auto TreeFloorKeyOr(const TTree& values, const TKey& key, TFallback&& fallback) -> typename TTree::key_type
    {
        if (values.empty())
            return static_cast<typename TTree::key_type>(std::forward<TFallback>(fallback));

        auto it = values.upper_bound(key);
        if (it == values.begin())
            return static_cast<typename TTree::key_type>(std::forward<TFallback>(fallback));

        --it;
        return it->first;
    }

    template <typename TTree, typename TKey, typename TFallback>
    inline auto TreeCeilKeyOr(const TTree& values, const TKey& key, TFallback&& fallback) -> typename TTree::key_type
    {
        if (auto it = values.lower_bound(key); it != values.end())
            return it->first;
        return static_cast<typename TTree::key_type>(std::forward<TFallback>(fallback));
    }

    inline bool StringContains(const std::string& value, const std::string& needle)
    {
        return value.find(needle) != std::string::npos;
    }

    inline bool StringContainsChar(const std::string& value, const char needle)
    {
        return value.find(needle) != std::string::npos;
    }

    inline bool StringStartsWith(const std::string& value, const std::string& prefix)
    {
        return value.starts_with(prefix);
    }

    inline bool StringEndsWith(const std::string& value, const std::string& suffix)
    {
        return value.ends_with(suffix);
    }

    inline std::ptrdiff_t StringIndexOf(const std::string& value, const std::string& needle)
    {
        if (const auto position = value.find(needle); position != std::string::npos)
            return detail::toSignedIndex(position);
        return -1;
    }

    inline std::ptrdiff_t StringLastIndexOf(const std::string& value, const std::string& needle)
    {
        if (const auto position = value.rfind(needle); position != std::string::npos)
            return detail::toSignedIndex(position);
        return -1;
    }

    inline std::ptrdiff_t StringIndexOfChar(const std::string& value, const char needle)
    {
        if (const auto position = value.find(needle); position != std::string::npos)
            return detail::toSignedIndex(position);
        return -1;
    }

    inline std::ptrdiff_t StringLastIndexOfChar(const std::string& value, const char needle)
    {
        if (const auto position = value.rfind(needle); position != std::string::npos)
            return detail::toSignedIndex(position);
        return -1;
    }

    inline char StringFirst(const std::string& value)
    {
        detail::ensureNotEmpty(value, "First");
        return value.front();
    }

    inline char StringLast(const std::string& value)
    {
        detail::ensureNotEmpty(value, "Last");
        return value.back();
    }

    inline char StringGetOr(const std::string& value, const std::size_t index, const char fallback)
    {
        if (index < value.size())
            return value[index];
        return fallback;
    }

    inline std::string StringSlice(const std::string& value, const std::size_t start, const std::size_t count)
    {
        const std::size_t normalizedStart = detail::clampSliceStart(value, start);
        const std::size_t normalizedCount = detail::clampSliceCount(value, normalizedStart, count);
        return value.substr(normalizedStart, normalizedCount);
    }

    inline std::string StringSliceFrom(const std::string& value, const std::size_t start)
    {
        const std::size_t normalizedStart = detail::clampSliceStart(value, start);
        return value.substr(normalizedStart);
    }

    inline std::string StringTake(const std::string& value, const std::size_t count)
    {
        return value.substr(0, std::min(count, value.size()));
    }

    inline std::string StringSkip(const std::string& value, const std::size_t count)
    {
        return value.substr(std::min(count, value.size()));
    }

    inline std::string StringLeft(const std::string& value, const std::size_t count)
    {
        return StringTake(value, count);
    }

    inline std::string StringRight(const std::string& value, const std::size_t count)
    {
        if (count >= value.size())
            return value;
        return value.substr(value.size() - count);
    }

    inline std::string StringTrim(const std::string& value)
    {
        return detail::trimCopy(value);
    }

    inline std::string StringTrimStart(const std::string& value)
    {
        return detail::trimStartCopy(value);
    }

    inline std::string StringTrimEnd(const std::string& value)
    {
        return detail::trimEndCopy(value);
    }

    inline std::string StringToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), detail::lowerChar);
        return value;
    }

    inline std::string StringToUpper(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), detail::upperChar);
        return value;
    }

    inline std::string StringReplace(std::string value, const std::string& oldValue, const std::string& newValue)
    {
        return detail::replaceAllCopy(std::move(value), oldValue, newValue);
    }

    inline std::string StringReplaceFirst(std::string value, const std::string& oldValue, const std::string& newValue)
    {
        return detail::replaceFirstCopy(std::move(value), oldValue, newValue);
    }

    inline std::string StringRepeat(const std::string& value, const std::size_t count)
    {
        std::string result;
        result.reserve(value.size() * count);
        for (std::size_t index = 0; index < count; ++index)
            result += value;
        return result;
    }

    inline std::vector<std::string> StringSplit(const std::string& value, const std::string& separator)
    {
        std::vector<std::string> result;

        if (separator.empty())
        {
            result.reserve(value.size());
            for (char ch : value)
                result.emplace_back(1, ch);
            return result;
        }

        std::size_t start = 0;
        while (true)
        {
            const std::size_t position = value.find(separator, start);
            if (position == std::string::npos)
            {
                result.push_back(value.substr(start));
                break;
            }

            result.push_back(value.substr(start, position - start));
            start = position + separator.size();
        }

        return result;
    }

    inline std::vector<std::string> StringLines(const std::string& value)
    {
        std::vector<std::string> result;
        std::size_t start = 0;

        while (start <= value.size())
        {
            std::size_t end = start;
            while (end < value.size() && value[end] != '\n' && value[end] != '\r')
                ++end;

            result.push_back(value.substr(start, end - start));

            if (end >= value.size())
                break;

            if (value[end] == '\r' && end + 1 < value.size() && value[end + 1] == '\n')
                start = end + 2;
            else
                start = end + 1;
        }

        return result;
    }

    inline std::string StringPadLeft(const std::string& value, const std::size_t totalWidth, const char fill)
    {
        if (value.size() >= totalWidth)
            return value;
        return std::string(totalWidth - value.size(), fill) + value;
    }

    inline std::string StringPadRight(const std::string& value, const std::size_t totalWidth, const char fill)
    {
        if (value.size() >= totalWidth)
            return value;
        return value + std::string(totalWidth - value.size(), fill);
    }

    inline std::string StringReversed(std::string value)
    {
        std::reverse(value.begin(), value.end());
        return value;
    }

    inline void StringAppend(std::string& value, const std::string& suffix)
    {
        value += suffix;
    }

    inline void StringPush(std::string& value, const char ch)
    {
        value.push_back(ch);
    }

    inline void StringInsert(std::string& value, const std::size_t index, const std::string& fragment)
    {
        value.insert(std::min(index, value.size()), fragment);
    }

    inline void StringErase(std::string& value, const std::size_t index, const std::size_t count)
    {
        if (index >= value.size())
            return;
        value.erase(index, count);
    }

    inline void StringClear(std::string& value)
    {
        value.clear();
    }

    inline void StringReverse(std::string& value)
    {
        std::reverse(value.begin(), value.end());
    }

    inline void StringReplaceInPlace(std::string& value, const std::string& oldValue, const std::string& newValue)
    {
        value = detail::replaceAllCopy(std::move(value), oldValue, newValue);
    }

    inline void StringTrimInPlace(std::string& value)
    {
        value = detail::trimCopy(value);
    }

    inline void StringToLowerInPlace(std::string& value)
    {
        std::transform(value.begin(), value.end(), value.begin(), detail::lowerChar);
    }

    inline void StringToUpperInPlace(std::string& value)
    {
        std::transform(value.begin(), value.end(), value.begin(), detail::upperChar);
    }
}
