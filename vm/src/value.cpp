#include "wio/vm/value.h"

#include <algorithm>
#include <atomic>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace wio::vm
{
    class AggregateStorage final
    {
    public:
        AggregateStorage(const Value::Kind kind, const std::uint32_t type, std::vector<Value> values,
                         const bool ordered = false)
            : kind_(kind), type_(type), values_(std::move(values)), ordered_(ordered)
        {
        }

        void retain() noexcept
        {
            references_.fetch_add(1, std::memory_order_relaxed);
        }

        void release() noexcept
        {
            if (references_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                delete this;
        }

        [[nodiscard]] std::vector<Value>& values() noexcept
        {
            return values_;
        }

        [[nodiscard]] const std::vector<Value>& values() const noexcept
        {
            return values_;
        }

        [[nodiscard]] Value::Kind kind() const noexcept
        {
            return kind_;
        }

        [[nodiscard]] std::uint32_t type() const noexcept
        {
            return type_;
        }

        [[nodiscard]] std::uint32_t referenceCount() const noexcept
        {
            return references_.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool ordered() const noexcept
        {
            return ordered_;
        }

        [[nodiscard]] std::size_t cursor() const noexcept
        {
            return cursor_;
        }

        [[nodiscard]] std::size_t step() const noexcept
        {
            return step_;
        }

        void advanceCursor() noexcept
        {
            cursor_ += step_;
        }

        [[nodiscard]] bool finished() const noexcept
        {
            return finished_;
        }

        void finish() noexcept
        {
            finished_ = true;
        }

        void configureIterator(const std::size_t step) noexcept
        {
            step_ = step;
        }

        [[nodiscard]] AsyncTaskState taskState() const noexcept
        {
            return taskState_.load(std::memory_order_acquire);
        }

        bool beginTask() noexcept
        {
            AsyncTaskState expected = AsyncTaskState::Pending;
            return taskState_.compare_exchange_strong(expected, AsyncTaskState::Running, std::memory_order_acq_rel);
        }

        bool cancelTask() noexcept
        {
            AsyncTaskState state = taskState();
            while (state == AsyncTaskState::Pending || state == AsyncTaskState::Running)
            {
                if (taskState_.compare_exchange_weak(state, AsyncTaskState::Cancelled, std::memory_order_acq_rel))
                    return true;
            }
            return false;
        }

        void completeTask(Value result)
        {
            taskResult_ = std::move(result);
            taskState_.store(AsyncTaskState::Ready, std::memory_order_release);
        }

        void failTask(std::string code, std::string message)
        {
            taskErrorCode_ = std::move(code);
            taskErrorMessage_ = std::move(message);
            taskState_.store(AsyncTaskState::Faulted, std::memory_order_release);
        }

        [[nodiscard]] const Value& taskResult() const noexcept
        {
            return taskResult_;
        }

        [[nodiscard]] std::string_view taskErrorCode() const noexcept
        {
            return taskErrorCode_;
        }

        [[nodiscard]] std::string_view taskErrorMessage() const noexcept
        {
            return taskErrorMessage_;
        }

    private:
        std::atomic<std::uint32_t> references_{1};
        Value::Kind kind_;
        std::uint32_t type_ = 0;
        std::vector<Value> values_;
        bool ordered_ = false;
        std::size_t cursor_ = 0;
        std::size_t step_ = 1;
        bool finished_ = false;
        std::atomic<AsyncTaskState> taskState_{AsyncTaskState::Pending};
        Value taskResult_;
        std::string taskErrorCode_;
        std::string taskErrorMessage_;
    };

    namespace
    {
        int compareDictionaryKeys(const Value& left, const Value& right) noexcept
        {
            if (left.kind() != right.kind())
                return static_cast<int>(left.kind()) < static_cast<int>(right.kind()) ? -1 : 1;
            switch (left.kind())
            {
            case Value::Kind::Boolean:
                return left.asBoolean() == right.asBoolean() ? 0 : (left.asBoolean() ? 1 : -1);
            case Value::Kind::SignedInteger:
                return left.asSignedInteger() == right.asSignedInteger()
                           ? 0
                           : (left.asSignedInteger() < right.asSignedInteger() ? -1 : 1);
            case Value::Kind::UnsignedInteger:
                return left.asUnsignedInteger() == right.asUnsignedInteger()
                           ? 0
                           : (left.asUnsignedInteger() < right.asUnsignedInteger() ? -1 : 1);
            case Value::Kind::Float64:
                return left.asFloat64() == right.asFloat64() ? 0 : (left.asFloat64() < right.asFloat64() ? -1 : 1);
            case Value::Kind::String:
                return left.asString() == right.asString() ? 0 : (left.asString() < right.asString() ? -1 : 1);
            case Value::Kind::Text:
                return left.asText() == right.asText() ? 0 : (left.asText() < right.asText() ? -1 : 1);
            default:
                return left == right ? 0 : -1;
            }
        }

        bool dictionaryValuesEqual(const Value& left, const Value& right) noexcept
        {
            return left == right;
        }

        void sortDictionaryEntries(std::vector<Value>& entries)
        {
            const std::size_t count = entries.size() / 2;
            std::vector<std::size_t> order(count);
            for (std::size_t index = 0; index < count; ++index)
                order[index] = index;
            std::stable_sort(order.begin(), order.end(), [&](const std::size_t left, const std::size_t right)
                             { return compareDictionaryKeys(entries[left * 2], entries[right * 2]) < 0; });
            std::vector<Value> sorted;
            sorted.reserve(entries.size());
            for (const std::size_t index : order)
            {
                sorted.push_back(std::move(entries[index * 2]));
                sorted.push_back(std::move(entries[index * 2 + 1]));
            }
            entries = std::move(sorted);
        }
    } // namespace

    Value::Value(const Kind kind) noexcept : kind_(kind)
    {
    }

    Value::Value(const Value& other)
    {
        copyFrom(other);
    }

    Value::Value(Value&& other) noexcept
    {
        moveFrom(std::move(other));
    }

    Value& Value::operator=(const Value& other)
    {
        if (this == &other)
            return *this;
        destroy();
        copyFrom(other);
        return *this;
    }

    Value& Value::operator=(Value&& other) noexcept
    {
        if (this == &other)
            return *this;
        destroy();
        moveFrom(std::move(other));
        return *this;
    }

    Value::~Value()
    {
        destroy();
    }

    Value Value::null() noexcept
    {
        return Value{Kind::Null};
    }

    Value Value::boolean(const bool value) noexcept
    {
        Value result{Kind::Boolean};
        result.scalar_.boolean = value;
        return result;
    }

    Value Value::signedInteger(const std::int64_t value) noexcept
    {
        Value result{Kind::SignedInteger};
        result.scalar_.signedInteger = value;
        return result;
    }

    Value Value::unsignedInteger(const std::uint64_t value) noexcept
    {
        Value result{Kind::UnsignedInteger};
        result.scalar_.unsignedInteger = value;
        return result;
    }

    Value Value::floating(const double value) noexcept
    {
        Value result{Kind::Float64};
        result.scalar_.floating = value;
        return result;
    }

    Value Value::string(std::string value)
    {
        Value result{Kind::String};
        std::construct_at(&result.text_.string, std::move(value));
        return result;
    }

    Value Value::text(std::u32string value)
    {
        Value result{Kind::Text};
        std::construct_at(&result.text_.text, std::move(value));
        return result;
    }

    Value Value::array(std::vector<Value> values)
    {
        Value result{Kind::Array};
        result.scalar_.aggregate = new AggregateStorage{Kind::Array, 0, std::move(values)};
        return result;
    }

    Value Value::dictionary(const std::uint32_t type, std::vector<Value> entries, const bool ordered)
    {
        std::vector<Value> unique;
        unique.reserve(entries.size());
        for (std::size_t index = 0; index + 1 < entries.size(); index += 2)
        {
            bool duplicate = false;
            for (std::size_t existing = 0; existing < unique.size(); existing += 2)
            {
                if (unique[existing] == entries[index])
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
            {
                unique.push_back(std::move(entries[index]));
                unique.push_back(std::move(entries[index + 1]));
            }
        }
        if (ordered)
            sortDictionaryEntries(unique);
        Value result{Kind::Dictionary};
        result.scalar_.aggregate = new AggregateStorage{Kind::Dictionary, type, std::move(unique), ordered};
        return result;
    }

    Value Value::component(const std::uint32_t type, std::vector<Value> fields)
    {
        Value result{Kind::Component};
        result.scalar_.aggregate = new AggregateStorage{Kind::Component, type, std::move(fields)};
        return result;
    }

    Value Value::object(const std::uint32_t type, std::vector<Value> fields)
    {
        Value result{Kind::Object};
        result.scalar_.aggregate = new AggregateStorage{Kind::Object, type, std::move(fields)};
        return result;
    }

    Value Value::callable(const std::uint32_t function, std::vector<Value> captures)
    {
        Value result{Kind::Callable};
        result.scalar_.aggregate = new AggregateStorage{Kind::Callable, function, std::move(captures)};
        return result;
    }

    Value Value::asyncTask(const std::uint32_t function, std::vector<Value> arguments)
    {
        Value result{Kind::AsyncTask};
        result.scalar_.aggregate = new AggregateStorage{Kind::AsyncTask, function, std::move(arguments)};
        return result;
    }

    Value Value::any(const std::uint32_t type, Value payload)
    {
        Value result{Kind::Any};
        std::vector<Value> values;
        values.push_back(std::move(payload));
        result.scalar_.aggregate = new AggregateStorage{Kind::Any, type, std::move(values)};
        return result;
    }

    Value Value::rangeIterator(const std::uint32_t type, Value start, Value end, Value step, const bool inclusive)
    {
        Value result{Kind::Iterator};
        std::vector<Value> state;
        state.reserve(3);
        state.push_back(std::move(start));
        state.push_back(std::move(end));
        state.push_back(std::move(step));
        result.scalar_.aggregate = new AggregateStorage{Kind::Iterator, type, std::move(state), inclusive};
        return result;
    }

    Value Value::containerIterator(const std::uint32_t type, Value source, const std::size_t step)
    {
        Value result{Kind::Iterator};
        std::vector<Value> state;
        state.push_back(std::move(source));
        result.scalar_.aggregate = new AggregateStorage{Kind::Iterator, type, std::move(state)};
        result.scalar_.aggregate->configureIterator(step);
        return result;
    }

    Value Value::objectBorrow(AggregateStorage* const storage) noexcept
    {
        Value result{Kind::ObjectBorrow};
        result.scalar_.aggregate = storage;
        return result;
    }

    std::uint32_t Value::aggregateType() const noexcept
    {
        return kind_ == Kind::Array || kind_ == Kind::Dictionary || kind_ == Kind::Component || kind_ == Kind::Object ||
                       kind_ == Kind::ObjectBorrow || kind_ == Kind::Any || kind_ == Kind::Iterator
                   ? scalar_.aggregate->type()
                   : 0;
    }

    std::uint32_t Value::anyType() const noexcept
    {
        return kind_ == Kind::Any ? scalar_.aggregate->type() : 0;
    }

    const Value* Value::anyPayload() const noexcept
    {
        return kind_ == Kind::Any && !scalar_.aggregate->values().empty() ? &scalar_.aggregate->values().front()
                                                                          : nullptr;
    }

    bool Value::iteratorIsRange() const noexcept
    {
        return kind_ == Kind::Iterator && scalar_.aggregate->values().size() == 3;
    }

    bool Value::iteratorInclusive() const noexcept
    {
        return kind_ == Kind::Iterator && scalar_.aggregate->ordered();
    }

    bool Value::iteratorFinished() const noexcept
    {
        return kind_ != Kind::Iterator || scalar_.aggregate->finished();
    }

    void Value::finishIterator() noexcept
    {
        if (kind_ == Kind::Iterator)
            scalar_.aggregate->finish();
    }

    std::size_t Value::iteratorPosition() const noexcept
    {
        return kind_ == Kind::Iterator ? scalar_.aggregate->cursor() : 0;
    }

    std::size_t Value::iteratorStep() const noexcept
    {
        return kind_ == Kind::Iterator ? scalar_.aggregate->step() : 0;
    }

    void Value::advanceIteratorPosition() noexcept
    {
        if (kind_ == Kind::Iterator)
            scalar_.aggregate->advanceCursor();
    }

    const Value* Value::iteratorState(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::Iterator || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value* Value::mutableIteratorState(const std::size_t index) noexcept
    {
        if (kind_ != Kind::Iterator || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    std::uint32_t Value::strongReferenceCount() const noexcept
    {
        return kind_ == Kind::Object ? scalar_.aggregate->referenceCount() : 0;
    }

    std::uint32_t Value::callableFunction() const noexcept
    {
        return kind_ == Kind::Callable ? scalar_.aggregate->type() : 0;
    }

    std::size_t Value::captureCount() const noexcept
    {
        return kind_ == Kind::Callable ? scalar_.aggregate->values().size() : 0;
    }

    const Value* Value::capture(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::Callable || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    AsyncTaskState Value::taskState() const noexcept
    {
        return kind_ == Kind::AsyncTask ? scalar_.aggregate->taskState() : AsyncTaskState::Faulted;
    }

    std::uint32_t Value::taskFunction() const noexcept
    {
        return kind_ == Kind::AsyncTask ? scalar_.aggregate->type() : 0;
    }

    std::size_t Value::taskArgumentCount() const noexcept
    {
        return kind_ == Kind::AsyncTask ? scalar_.aggregate->values().size() : 0;
    }

    const Value* Value::taskArgument(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::AsyncTask || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value Value::taskResult() const
    {
        return kind_ == Kind::AsyncTask ? scalar_.aggregate->taskResult() : Value{};
    }

    std::string_view Value::taskErrorCode() const noexcept
    {
        return kind_ == Kind::AsyncTask ? scalar_.aggregate->taskErrorCode() : std::string_view{};
    }

    std::string_view Value::taskErrorMessage() const noexcept
    {
        return kind_ == Kind::AsyncTask ? scalar_.aggregate->taskErrorMessage() : std::string_view{};
    }

    Value Value::place(PlaceStorage* const storage) noexcept
    {
        Value result{Kind::Place};
        result.scalar_.place = storage;
        return result;
    }

    PlaceStorage* Value::asPlace() const noexcept
    {
        return kind_ == Kind::Place ? scalar_.place : nullptr;
    }

    std::string_view Value::asString() const noexcept
    {
        return kind_ == Kind::String ? std::string_view{text_.string} : std::string_view{};
    }

    std::u32string_view Value::asText() const noexcept
    {
        return kind_ == Kind::Text ? std::u32string_view{text_.text} : std::u32string_view{};
    }

    std::size_t Value::elementCount() const noexcept
    {
        return kind_ == Kind::Array ? scalar_.aggregate->values().size() : 0;
    }

    std::size_t Value::elementCapacity() const noexcept
    {
        return kind_ == Kind::Array ? scalar_.aggregate->values().capacity() : 0;
    }

    const Value* Value::element(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::Array || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value* Value::mutableElement(const std::size_t index) noexcept
    {
        if (kind_ != Kind::Array || index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value Value::arraySlice(const std::size_t start, const std::size_t count) const
    {
        if (kind_ != Kind::Array)
            return {};
        const std::vector<Value>& source = scalar_.aggregate->values();
        const std::size_t normalizedStart = (std::min)(start, source.size());
        const std::size_t normalizedCount = (std::min)(count, source.size() - normalizedStart);
        std::vector<Value> values;
        values.reserve(normalizedCount);
        for (std::size_t index = normalizedStart; index < normalizedStart + normalizedCount; ++index)
            values.push_back(source[index].cloneOwned());
        return array(std::move(values));
    }

    Value Value::arrayConcat(const Value& other) const
    {
        if (kind_ != Kind::Array || other.kind_ != Kind::Array)
            return {};
        std::vector<Value> values;
        values.reserve(elementCount() + other.elementCount());
        for (const Value& value : scalar_.aggregate->values())
            values.push_back(value.cloneOwned());
        for (const Value& value : other.scalar_.aggregate->values())
            values.push_back(value.cloneOwned());
        return array(std::move(values));
    }

    bool Value::arrayPush(Value value, const bool front)
    {
        if (kind_ != Kind::Array)
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        if (front)
            values.insert(values.begin(), std::move(value));
        else
            values.push_back(std::move(value));
        return true;
    }

    bool Value::arrayPop(Value& value, const bool front)
    {
        if (kind_ != Kind::Array || elementCount() == 0)
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        if (front)
        {
            value = std::move(values.front());
            values.erase(values.begin());
        }
        else
        {
            value = std::move(values.back());
            values.pop_back();
        }
        return true;
    }

    bool Value::arrayInsert(const std::size_t index, Value value)
    {
        if (kind_ != Kind::Array || index > elementCount())
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        values.insert(values.begin() + static_cast<std::ptrdiff_t>(index), std::move(value));
        return true;
    }

    bool Value::arrayRemoveAt(const std::size_t index) noexcept
    {
        if (kind_ != Kind::Array || index >= elementCount())
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        values.erase(values.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool Value::arrayRemove(const Value& value) noexcept
    {
        if (kind_ != Kind::Array)
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        const auto found = std::find(values.begin(), values.end(), value);
        if (found == values.end())
            return false;
        values.erase(found);
        return true;
    }

    bool Value::arrayExtend(const Value& other)
    {
        if (kind_ != Kind::Array || other.kind_ != Kind::Array)
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        const std::vector<Value>& appended = other.scalar_.aggregate->values();
        std::vector<Value> copies;
        copies.reserve(appended.size());
        for (const Value& value : appended)
            copies.push_back(value.cloneOwned());
        values.reserve(values.size() + copies.size());
        values.insert(values.end(), std::make_move_iterator(copies.begin()), std::make_move_iterator(copies.end()));
        return true;
    }

    bool Value::arrayReserve(const std::size_t capacity)
    {
        if (kind_ != Kind::Array)
            return false;
        scalar_.aggregate->values().reserve(capacity);
        return true;
    }

    void Value::arrayShrinkToFit()
    {
        if (kind_ == Kind::Array)
            scalar_.aggregate->values().shrink_to_fit();
    }

    void Value::arrayClear() noexcept
    {
        if (kind_ == Kind::Array)
            scalar_.aggregate->values().clear();
    }

    void Value::arrayFill(const Value& value)
    {
        if (kind_ == Kind::Array)
        {
            for (Value& element : scalar_.aggregate->values())
                element = value.cloneOwned();
        }
    }

    void Value::arrayReverse()
    {
        if (kind_ == Kind::Array)
            std::reverse(scalar_.aggregate->values().begin(), scalar_.aggregate->values().end());
    }

    bool Value::arraySort()
    {
        if (kind_ != Kind::Array)
            return false;
        std::vector<Value>& values = scalar_.aggregate->values();
        if (!std::all_of(values.begin(), values.end(),
                         [&](const Value& value) { return values.empty() || value.kind() == values.front().kind(); }))
            return false;
        std::sort(values.begin(), values.end(),
                  [](const Value& left, const Value& right) { return left.lessThan(right); });
        return true;
    }

    std::size_t Value::dictionaryCount() const noexcept
    {
        return kind_ == Kind::Dictionary ? scalar_.aggregate->values().size() / 2 : 0;
    }

    bool Value::dictionaryIsOrdered() const noexcept
    {
        return kind_ == Kind::Dictionary && scalar_.aggregate->ordered();
    }

    const Value* Value::dictionaryKey(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::Dictionary || index >= dictionaryCount())
            return nullptr;
        return &scalar_.aggregate->values()[index * 2];
    }

    const Value* Value::dictionaryValue(const std::size_t index) const noexcept
    {
        if (kind_ != Kind::Dictionary || index >= dictionaryCount())
            return nullptr;
        return &scalar_.aggregate->values()[index * 2 + 1];
    }

    const Value* Value::dictionaryValue(const Value& key) const noexcept
    {
        if (kind_ != Kind::Dictionary)
            return nullptr;
        const std::vector<Value>& entries = scalar_.aggregate->values();
        for (std::size_t index = 0; index < entries.size(); index += 2)
        {
            if (entries[index] == key)
                return &entries[index + 1];
        }
        return nullptr;
    }

    Value* Value::mutableDictionaryValue(const Value& key) noexcept
    {
        if (kind_ != Kind::Dictionary)
            return nullptr;
        std::vector<Value>& entries = scalar_.aggregate->values();
        for (std::size_t index = 0; index < entries.size(); index += 2)
        {
            if (entries[index] == key)
                return &entries[index + 1];
        }
        return nullptr;
    }

    const Value* Value::dictionaryFloorKey(const Value& key) const noexcept
    {
        if (!dictionaryIsOrdered())
            return nullptr;
        const Value* found = nullptr;
        for (std::size_t index = 0; index < dictionaryCount(); ++index)
        {
            const Value* candidate = dictionaryKey(index);
            if (compareDictionaryKeys(*candidate, key) > 0)
                break;
            found = candidate;
        }
        return found;
    }

    const Value* Value::dictionaryCeilKey(const Value& key) const noexcept
    {
        if (!dictionaryIsOrdered())
            return nullptr;
        for (std::size_t index = 0; index < dictionaryCount(); ++index)
        {
            const Value* candidate = dictionaryKey(index);
            if (compareDictionaryKeys(*candidate, key) >= 0)
                return candidate;
        }
        return nullptr;
    }

    bool Value::dictionaryContainsValue(const Value& value) const noexcept
    {
        for (std::size_t index = 0; index < dictionaryCount(); ++index)
        {
            if (*dictionaryValue(index) == value)
                return true;
        }
        return false;
    }

    bool Value::dictionarySet(Value key, Value value)
    {
        if (kind_ != Kind::Dictionary)
            return false;
        if (Value* existing = mutableDictionaryValue(key))
        {
            *existing = std::move(value);
            return false;
        }
        std::vector<Value>& entries = scalar_.aggregate->values();
        entries.push_back(std::move(key));
        entries.push_back(std::move(value));
        if (dictionaryIsOrdered())
            sortDictionaryEntries(entries);
        return true;
    }

    bool Value::dictionaryRemove(const Value& key) noexcept
    {
        if (kind_ != Kind::Dictionary)
            return false;
        std::vector<Value>& entries = scalar_.aggregate->values();
        for (std::size_t index = 0; index < entries.size(); index += 2)
        {
            if (entries[index] == key)
            {
                entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index),
                              entries.begin() + static_cast<std::ptrdiff_t>(index + 2));
                return true;
            }
        }
        return false;
    }

    void Value::dictionaryClear() noexcept
    {
        if (kind_ == Kind::Dictionary)
            scalar_.aggregate->values().clear();
    }

    std::size_t Value::fieldCount() const noexcept
    {
        return kind_ == Kind::Component || kind_ == Kind::Object || kind_ == Kind::ObjectBorrow
                   ? scalar_.aggregate->values().size()
                   : 0;
    }

    const Value* Value::field(const std::size_t index) const noexcept
    {
        if ((kind_ != Kind::Component && kind_ != Kind::Object && kind_ != Kind::ObjectBorrow) ||
            index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value* Value::mutableField(const std::size_t index) noexcept
    {
        if ((kind_ != Kind::Component && kind_ != Kind::Object && kind_ != Kind::ObjectBorrow) ||
            index >= scalar_.aggregate->values().size())
            return nullptr;
        return &scalar_.aggregate->values()[index];
    }

    Value Value::cloneOwned() const
    {
        if (kind_ != Kind::Array && kind_ != Kind::Dictionary && kind_ != Kind::Component)
            return *this;
        std::vector<Value> cloned;
        cloned.reserve(scalar_.aggregate->values().size());
        for (const Value& value : scalar_.aggregate->values())
            cloned.push_back(value.cloneOwned());
        if (kind_ == Kind::Array)
            return array(std::move(cloned));
        if (kind_ == Kind::Dictionary)
            return dictionary(aggregateType(), std::move(cloned), dictionaryIsOrdered());
        return component(aggregateType(), std::move(cloned));
    }

    Value Value::retainObject() const noexcept
    {
        if (kind_ != Kind::Object && kind_ != Kind::ObjectBorrow)
            return {};
        Value result{Kind::Object};
        result.scalar_.aggregate = scalar_.aggregate;
        result.scalar_.aggregate->retain();
        return result;
    }

    bool Value::beginTask() const noexcept
    {
        return kind_ == Kind::AsyncTask && scalar_.aggregate->beginTask();
    }

    bool Value::cancelTask() const noexcept
    {
        return kind_ == Kind::AsyncTask && scalar_.aggregate->cancelTask();
    }

    void Value::completeTask(Value result) const
    {
        if (kind_ == Kind::AsyncTask)
            scalar_.aggregate->completeTask(std::move(result));
    }

    void Value::failTask(std::string code, std::string message) const
    {
        if (kind_ == Kind::AsyncTask)
            scalar_.aggregate->failTask(std::move(code), std::move(message));
    }

    bool Value::operator==(const Value& other) const noexcept
    {
        if (kind_ != other.kind_)
            return false;
        switch (kind_)
        {
        case Kind::Empty:
        case Kind::Null:
            return true;
        case Kind::Boolean:
            return scalar_.boolean == other.scalar_.boolean;
        case Kind::SignedInteger:
            return scalar_.signedInteger == other.scalar_.signedInteger;
        case Kind::UnsignedInteger:
            return scalar_.unsignedInteger == other.scalar_.unsignedInteger;
        case Kind::Float64:
            return scalar_.floating == other.scalar_.floating;
        case Kind::String:
            return text_.string == other.text_.string;
        case Kind::Text:
            return text_.text == other.text_.text;
        case Kind::Array:
            return scalar_.aggregate->values() == other.scalar_.aggregate->values();
        case Kind::Dictionary:
            if (aggregateType() != other.aggregateType() || dictionaryCount() != other.dictionaryCount())
                return false;
            for (std::size_t index = 0; index < dictionaryCount(); ++index)
            {
                const Value* otherValue = other.dictionaryValue(*dictionaryKey(index));
                if (!otherValue || !dictionaryValuesEqual(*otherValue, *dictionaryValue(index)))
                    return false;
            }
            return true;
        case Kind::Component:
            return aggregateType() == other.aggregateType() &&
                   scalar_.aggregate->values() == other.scalar_.aggregate->values();
        case Kind::Object:
        case Kind::ObjectBorrow:
        case Kind::Callable:
        case Kind::AsyncTask:
        case Kind::Any:
        case Kind::Iterator:
            return scalar_.aggregate == other.scalar_.aggregate;
        case Kind::Place:
            return scalar_.place == other.scalar_.place;
        }
        return false;
    }

    bool Value::lessThan(const Value& other) const noexcept
    {
        if (kind_ != other.kind_)
            return false;
        switch (kind_)
        {
        case Kind::Boolean:
            return !scalar_.boolean && other.scalar_.boolean;
        case Kind::SignedInteger:
            return scalar_.signedInteger < other.scalar_.signedInteger;
        case Kind::UnsignedInteger:
            return scalar_.unsignedInteger < other.scalar_.unsignedInteger;
        case Kind::Float64:
            return scalar_.floating < other.scalar_.floating;
        case Kind::String:
            return text_.string < other.text_.string;
        case Kind::Text:
            return text_.text < other.text_.text;
        default:
            return false;
        }
    }

    void Value::destroy() noexcept
    {
        if (kind_ == Kind::String)
            std::destroy_at(&text_.string);
        else if (kind_ == Kind::Text)
            std::destroy_at(&text_.text);
        else if (kind_ == Kind::Array || kind_ == Kind::Dictionary || kind_ == Kind::Component ||
                 kind_ == Kind::Object || kind_ == Kind::Callable || kind_ == Kind::AsyncTask || kind_ == Kind::Any ||
                 kind_ == Kind::Iterator)
            scalar_.aggregate->release();
        kind_ = Kind::Empty;
        scalar_.unsignedInteger = 0;
    }

    void Value::copyFrom(const Value& other)
    {
        kind_ = other.kind_;
        if (kind_ == Kind::String)
            std::construct_at(&text_.string, other.text_.string);
        else if (kind_ == Kind::Text)
            std::construct_at(&text_.text, other.text_.text);
        else if (kind_ == Kind::Array || kind_ == Kind::Dictionary || kind_ == Kind::Component ||
                 kind_ == Kind::Object || kind_ == Kind::Callable || kind_ == Kind::AsyncTask || kind_ == Kind::Any ||
                 kind_ == Kind::Iterator)
        {
            scalar_.aggregate = other.scalar_.aggregate;
            scalar_.aggregate->retain();
        }
        else
            scalar_ = other.scalar_;
    }

    void Value::moveFrom(Value&& other) noexcept
    {
        kind_ = other.kind_;
        if (kind_ == Kind::String)
        {
            std::construct_at(&text_.string, std::move(other.text_.string));
            std::destroy_at(&other.text_.string);
        }
        else if (kind_ == Kind::Text)
        {
            std::construct_at(&text_.text, std::move(other.text_.text));
            std::destroy_at(&other.text_.text);
        }
        else
            scalar_ = other.scalar_;
        other.kind_ = Kind::Empty;
        other.scalar_.unsignedInteger = 0;
    }
} // namespace wio::vm
