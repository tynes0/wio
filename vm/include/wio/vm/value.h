#pragma once

#include "wio/vm/diagnostic.h"

#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace wio::vm
{
    class Machine;
    class PlaceStorage;
    class AggregateStorage;

    enum class AsyncTaskState : std::uint8_t
    {
        Pending,
        Running,
        Ready,
        Cancelled,
        Faulted
    };

    class Value final
    {
    public:
        enum class Kind : std::uint8_t
        {
            Empty,
            Null,
            Boolean,
            SignedInteger,
            UnsignedInteger,
            Float64,
            String,
            Text,
            Array,
            Dictionary,
            Component,
            Object,
            ObjectBorrow,
            Callable,
            AsyncTask,
            Any,
            Iterator,
            Place
        };

        Value() noexcept = default;
        Value(const Value& other);
        Value(Value&& other) noexcept;
        Value& operator=(const Value& other);
        Value& operator=(Value&& other) noexcept;
        ~Value();

        [[nodiscard]] static Value null() noexcept;
        [[nodiscard]] static Value boolean(bool value) noexcept;
        [[nodiscard]] static Value signedInteger(std::int64_t value) noexcept;
        [[nodiscard]] static Value unsignedInteger(std::uint64_t value) noexcept;
        [[nodiscard]] static Value floating(double value) noexcept;
        [[nodiscard]] static Value string(std::string value);
        [[nodiscard]] static Value text(std::u32string value);
        [[nodiscard]] static Value array(std::vector<Value> values);
        [[nodiscard]] static Value dictionary(std::uint32_t type, std::vector<Value> entries, bool ordered);
        [[nodiscard]] static Value component(std::uint32_t type, std::vector<Value> fields);
        [[nodiscard]] static Value object(std::uint32_t type, std::vector<Value> fields);
        [[nodiscard]] static Value callable(std::uint32_t function, std::vector<Value> captures);
        [[nodiscard]] static Value asyncTask(std::uint32_t function, std::vector<Value> arguments);
        [[nodiscard]] static Value any(std::uint32_t type, Value payload);
        [[nodiscard]] std::uint32_t aggregateType() const noexcept;
        [[nodiscard]] std::uint32_t strongReferenceCount() const noexcept;
        [[nodiscard]] std::uint32_t callableFunction() const noexcept;
        [[nodiscard]] std::size_t captureCount() const noexcept;
        [[nodiscard]] const Value* capture(std::size_t index) const noexcept;
        [[nodiscard]] AsyncTaskState taskState() const noexcept;
        [[nodiscard]] std::uint32_t taskFunction() const noexcept;
        [[nodiscard]] std::size_t taskArgumentCount() const noexcept;
        [[nodiscard]] const Value* taskArgument(std::size_t index) const noexcept;
        [[nodiscard]] Value taskResult() const;
        [[nodiscard]] std::string_view taskErrorCode() const noexcept;
        [[nodiscard]] std::string_view taskErrorMessage() const noexcept;

        [[nodiscard]] Kind kind() const noexcept
        {
            return kind_;
        }
        [[nodiscard]] bool isEmpty() const noexcept
        {
            return kind_ == Kind::Empty;
        }
        [[nodiscard]] bool asBoolean() const noexcept
        {
            return scalar_.boolean;
        }
        [[nodiscard]] std::int64_t asSignedInteger() const noexcept
        {
            return scalar_.signedInteger;
        }
        [[nodiscard]] std::uint64_t asUnsignedInteger() const noexcept
        {
            return scalar_.unsignedInteger;
        }
        [[nodiscard]] double asFloat64() const noexcept
        {
            return scalar_.floating;
        }
        [[nodiscard]] std::string_view asString() const noexcept;
        [[nodiscard]] std::u32string_view asText() const noexcept;
        [[nodiscard]] std::size_t elementCount() const noexcept;
        [[nodiscard]] std::size_t elementCapacity() const noexcept;
        [[nodiscard]] const Value* element(std::size_t index) const noexcept;
        [[nodiscard]] Value arraySlice(std::size_t start, std::size_t count) const;
        [[nodiscard]] Value arrayConcat(const Value& other) const;
        bool arrayPush(Value value, bool front = false);
        bool arrayPop(Value& value, bool front = false);
        bool arrayInsert(std::size_t index, Value value);
        bool arrayRemoveAt(std::size_t index) noexcept;
        bool arrayRemove(const Value& value) noexcept;
        bool arrayExtend(const Value& other);
        bool arrayReserve(std::size_t capacity);
        void arrayShrinkToFit();
        void arrayClear() noexcept;
        void arrayFill(const Value& value);
        void arrayReverse();
        bool arraySort();
        [[nodiscard]] std::size_t dictionaryCount() const noexcept;
        [[nodiscard]] bool dictionaryIsOrdered() const noexcept;
        [[nodiscard]] const Value* dictionaryKey(std::size_t index) const noexcept;
        [[nodiscard]] const Value* dictionaryValue(std::size_t index) const noexcept;
        [[nodiscard]] const Value* dictionaryValue(const Value& key) const noexcept;
        [[nodiscard]] const Value* dictionaryFloorKey(const Value& key) const noexcept;
        [[nodiscard]] const Value* dictionaryCeilKey(const Value& key) const noexcept;
        [[nodiscard]] bool dictionaryContainsValue(const Value& value) const noexcept;
        bool dictionarySet(Value key, Value value);
        bool dictionaryRemove(const Value& key) noexcept;
        void dictionaryClear() noexcept;
        [[nodiscard]] std::size_t fieldCount() const noexcept;
        [[nodiscard]] const Value* field(std::size_t index) const noexcept;

        [[nodiscard]] bool operator==(const Value& other) const noexcept;
        [[nodiscard]] bool lessThan(const Value& other) const noexcept;

    private:
        using TaskWaiter = std::function<void(AsyncTaskState, const Value&, const ExecutionError&)>;

        union Scalar
        {
            bool boolean;
            std::int64_t signedInteger;
            std::uint64_t unsignedInteger;
            double floating;
            PlaceStorage* place;
            AggregateStorage* aggregate;

            constexpr Scalar() noexcept : unsignedInteger(0)
            {
            }
        } scalar_;

        union Text
        {
            constexpr Text() noexcept : empty{}
            {
            }
            ~Text()
            {
            }

            char empty;
            std::string string;
            std::u32string text;
        } text_;

        explicit Value(Kind kind) noexcept;
        [[nodiscard]] static Value place(PlaceStorage* storage) noexcept;
        [[nodiscard]] static Value objectBorrow(AggregateStorage* storage) noexcept;
        [[nodiscard]] static Value externalTask();
        [[nodiscard]] static Value rangeIterator(std::uint32_t type, Value start, Value end, Value step,
                                                 bool inclusive);
        [[nodiscard]] static Value containerIterator(std::uint32_t type, Value source, std::size_t step);
        [[nodiscard]] PlaceStorage* asPlace() const noexcept;
        [[nodiscard]] std::uint32_t anyType() const noexcept;
        [[nodiscard]] const Value* anyPayload() const noexcept;
        [[nodiscard]] bool iteratorIsRange() const noexcept;
        [[nodiscard]] bool iteratorInclusive() const noexcept;
        [[nodiscard]] bool iteratorFinished() const noexcept;
        void finishIterator() noexcept;
        [[nodiscard]] std::size_t iteratorPosition() const noexcept;
        [[nodiscard]] std::size_t iteratorStep() const noexcept;
        void advanceIteratorPosition() noexcept;
        [[nodiscard]] const Value* iteratorState(std::size_t index) const noexcept;
        [[nodiscard]] Value* mutableIteratorState(std::size_t index) noexcept;
        [[nodiscard]] Value* mutableElement(std::size_t index) noexcept;
        [[nodiscard]] Value* mutableDictionaryValue(const Value& key) noexcept;
        [[nodiscard]] Value* mutableField(std::size_t index) noexcept;
        [[nodiscard]] Value cloneOwned() const;
        [[nodiscard]] Value retainObject() const noexcept;
        [[nodiscard]] std::vector<Value> takeOwnedChildrenForCleanup();
        [[nodiscard]] std::vector<Value> takeTaskArgumentsForCleanup() const;
        bool beginTask() const noexcept;
        bool cancelTask() const noexcept;
        bool completeTask(Value result) const;
        bool failTask(ExecutionError error) const;
        [[nodiscard]] ExecutionError taskError() const;
        void waitTask() const;
        bool waitTaskFor(std::chrono::milliseconds duration) const;
        bool registerTaskWaiter(TaskWaiter waiter) const;
        void destroy() noexcept;
        void copyFrom(const Value& other);
        void moveFrom(Value&& other) noexcept;

        Kind kind_ = Kind::Empty;

        friend class Machine;
    };
} // namespace wio::vm
