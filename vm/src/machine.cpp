#include "wio/vm/machine.h"

#include "wio/bytecode/format.h"
#include "wio/bytecode/verifier.h"
#include "wio/vm/unicode.h"

#include "string_intrinsics.h"
#include "text_intrinsics.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <condition_variable>
#include <cmath>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wio::vm
{
    class PlaceStorage final
    {
    public:
        Value value;
        Value owner;
        Value* alias = nullptr;
        bool initialized = false;
        bool mutableValue = true;

        [[nodiscard]] Value& storedValue() noexcept
        {
            return alias ? *alias : value;
        }

        [[nodiscard]] const Value& storedValue() const noexcept
        {
            return alias ? *alias : value;
        }

        void clear() noexcept
        {
            storedValue() = {};
            initialized = false;
        }
    };

    namespace
    {
        // These values are part of the pinned bytecode semantic-enum ABI.
        constexpr std::uint8_t TypeVoid = 1;
        constexpr std::uint8_t TypeBool = 2;
        constexpr std::uint8_t TypeI8 = 3;
        constexpr std::uint8_t TypeI16 = 4;
        constexpr std::uint8_t TypeI32 = 5;
        constexpr std::uint8_t TypeI64 = 6;
        constexpr std::uint8_t TypeISize = 7;
        constexpr std::uint8_t TypeU8 = 8;
        constexpr std::uint8_t TypeU16 = 9;
        constexpr std::uint8_t TypeU32 = 10;
        constexpr std::uint8_t TypeU64 = 11;
        constexpr std::uint8_t TypeUSize = 12;
        constexpr std::uint8_t TypeF32 = 13;
        constexpr std::uint8_t TypeF64 = 14;
        constexpr std::uint8_t TypeByte = 15;
        constexpr std::uint8_t TypeChar = 16;
        constexpr std::uint8_t TypeString = 17;
        constexpr std::uint8_t TypeText = 18;
        constexpr std::uint8_t TypeAny = 19;
        constexpr std::uint8_t TypeNamed = 28;
        constexpr std::uint8_t TypeReference = 29;
        constexpr std::uint8_t TypeNullable = 30;
        constexpr std::uint8_t TypeArray = 31;
        constexpr std::uint8_t TypeDictionary = 32;
        constexpr std::uint8_t TypeFunction = 33;
        constexpr std::uint8_t TypeIterator = 35;

        constexpr std::uint8_t NominalComponent = 1;
        constexpr std::uint8_t NominalObject = 2;
        constexpr std::uint8_t NominalInterface = 3;
        constexpr std::uint8_t NominalEnum = 4;
        constexpr std::uint8_t NominalFlagset = 5;

        constexpr std::uint8_t IntrinsicArray = 1;
        constexpr std::uint8_t IntrinsicDictionary = 2;
        constexpr std::uint8_t IntrinsicString = 3;
        constexpr std::uint8_t IntrinsicText = 4;
        constexpr std::uint8_t IntrinsicEnum = 5;
        constexpr std::uint8_t IntrinsicFlagset = 6;

        constexpr std::uint8_t CaptureValue = 0;
        constexpr std::uint8_t CaptureReference = 1;
        constexpr std::uint8_t CaptureRetainedSelf = 2;

        constexpr std::uint8_t AsyncAwaitTask = 1;
        constexpr std::uint8_t AsyncSwitchExecutor = 2;

        constexpr std::uint8_t UnaryNegate = 0;
        constexpr std::uint8_t UnaryLogicalNot = 1;
        constexpr std::uint8_t UnaryBitwiseNot = 2;

        constexpr std::uint8_t BinaryAdd = 0;
        constexpr std::uint8_t BinarySubtract = 1;
        constexpr std::uint8_t BinaryMultiply = 2;
        constexpr std::uint8_t BinaryDivide = 3;
        constexpr std::uint8_t BinaryRemainder = 4;
        constexpr std::uint8_t BinaryEqual = 5;
        constexpr std::uint8_t BinaryNotEqual = 6;
        constexpr std::uint8_t BinaryLess = 7;
        constexpr std::uint8_t BinaryLessEqual = 8;
        constexpr std::uint8_t BinaryGreater = 9;
        constexpr std::uint8_t BinaryGreaterEqual = 10;
        constexpr std::uint8_t BinaryBitwiseAnd = 11;
        constexpr std::uint8_t BinaryBitwiseOr = 12;
        constexpr std::uint8_t BinaryBitwiseXor = 13;
        constexpr std::uint8_t BinaryShiftLeft = 14;
        constexpr std::uint8_t BinaryShiftRight = 15;

        thread_local ExecutorKind activeExecutor = ExecutorKind::Inherit;

        struct Register
        {
            Value value;
            bool initialized = false;
        };

        enum class FrameCompletion : std::uint8_t
        {
            ReturnValue,
            Ignore,
            ContinueConstruction
        };

        struct FrameContinuation
        {
            FrameCompletion completion = FrameCompletion::ReturnValue;
            std::uint32_t callerResult = bytecode::InvalidIndex;
            std::uint32_t nextFunction = bytecode::InvalidIndex;
            std::vector<Value> nextArguments;
        };

        struct Frame
        {
            const bytecode::Function* function = nullptr;
            const bytecode::Block* block = nullptr;
            std::size_t instruction = 0;
            std::vector<Register> registers;
            std::vector<std::unique_ptr<PlaceStorage>> localPlaces;
            FrameContinuation continuation;
            Value resumedValue;
            std::uint32_t resumedState = bytecode::InvalidIndex;
            bool hasResumedValue = false;
        };

        [[nodiscard]] ExecutionError withStack(ExecutionError error, const std::vector<Frame>& frames)
        {
            error.stack.reserve(error.stack.size() + frames.size());
            bool leaf = true;
            for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame)
            {
                std::size_t instructionIndex = frame->instruction;
                if (!leaf && instructionIndex > 0)
                    --instructionIndex;
                const bytecode::SourceSpan source = instructionIndex < frame->block->instructions.size()
                                                        ? frame->block->instructions[instructionIndex].source
                                                        : frame->block->source;
                error.stack.push_back(
                    {frame->function->id, frame->block->id, static_cast<std::uint32_t>(instructionIndex), source});
                leaf = false;
            }
            return error;
        }

        [[nodiscard]] bool isSignedType(const std::uint8_t kind) noexcept
        {
            return kind >= TypeI8 && kind <= TypeISize;
        }

        [[nodiscard]] bool isUnsignedType(const std::uint8_t kind) noexcept
        {
            return (kind >= TypeU8 && kind <= TypeUSize) || kind == TypeByte || kind == TypeChar;
        }

        [[nodiscard]] bool isFloatType(const std::uint8_t kind) noexcept
        {
            return kind == TypeF32 || kind == TypeF64;
        }

        [[nodiscard]] std::uint32_t integerWidth(const std::uint8_t kind) noexcept
        {
            switch (kind)
            {
            case TypeI8:
            case TypeU8:
            case TypeByte:
                return 8;
            case TypeI16:
            case TypeU16:
                return 16;
            case TypeI32:
            case TypeU32:
            case TypeChar:
                return 32;
            default:
                return 64;
            }
        }

        [[nodiscard]] std::uint64_t lowBits(const std::uint64_t value, const std::uint32_t width) noexcept
        {
            return width == 64 ? value : value & ((std::uint64_t{1} << width) - 1);
        }

        [[nodiscard]] std::int64_t signExtend(const std::uint64_t value, const std::uint32_t width) noexcept
        {
            if (width == 64)
                return std::bit_cast<std::int64_t>(value);
            const std::uint64_t mask = (std::uint64_t{1} << width) - 1;
            const std::uint64_t sign = std::uint64_t{1} << (width - 1);
            const std::uint64_t narrowed = value & mask;
            return std::bit_cast<std::int64_t>((narrowed & sign) != 0 ? narrowed | ~mask : narrowed);
        }

        [[nodiscard]] std::uint32_t registerCount(const bytecode::Function& function) noexcept
        {
            std::uint32_t maximum = 0;
            bool found = false;
            const auto observe = [&](const std::uint32_t value)
            {
                if (value == bytecode::InvalidIndex)
                    return;
                found = true;
                maximum = (std::max)(maximum, value);
            };
            for (const bytecode::Parameter& parameter : function.parameters)
                observe(parameter.value);
            for (const bytecode::Block& block : function.blocks)
            {
                for (const bytecode::Parameter& parameter : block.parameters)
                    observe(parameter.value);
                for (const bytecode::Instruction& instruction : block.instructions)
                    observe(instruction.result);
            }
            return found && maximum != (std::numeric_limits<std::uint32_t>::max)() ? maximum + 1 : 0;
        }

        [[nodiscard]] std::optional<Value> constantValue(const bytecode::Constant& constant,
                                                         const bytecode::Module& module, const std::uint8_t resultType)
        {
            switch (constant.kind)
            {
            case bytecode::ConstantKind::Empty:
                return {};
            case bytecode::ConstantKind::Null:
                return Value::null();
            case bytecode::ConstantKind::Boolean:
                return Value::boolean(constant.bits != 0);
            case bytecode::ConstantKind::SignedInteger:
                return Value::signedInteger(std::bit_cast<std::int64_t>(constant.bits));
            case bytecode::ConstantKind::UnsignedInteger:
                return Value::unsignedInteger(constant.bits);
            case bytecode::ConstantKind::Float64:
                return Value::floating(std::bit_cast<double>(constant.bits));
            case bytecode::ConstantKind::String:
                if (resultType == TypeText)
                {
                    Utf8DecodeResult decoded = decodeUtf8(module.string(constant.string));
                    if (!decoded.succeeded())
                        return std::nullopt;
                    return Value::text(std::move(decoded.value));
                }
                return Value::string(std::string{module.string(constant.string)});
            }
            return std::nullopt;
        }

        [[nodiscard]] Value defaultValue(const bytecode::Module& module, const std::uint32_t typeId,
                                         const std::uint32_t depth = 0)
        {
            if (typeId >= module.types.size() || depth > 128)
                return {};
            const bytecode::Type& type = module.types[typeId];
            const std::uint8_t kind = type.kind;
            if (kind == TypeBool)
                return Value::boolean(false);
            if (isSignedType(kind))
                return Value::signedInteger(0);
            if (isUnsignedType(kind))
                return Value::unsignedInteger(0);
            if (isFloatType(kind))
                return Value::floating(0.0);
            if (kind == TypeString)
                return Value::string({});
            if (kind == TypeText)
                return Value::text({});
            if (kind == TypeNullable)
                return Value::null();
            if (kind == TypeArray)
            {
                std::vector<Value> elements;
                if (type.staticExtent != (std::numeric_limits<std::uint64_t>::max)() && !type.arguments.empty())
                {
                    if (type.staticExtent > (std::numeric_limits<std::size_t>::max)())
                        return {};
                    elements.reserve(static_cast<std::size_t>(type.staticExtent));
                    for (std::uint64_t index = 0; index < type.staticExtent; ++index)
                        elements.push_back(defaultValue(module, type.arguments.front(), depth + 1));
                }
                return Value::array(std::move(elements));
            }
            if (kind == TypeDictionary)
                return Value::dictionary(typeId, {}, module.string(type.name) == "ordered");
            if (kind == TypeNamed && type.nominalKind == NominalComponent)
            {
                std::vector<Value> fields;
                fields.reserve(type.fields.size());
                for (const bytecode::Type::Field& field : type.fields)
                    fields.push_back(defaultValue(module, field.type, depth + 1));
                return Value::component(typeId, std::move(fields));
            }
            if (kind == TypeNamed && (type.nominalKind == NominalObject || type.nominalKind == NominalInterface))
                return Value::null();
            return {};
        }

        [[nodiscard]] Value constructAggregate(const bytecode::Module& module, const std::uint32_t typeId,
                                               const bool object)
        {
            const bytecode::Type& type = module.types[typeId];
            std::vector<Value> fields;
            fields.reserve(type.fields.size());
            for (const bytecode::Type::Field& field : type.fields)
                fields.push_back(defaultValue(module, field.type));
            return object ? Value::object(typeId, std::move(fields)) : Value::component(typeId, std::move(fields));
        }

        [[nodiscard]] bool compareResult(const std::uint8_t operation, const bool less, const bool equal) noexcept
        {
            switch (operation)
            {
            case BinaryEqual:
                return equal;
            case BinaryNotEqual:
                return !equal;
            case BinaryLess:
                return less;
            case BinaryLessEqual:
                return less || equal;
            case BinaryGreater:
                return !less && !equal;
            case BinaryGreaterEqual:
                return !less || equal;
            default:
                return false;
            }
        }

        [[nodiscard]] bool isComparison(const std::uint8_t operation) noexcept
        {
            return operation >= BinaryEqual && operation <= BinaryGreaterEqual;
        }

        [[nodiscard]] std::optional<ExecutorKind> executorKind(const std::uint8_t value) noexcept
        {
            if (value < static_cast<std::uint8_t>(ExecutorKind::Main) ||
                value > static_cast<std::uint8_t>(ExecutorKind::Io))
                return std::nullopt;
            return static_cast<ExecutorKind>(value);
        }

        [[nodiscard]] std::optional<std::size_t> valueIndex(const Value& value) noexcept
        {
            if (value.kind() == Value::Kind::UnsignedInteger &&
                value.asUnsignedInteger() <= (std::numeric_limits<std::size_t>::max)())
                return static_cast<std::size_t>(value.asUnsignedInteger());
            if (value.kind() == Value::Kind::SignedInteger && value.asSignedInteger() >= 0 &&
                static_cast<std::uint64_t>(value.asSignedInteger()) <=
                    static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
                return static_cast<std::size_t>(value.asSignedInteger());
            return std::nullopt;
        }

        [[nodiscard]] std::optional<std::string> formatUtf8(const Value& value)
        {
            if (value.kind() == Value::Kind::String)
                return std::string{value.asString()};
            if (value.kind() == Value::Kind::Text)
                return encodeUtf8(value.asText());
            if (value.kind() == Value::Kind::Null)
                return "null";
            if (value.kind() == Value::Kind::Boolean)
                return value.asBoolean() ? "true" : "false";

            char buffer[128]{};
            std::to_chars_result converted{};
            if (value.kind() == Value::Kind::SignedInteger)
                converted = std::to_chars(std::begin(buffer), std::end(buffer), value.asSignedInteger());
            else if (value.kind() == Value::Kind::UnsignedInteger)
                converted = std::to_chars(std::begin(buffer), std::end(buffer), value.asUnsignedInteger());
            else if (value.kind() == Value::Kind::Float64)
                converted = std::to_chars(std::begin(buffer), std::end(buffer), value.asFloat64());
            else
                return std::nullopt;
            if (converted.ec != std::errc{})
                return std::nullopt;
            return std::string{buffer, converted.ptr};
        }

        class TimerScheduler final
        {
        public:
            using Callback = std::function<void()>;

            TimerScheduler() : worker_([this] { run(); })
            {
            }

            ~TimerScheduler()
            {
                {
                    std::scoped_lock lock{mutex_};
                    stopping_ = true;
                }
                changed_.notify_all();
                if (worker_.joinable())
                    worker_.join();
            }

            TimerScheduler(const TimerScheduler&) = delete;
            TimerScheduler& operator=(const TimerScheduler&) = delete;

            void schedule(Value task, const std::chrono::steady_clock::time_point deadline, Callback fire,
                          Callback cancel)
            {
                {
                    std::scoped_lock lock{mutex_};
                    if (!stopping_)
                    {
                        timers_.emplace(std::make_pair(deadline, nextSequence_++),
                                        Timer{std::move(task), std::move(fire), std::move(cancel)});
                        changed_.notify_all();
                        return;
                    }
                }
                cancel();
            }

            bool remove(const Value& task)
            {
                std::scoped_lock lock{mutex_};
                const auto timer = std::find_if(timers_.begin(), timers_.end(),
                                                [&](const auto& entry) { return entry.second.task == task; });
                if (timer == timers_.end())
                    return false;
                timers_.erase(timer);
                changed_.notify_all();
                return true;
            }

        private:
            struct Timer
            {
                Value task;
                Callback fire;
                Callback cancel;
            };

            void run()
            {
                std::unique_lock lock{mutex_};
                while (!stopping_)
                {
                    if (timers_.empty())
                    {
                        changed_.wait(lock, [&] { return stopping_ || !timers_.empty(); });
                        continue;
                    }

                    const auto deadline = timers_.begin()->first.first;
                    if (std::chrono::steady_clock::now() < deadline)
                    {
                        changed_.wait_until(lock, deadline);
                        continue;
                    }

                    Callback fire = std::move(timers_.begin()->second.fire);
                    timers_.erase(timers_.begin());
                    lock.unlock();
                    fire();
                    lock.lock();
                }

                std::vector<Callback> cancellations;
                cancellations.reserve(timers_.size());
                for (auto& entry : timers_)
                    cancellations.push_back(std::move(entry.second.cancel));
                timers_.clear();
                lock.unlock();
                for (Callback& cancel : cancellations)
                    cancel();
            }

            std::mutex mutex_;
            std::condition_variable changed_;
            std::map<std::pair<std::chrono::steady_clock::time_point, std::uint64_t>, Timer> timers_;
            std::uint64_t nextSequence_ = 0;
            bool stopping_ = false;
            std::thread worker_;
        };

        class ExecutorQueue final
        {
        public:
            using Callback = std::function<void()>;

            struct Work
            {
                Callback run;
                Callback cancel;
            };

            ExecutorQueue(const ExecutorKind kind, const std::uint32_t threadCount) : kind_(kind)
            {
                workers_.reserve(threadCount);
                for (std::uint32_t index = 0; index < threadCount; ++index)
                    workers_.emplace_back([this] { workerLoop(); });
            }

            ~ExecutorQueue()
            {
                std::deque<Work> cancelled;
                {
                    std::scoped_lock lock{mutex_};
                    stopping_ = true;
                    cancelled.swap(queue_);
                }
                changed_.notify_all();
                for (std::thread& worker : workers_)
                    if (worker.joinable())
                        worker.join();
                for (Work& work : cancelled)
                    if (work.cancel)
                        work.cancel();
            }

            ExecutorQueue(const ExecutorQueue&) = delete;
            ExecutorQueue& operator=(const ExecutorQueue&) = delete;

            bool schedule(Work work)
            {
                {
                    std::scoped_lock lock{mutex_};
                    if (stopping_)
                        return false;
                    queue_.push_back(std::move(work));
                }
                changed_.notify_one();
                return true;
            }

        private:
            void workerLoop()
            {
                const ExecutorKind previous = activeExecutor;
                activeExecutor = kind_;
                for (;;)
                {
                    Work work;
                    {
                        std::unique_lock lock{mutex_};
                        changed_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
                        if (stopping_)
                            break;
                        work = std::move(queue_.front());
                        queue_.pop_front();
                    }
                    try
                    {
                        work.run();
                    }
                    catch (...)
                    {
                        if (work.cancel)
                            work.cancel();
                    }
                }
                activeExecutor = previous;
            }

            ExecutorKind kind_;
            std::mutex mutex_;
            std::condition_variable changed_;
            std::deque<Work> queue_;
            std::vector<std::thread> workers_;
            bool stopping_ = false;
        };

        class ExecutorScheduler final
        {
        public:
            using Work = ExecutorQueue::Work;

            ExecutorScheduler(std::uint32_t workerThreads, const std::uint32_t blockingThreads,
                              const std::uint32_t ioThreads)
                : workerThreads_(workerThreads == 0 ? defaultWorkerCount() : workerThreads),
                  blockingThreads_((std::max)(std::uint32_t{1}, blockingThreads)),
                  ioThreads_((std::max)(std::uint32_t{1}, ioThreads)), mainThread_(std::this_thread::get_id())
            {
            }

            ~ExecutorScheduler()
            {
                std::deque<Work> cancelled;
                {
                    std::scoped_lock lock{mutex_};
                    stopping_ = true;
                    cancelled.swap(mainQueue_);
                }
                for (Work& work : cancelled)
                    if (work.cancel)
                        work.cancel();
            }

            ExecutorScheduler(const ExecutorScheduler&) = delete;
            ExecutorScheduler& operator=(const ExecutorScheduler&) = delete;

            void bindMain()
            {
                std::scoped_lock lock{mutex_};
                mainThread_ = std::this_thread::get_id();
            }

            [[nodiscard]] bool isMainThread() const
            {
                std::scoped_lock lock{mutex_};
                return mainThread_ == std::this_thread::get_id();
            }

            bool schedule(const ExecutorKind kind, Work work)
            {
                if (kind == ExecutorKind::Main)
                {
                    std::scoped_lock lock{mutex_};
                    if (stopping_)
                        return false;
                    mainQueue_.push_back(std::move(work));
                    return true;
                }

                ExecutorQueue* queue = nullptr;
                {
                    std::scoped_lock lock{mutex_};
                    if (stopping_)
                        return false;
                    std::unique_ptr<ExecutorQueue>* target = nullptr;
                    std::uint32_t count = 1;
                    if (kind == ExecutorKind::Worker)
                    {
                        target = &worker_;
                        count = workerThreads_;
                    }
                    else if (kind == ExecutorKind::Blocking)
                    {
                        target = &blocking_;
                        count = blockingThreads_;
                    }
                    else if (kind == ExecutorKind::Io)
                    {
                        target = &io_;
                        count = ioThreads_;
                    }
                    if (!target)
                        return false;
                    if (!*target)
                        *target = std::make_unique<ExecutorQueue>(kind, count);
                    queue = target->get();
                }
                return queue->schedule(std::move(work));
            }

            std::uint64_t drainMain()
            {
                if (!isMainThread())
                    return 0;
                const ExecutorKind previous = activeExecutor;
                activeExecutor = ExecutorKind::Main;
                std::uint64_t count = 0;
                for (;;)
                {
                    Work work;
                    {
                        std::scoped_lock lock{mutex_};
                        if (mainQueue_.empty())
                            break;
                        work = std::move(mainQueue_.front());
                        mainQueue_.pop_front();
                    }
                    try
                    {
                        work.run();
                    }
                    catch (...)
                    {
                        if (work.cancel)
                            work.cancel();
                    }
                    ++count;
                }
                activeExecutor = previous;
                return count;
            }

        private:
            [[nodiscard]] static std::uint32_t defaultWorkerCount() noexcept
            {
                const std::uint32_t hardware = std::thread::hardware_concurrency();
                return hardware > 1 ? hardware - 1 : 1;
            }

            mutable std::mutex mutex_;
            std::deque<Work> mainQueue_;
            std::unique_ptr<ExecutorQueue> worker_;
            std::unique_ptr<ExecutorQueue> blocking_;
            std::unique_ptr<ExecutorQueue> io_;
            std::uint32_t workerThreads_;
            std::uint32_t blockingThreads_;
            std::uint32_t ioThreads_;
            std::thread::id mainThread_;
            bool stopping_ = false;
        };
    } // namespace

    struct Machine::Program
    {
        struct FunctionPlan
        {
            std::uint32_t registerCount = 0;
            std::unordered_map<std::uint32_t, const bytecode::Block*> blocks;
        };

        [[nodiscard]] Machine* acquireOwner()
        {
            std::scoped_lock lock{ownerMutex};
            if (!owner)
                return nullptr;
            ++activeOwnerCalls;
            return owner;
        }

        void releaseOwner() noexcept
        {
            std::scoped_lock lock{ownerMutex};
            if (--activeOwnerCalls == 0)
                ownerChanged.notify_all();
        }

        void detachOwner(Machine* expected)
        {
            std::unique_lock lock{ownerMutex};
            if (owner == expected)
                owner = nullptr;
            ownerChanged.wait(lock, [&] { return activeOwnerCalls == 0; });
        }

        void retainContinuation(const Value& task, std::shared_ptr<void> continuation)
        {
            std::scoped_lock lock{continuationMutex};
            const auto existing = std::find_if(continuations.begin(), continuations.end(),
                                               [&](const auto& entry) { return entry.first == task; });
            if (existing != continuations.end())
                existing->second = std::move(continuation);
            else
                continuations.emplace_back(task, std::move(continuation));
        }

        void releaseContinuation(const Value& task)
        {
            std::scoped_lock lock{continuationMutex};
            std::erase_if(continuations, [&](const auto& entry) { return entry.first == task; });
        }

        [[nodiscard]] std::shared_ptr<void> takeContinuation(const Value& task)
        {
            std::scoped_lock lock{continuationMutex};
            const auto found = std::find_if(continuations.begin(), continuations.end(),
                                            [&](const auto& entry) { return entry.first == task; });
            if (found == continuations.end())
                return {};
            std::shared_ptr<void> continuation = std::move(found->second);
            continuations.erase(found);
            return continuation;
        }

        [[nodiscard]] std::vector<std::pair<Value, std::shared_ptr<void>>> takeContinuations()
        {
            std::scoped_lock lock{continuationMutex};
            std::vector<std::pair<Value, std::shared_ptr<void>>> result;
            result.swap(continuations);
            return result;
        }

        std::mutex ownerMutex;
        std::condition_variable ownerChanged;
        Machine* owner = nullptr;
        std::size_t activeOwnerCalls = 0;
        const bytecode::Module* module = nullptr;
        ExecutionError loadError;
        std::vector<FunctionPlan> functions;
        std::vector<std::unique_ptr<PlaceStorage>> globals;
        std::mutex timerMutex;
        std::unique_ptr<TimerScheduler> timers;
        std::mutex continuationMutex;
        std::vector<std::pair<Value, std::shared_ptr<void>>> continuations;
        std::recursive_mutex executionMutex;
        std::unique_ptr<ExecutorScheduler> executors;
        bool valid = false;
    };

    struct Machine::ExecutionState
    {
        std::vector<Frame> frames;
        Value activeTask;
        ExecutorKind executor = ExecutorKind::Inherit;
        std::uint64_t executed = 0;
        std::uint32_t awaitedState = bytecode::InvalidIndex;
        std::uint32_t awaitedResumeBlock = bytecode::InvalidIndex;
    };

    ExecutionResult ExecutionResult::success(Value value)
    {
        ExecutionResult result;
        result.succeeded_ = true;
        result.value_ = std::move(value);
        return result;
    }

    ExecutionResult ExecutionResult::failure(ExecutionError error)
    {
        ExecutionResult result;
        result.error_ = std::move(error);
        return result;
    }

    ExecutionResult ExecutionResult::suspended()
    {
        ExecutionResult result;
        result.suspended_ = true;
        return result;
    }

    void Machine::cleanupValue(Value& value, ExecutionError& primaryError)
    {
        if (value.isEmpty() || value.kind() == Value::Kind::Null || value.kind() == Value::Kind::Place ||
            value.kind() == Value::Kind::ObjectBorrow)
        {
            value = {};
            return;
        }

        const bool uniqueStorage = value.strongReferenceCount() == 1;
        if (program_ && program_->module && uniqueStorage &&
            (value.kind() == Value::Kind::Object || value.kind() == Value::Kind::Component))
        {
            const bytecode::Module& module = *program_->module;
            const std::uint32_t type = value.aggregateType();
            const std::uint32_t destructor =
                type < module.types.size() ? module.types[type].destructor : bytecode::InvalidIndex;
            if (destructor != bytecode::InvalidIndex && destructor < module.functions.size())
            {
                try
                {
                    Value receiver;
                    std::unique_ptr<PlaceStorage> componentPlace;
                    if (value.kind() == Value::Kind::Object)
                        receiver = Value::objectBorrow(value.scalar_.aggregate);
                    else
                    {
                        componentPlace = std::make_unique<PlaceStorage>();
                        componentPlace->alias = &value;
                        componentPlace->initialized = true;
                        componentPlace->mutableValue = true;
                        receiver = Value::place(componentPlace.get());
                    }

                    const ExecutionResult destroyed = execute(destructor, std::span{&receiver, 1}, nullptr);
                    if (!destroyed.succeeded())
                    {
                        primaryError.cleanupFailures.push_back(
                            {destroyed.error().code, destroyed.error().message, destroyed.error().stack});
                        primaryError.cleanupFailures.insert(primaryError.cleanupFailures.end(),
                                                            destroyed.error().cleanupFailures.begin(),
                                                            destroyed.error().cleanupFailures.end());
                    }
                }
                catch (const std::exception& error)
                {
                    primaryError.cleanupFailures.push_back(
                        {"WVM1210", std::string{"VM destructor threw while unwinding: "} + error.what(), {}});
                }
                catch (...)
                {
                    primaryError.cleanupFailures.push_back(
                        {"WVM1210", "VM destructor threw a non-standard exception while unwinding", {}});
                }
            }
        }

        std::vector<Value> children = value.takeOwnedChildrenForCleanup();
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            cleanupValue(*child, primaryError);
        value = {};
    }

    void Machine::cleanupTaskArguments(const Value& task, ExecutionError& primaryError)
    {
        std::vector<Value> arguments = task.takeTaskArgumentsForCleanup();
        for (auto argument = arguments.rbegin(); argument != arguments.rend(); ++argument)
            cleanupValue(*argument, primaryError);
    }

    ExecutionError Machine::unwind(ExecutionState& state, ExecutionError error)
    {
        error = withStack(std::move(error), state.frames);
        for (auto frame = state.frames.rbegin(); frame != state.frames.rend(); ++frame)
        {
            cleanupValue(frame->resumedValue, error);
            for (auto argument = frame->continuation.nextArguments.rbegin();
                 argument != frame->continuation.nextArguments.rend(); ++argument)
                cleanupValue(*argument, error);
            for (auto place = frame->localPlaces.rbegin(); place != frame->localPlaces.rend(); ++place)
            {
                PlaceStorage& storage = **place;
                if (!storage.alias && storage.initialized)
                {
                    cleanupValue(storage.value, error);
                    storage.initialized = false;
                }
                cleanupValue(storage.owner, error);
            }
            for (auto registerValue = frame->registers.rbegin(); registerValue != frame->registers.rend();
                 ++registerValue)
            {
                if (!registerValue->initialized)
                    continue;
                cleanupValue(registerValue->value, error);
                registerValue->initialized = false;
            }
        }
        state.frames.clear();
        return error;
    }

    void Machine::cancelAndUnwindContinuations()
    {
        if (!program_)
            return;
        std::vector<std::pair<Value, std::shared_ptr<void>>> continuations = program_->takeContinuations();
        std::scoped_lock executionLock{program_->executionMutex};
        for (auto& [task, erased] : continuations)
        {
            (void)task.cancelTask();
            auto state = std::static_pointer_cast<ExecutionState>(std::move(erased));
            if (state)
                (void)unwind(*state, {"WVM1165", "Async task was cancelled during machine shutdown"});
            ExecutionError cleanupError;
            cleanupTaskArguments(task, cleanupError);
        }
    }

    void Machine::cleanupGlobals()
    {
        if (!program_)
            return;
        std::scoped_lock executionLock{program_->executionMutex};
        ExecutionError cleanupError;
        for (auto global = program_->globals.rbegin(); global != program_->globals.rend(); ++global)
        {
            if (!(*global)->initialized)
                continue;
            cleanupValue((*global)->value, cleanupError);
            (*global)->initialized = false;
        }
    }

    Machine::Machine(const bytecode::Module& module, const MachineOptions options)
        : program_(std::make_shared<Program>()), options_(options)
    {
        program_->owner = this;
        program_->module = &module;
        program_->executors = std::make_unique<ExecutorScheduler>(options.workerThreadCount,
                                                                  options.blockingThreadCount, options.ioThreadCount);
        const bytecode::VerificationResult verification = bytecode::Verifier{}.verify(module);
        if (!verification.succeeded())
        {
            const bytecode::VerificationDiagnostic& diagnostic = verification.diagnostics().front();
            program_->loadError = {"WVM1001",
                                   "Bytecode verification failed (" + diagnostic.code + "): " + diagnostic.message,
                                   diagnostic.function, diagnostic.block, diagnostic.instruction};
            return;
        }

        program_->functions.resize(module.functions.size());
        for (const bytecode::Function& function : module.functions)
        {
            Program::FunctionPlan& plan = program_->functions[function.id];
            plan.registerCount = registerCount(function);
            plan.blocks.reserve(function.blocks.size());
            for (const bytecode::Block& block : function.blocks)
                plan.blocks.emplace(block.id, &block);
        }
        program_->globals.reserve(module.globals.size());
        for (const bytecode::Global& global : module.globals)
        {
            auto storage = std::make_unique<PlaceStorage>();
            storage->value = defaultValue(module, global.type);
            storage->initialized = !storage->value.isEmpty();
            storage->mutableValue = (global.flags & 0x01u) != 0;
            program_->globals.push_back(std::move(storage));
        }
        program_->valid = true;
    }

    Machine::~Machine()
    {
        if (program_)
        {
            program_->detachOwner(this);
            cancelAndUnwindContinuations();
            cleanupGlobals();
        }
        program_.reset();
    }

    Machine::Machine(Machine&& other) noexcept
        : program_(std::move(other.program_)), options_(std::move(other.options_))
    {
        if (program_)
        {
            program_->detachOwner(&other);
            std::scoped_lock lock{program_->ownerMutex};
            program_->owner = this;
        }
    }

    Machine& Machine::operator=(Machine&& other) noexcept
    {
        if (this == &other)
            return *this;
        if (program_)
        {
            program_->detachOwner(this);
            cancelAndUnwindContinuations();
            cleanupGlobals();
        }
        program_.reset();
        program_ = std::move(other.program_);
        options_ = std::move(other.options_);
        if (program_)
        {
            program_->detachOwner(&other);
            std::scoped_lock lock{program_->ownerMutex};
            program_->owner = this;
        }
        return *this;
    }

    ExecutionResult Machine::invoke(const std::uint32_t functionId, const std::span<const Value> arguments)
    {
        if (!program_ || !program_->module)
            return ExecutionResult::failure({"WVM1000", "The virtual machine has no loaded module"});
        if (!program_->valid)
            return ExecutionResult::failure(program_->loadError);
        if (functionId >= program_->module->functions.size())
            return ExecutionResult::failure({"WVM1002", "Entry function id is outside the module"});
        if ((program_->module->functions[functionId].flags & 0x0001u) != 0)
            return ExecutionResult::success(startTask(functionId, arguments));
        return execute(functionId, arguments, nullptr);
    }

    ExecutionResult Machine::wait(const Value& task)
    {
        ExecutionResult result = driveTask(task);
        if (!result.isSuspended())
            return result;

        while (task.taskState() == AsyncTaskState::Pending || task.taskState() == AsyncTaskState::Running)
        {
            const std::uint64_t drained = program_ && program_->executors ? program_->executors->drainMain() : 0;
            if (drained == 0)
                (void)task.waitTaskFor(std::chrono::milliseconds{1});
        }
        if (task.taskState() == AsyncTaskState::Ready)
            return ExecutionResult::success(task.taskResult());
        if (task.taskState() == AsyncTaskState::Cancelled)
            return ExecutionResult::failure({"WVM1165", "Async task was cancelled"});
        return ExecutionResult::failure(task.taskError());
    }

    void Machine::bindMainExecutor()
    {
        if (program_ && program_->executors)
            program_->executors->bindMain();
    }

    std::uint64_t Machine::drainMainExecutor()
    {
        return program_ && program_->executors ? program_->executors->drainMain() : 0;
    }

    bool Machine::cancel(const Value& task)
    {
        if (!task.cancelTask())
            return false;
        if (program_)
        {
            auto state = std::static_pointer_cast<ExecutionState>(program_->takeContinuation(task));
            if (state)
            {
                std::scoped_lock executionLock{program_->executionMutex};
                (void)unwind(*state, {"WVM1165", "Async task was cancelled"});
            }
            ExecutionError cleanupError;
            cleanupTaskArguments(task, cleanupError);
            std::scoped_lock lock{program_->timerMutex};
            if (program_->timers)
                (void)program_->timers->remove(task);
        }
        return true;
    }

    Value Machine::makeExternalTask()
    {
        return Value::externalTask();
    }

    bool Machine::complete(const Value& task, Value result)
    {
        if (!task.completeTask(std::move(result)))
            return false;
        if (program_)
        {
            ExecutionError cleanupError;
            cleanupTaskArguments(task, cleanupError);
            program_->releaseContinuation(task);
            std::scoped_lock lock{program_->timerMutex};
            if (program_->timers)
                (void)program_->timers->remove(task);
        }
        return true;
    }

    bool Machine::fail(const Value& task, std::string code, std::string message)
    {
        if (!task.failTask({std::move(code), std::move(message)}))
            return false;
        if (program_)
        {
            ExecutionError cleanupError;
            cleanupTaskArguments(task, cleanupError);
            program_->releaseContinuation(task);
            std::scoped_lock lock{program_->timerMutex};
            if (program_->timers)
                (void)program_->timers->remove(task);
        }
        return true;
    }

    Value Machine::sleepFor(const std::chrono::nanoseconds duration)
    {
        Value task = Value::externalTask();
        if (!program_ || !program_->valid)
        {
            (void)task.failTask({"WVM1000", "Timer scheduling requires a loaded VM program"});
            return task;
        }
        if (duration <= std::chrono::nanoseconds::zero())
        {
            task.completeTask({});
            return task;
        }

        TimerScheduler* scheduler = nullptr;
        {
            std::scoped_lock lock{program_->timerMutex};
            if (!program_->timers)
                program_->timers = std::make_unique<TimerScheduler>();
            scheduler = program_->timers.get();
        }
        scheduler->schedule(
            task, std::chrono::steady_clock::now() + duration, [task] { task.completeTask({}); },
            [task] { task.cancelTask(); });
        return task;
    }

    Value Machine::startTask(const std::uint32_t functionId, const std::span<const Value> arguments)
    {
        std::vector<Value> captured;
        captured.reserve(arguments.size());
        const bytecode::Function& function = program_->module->functions[functionId];
        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const bool retainedReceiver = function.hasCoroutine && index < function.parameters.size() &&
                                          function.coroutine.retainedReceiver == function.parameters[index].value;
            if (retainedReceiver && arguments[index].kind() == Value::Kind::ObjectBorrow)
                captured.push_back(arguments[index].retainObject());
            else
                captured.push_back(arguments[index]);
        }
        Value task = Value::asyncTask(functionId, std::move(captured));
        (void)driveTask(task);
        return task;
    }

    ExecutionResult Machine::driveTask(const Value& task)
    {
        if (task.kind() != Value::Kind::AsyncTask)
            return ExecutionResult::failure({"WVM1164", "Wait requires an async task value"});
        const auto releaseContinuation = [&]
        {
            if (program_)
                program_->releaseContinuation(task);
        };
        if (task.taskState() == AsyncTaskState::Ready)
        {
            releaseContinuation();
            return ExecutionResult::success(task.taskResult());
        }
        if (task.taskState() == AsyncTaskState::Cancelled)
        {
            releaseContinuation();
            return ExecutionResult::failure({"WVM1165", "Async task was cancelled"});
        }
        if (task.taskState() == AsyncTaskState::Faulted)
        {
            releaseContinuation();
            return ExecutionResult::failure(task.taskError());
        }
        if (task.taskState() == AsyncTaskState::Running)
            return ExecutionResult::suspended();
        if (task.taskFunction() == bytecode::InvalidIndex)
        {
            task.waitTask();
            if (task.taskState() == AsyncTaskState::Ready)
                return ExecutionResult::success(task.taskResult());
            if (task.taskState() == AsyncTaskState::Cancelled)
                return ExecutionResult::failure({"WVM1165", "Async task was cancelled"});
            return ExecutionResult::failure(task.taskError());
        }
        if (!task.beginTask())
            return ExecutionResult::failure({"WVM1166", "Async task is already running or has an invalid state"});

        std::vector<Value> arguments;
        arguments.reserve(task.taskArgumentCount());
        for (std::size_t index = 0; index < task.taskArgumentCount(); ++index)
        {
            const Value* argument = task.taskArgument(index);
            if (!argument)
            {
                (void)task.failTask({"WVM1167", "Async task lost a captured argument"});
                releaseContinuation();
                return ExecutionResult::failure({"WVM1167", "Async task lost a captured argument"});
            }
            arguments.push_back(*argument);
        }

        ExecutionResult result = execute(task.taskFunction(), arguments, &task);
        if (result.isSuspended())
            return result;
        arguments.clear();
        if (!result.succeeded())
        {
            if (task.taskState() == AsyncTaskState::Cancelled)
            {
                releaseContinuation();
                return ExecutionResult::failure({"WVM1165", "Async task was cancelled"});
            }
            ExecutionError failure = result.error();
            cleanupTaskArguments(task, failure);
            if (task.failTask(failure))
            {
                releaseContinuation();
                return ExecutionResult::failure(std::move(failure));
            }
            if (task.taskState() == AsyncTaskState::Cancelled)
            {
                releaseContinuation();
                return ExecutionResult::failure({"WVM1165", "Async task was cancelled"});
            }
            releaseContinuation();
            return ExecutionResult::failure({"WVM1204", "Async task failure lost its terminal-state race"});
        }
        ExecutionError cleanupError;
        cleanupTaskArguments(task, cleanupError);
        if (!cleanupError.cleanupFailures.empty())
        {
            ExecutionError failure{"WVM1212", "Async argument cleanup failed while running a destructor"};
            failure.cleanupFailures = std::move(cleanupError.cleanupFailures);
            (void)task.failTask(failure);
            releaseContinuation();
            return ExecutionResult::failure(std::move(failure));
        }
        if (!task.completeTask(result.value()))
        {
            if (task.taskState() == AsyncTaskState::Cancelled)
            {
                releaseContinuation();
                return ExecutionResult::failure({"WVM1165", "Async task was cancelled"});
            }
            releaseContinuation();
            return ExecutionResult::failure({"WVM1204", "Async task completion lost its terminal-state race"});
        }
        releaseContinuation();
        return ExecutionResult::success(task.taskResult());
    }

    bool Machine::scheduleTask(const std::shared_ptr<ExecutionState>& state, ExecutorKind executor)
    {
        if (!program_ || !program_->executors)
            return false;
        if (executor == ExecutorKind::Inherit)
            executor = activeExecutor != ExecutorKind::Inherit
                           ? activeExecutor
                           : (program_->executors->isMainThread() ? ExecutorKind::Main : ExecutorKind::Worker);
        state->executor = executor;
        program_->retainContinuation(state->activeTask, state);
        const std::weak_ptr<Program> weakProgram = program_;
        const Value task = state->activeTask;
        const bool scheduled = program_->executors->schedule(
            executor, ExecutorQueue::Work{.run =
                                              [weakProgram, state]
                                          {
                                              if (const std::shared_ptr<Program> program = weakProgram.lock())
                                              {
                                                  Machine* const owner = program->acquireOwner();
                                                  if (!owner)
                                                      return;
                                                  try
                                                  {
                                                      owner->resumeTask(state);
                                                  }
                                                  catch (...)
                                                  {
                                                      try
                                                      {
                                                          (void)state->activeTask.failTask(
                                                              {"WVM1207", "VM executor owner dispatch failed"});
                                                      }
                                                      catch (...)
                                                      {
                                                      }
                                                  }
                                                  program->releaseOwner();
                                              }
                                          },
                                          .cancel = [task] { (void)task.cancelTask(); }});
        if (!scheduled)
            program_->releaseContinuation(state->activeTask);
        return scheduled;
    }

    void Machine::wakeTask(const std::weak_ptr<ExecutionState> weakState, const AsyncTaskState terminalState,
                           const Value& result, const ExecutionError& error)
    {
        const std::shared_ptr<ExecutionState> state = weakState.lock();
        if (!state || state->activeTask.taskState() == AsyncTaskState::Cancelled || !program_)
            return;

        if (terminalState != AsyncTaskState::Ready)
        {
            ExecutionError propagated = terminalState == AsyncTaskState::Cancelled
                                            ? ExecutionError{"WVM1165", "Awaited async task was cancelled"}
                                            : error;
            {
                std::scoped_lock lock{program_->executionMutex};
                propagated = unwind(*state, std::move(propagated));
            }
            program_->releaseContinuation(state->activeTask);
            cleanupTaskArguments(state->activeTask, propagated);
            (void)state->activeTask.failTask(std::move(propagated));
            return;
        }

        ExecutorKind executor = ExecutorKind::Inherit;
        {
            std::scoped_lock lock{program_->executionMutex};
            if (state->frames.empty())
            {
                ExecutionError failure{"WVM1209", "Await continuation lost its VM frame"};
                program_->releaseContinuation(state->activeTask);
                cleanupTaskArguments(state->activeTask, failure);
                (void)state->activeTask.failTask(std::move(failure));
                return;
            }
            Frame& frame = state->frames.back();
            const auto resume = program_->functions[frame.function->id].blocks.find(state->awaitedResumeBlock);
            if (resume == program_->functions[frame.function->id].blocks.end())
            {
                ExecutionError failure = unwind(*state, {"WVM1175", "Coroutine resume block does not exist"});
                program_->releaseContinuation(state->activeTask);
                cleanupTaskArguments(state->activeTask, failure);
                (void)state->activeTask.failTask(std::move(failure));
                return;
            }
            frame.resumedValue = result;
            frame.hasResumedValue = true;
            frame.resumedState = state->awaitedState;
            frame.block = resume->second;
            frame.instruction = 0;
            state->awaitedState = bytecode::InvalidIndex;
            state->awaitedResumeBlock = bytecode::InvalidIndex;
            executor = state->executor;
        }
        if (!scheduleTask(state, executor))
        {
            ExecutionError failure;
            {
                std::scoped_lock lock{program_->executionMutex};
                failure = unwind(*state, {"WVM1208", "Await continuation executor rejected the resumed VM frame"});
            }
            program_->releaseContinuation(state->activeTask);
            cleanupTaskArguments(state->activeTask, failure);
            (void)state->activeTask.failTask(std::move(failure));
        }
    }

    void Machine::resumeTask(std::shared_ptr<ExecutionState> state)
    {
        if (state->activeTask.taskState() == AsyncTaskState::Cancelled)
        {
            if (program_)
            {
                std::scoped_lock lock{program_->executionMutex};
                (void)unwind(*state, {"WVM1165", "Async task was cancelled before its continuation resumed"});
                program_->releaseContinuation(state->activeTask);
                ExecutionError cleanupError;
                cleanupTaskArguments(state->activeTask, cleanupError);
            }
            return;
        }
        try
        {
            ExecutionResult result = execute(bytecode::InvalidIndex, {}, nullptr, state);
            if (result.isSuspended())
                return;

            const Value& task = state->activeTask;
            if (!result.succeeded())
            {
                program_->releaseContinuation(task);
                if (task.taskState() != AsyncTaskState::Cancelled)
                {
                    ExecutionError failure = result.error();
                    cleanupTaskArguments(task, failure);
                    (void)task.failTask(std::move(failure));
                }
                return;
            }
            program_->releaseContinuation(task);
            if (task.taskState() != AsyncTaskState::Cancelled)
            {
                ExecutionError cleanupError;
                cleanupTaskArguments(task, cleanupError);
                if (cleanupError.cleanupFailures.empty())
                    (void)task.completeTask(result.value());
                else
                {
                    ExecutionError failure{"WVM1212", "Async argument cleanup failed while running a destructor"};
                    failure.cleanupFailures = std::move(cleanupError.cleanupFailures);
                    (void)task.failTask(std::move(failure));
                }
            }
        }
        catch (...)
        {
            if (program_)
            {
                std::scoped_lock lock{program_->executionMutex};
                (void)unwind(*state, {"WVM1207", "VM executor continuation threw across the runtime boundary"});
                program_->releaseContinuation(state->activeTask);
            }
            if (state->activeTask.taskState() != AsyncTaskState::Cancelled)
            {
                ExecutionError failure{"WVM1207", "VM executor continuation threw across the runtime boundary"};
                cleanupTaskArguments(state->activeTask, failure);
                (void)state->activeTask.failTask(std::move(failure));
            }
        }
    }

    ExecutionResult Machine::execute(const std::uint32_t functionId, const std::span<const Value> arguments,
                                     const Value* activeTask, std::shared_ptr<ExecutionState> resumedState)
    {
        if (!program_ || !program_->module)
            return ExecutionResult::failure({"WVM1000", "The virtual machine has no loaded module"});
        if (!program_->valid)
            return ExecutionResult::failure(program_->loadError);
        std::unique_lock executionLock{program_->executionMutex};
        const bytecode::Module& module = *program_->module;
        if (!resumedState && functionId >= module.functions.size())
            return ExecutionResult::failure({"WVM1002", "Entry function id is outside the module"});

        const bool resuming = static_cast<bool>(resumedState);
        std::shared_ptr<ExecutionState> state = resuming ? std::move(resumedState) : std::make_shared<ExecutionState>();
        if (!resuming)
        {
            state->frames.reserve(16);
            if (activeTask)
                state->activeTask = *activeTask;
            state->executor = activeExecutor;
        }
        std::vector<Frame>& frames = state->frames;
        const Value* stateTask = state->activeTask.kind() == Value::Kind::AsyncTask ? &state->activeTask : nullptr;
        const auto failWithStack = [&](ExecutionError error)
        { return ExecutionResult::failure(unwind(*state, std::move(error))); };
        auto pushFrame = [&](const bytecode::Function& function, const std::span<const Value> values,
                             FrameContinuation continuation) -> ExecutionResult
        {
            if ((function.flags & 0x0002u) != 0)
                return failWithStack(
                    {"WVM1003", "External function requires the Sprint 20 native bridge", function.id});
            if (values.size() != function.parameters.size())
                return failWithStack(
                    {"WVM1004", "Call argument count does not match the function signature", function.id});
            if (frames.size() >= options_.callDepthLimit)
                return failWithStack({"WVM1005", "VM call depth limit exceeded", function.id});
            const std::uint32_t count = program_->functions[function.id].registerCount;
            if (count > options_.registerLimitPerFrame)
                return failWithStack({"WVM1006", "Function register limit exceeded", function.id});

            Frame frame;
            frame.function = &function;
            frame.block = &function.blocks.front();
            frame.registers.resize(count);
            frame.continuation = std::move(continuation);
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                const std::uint32_t id = function.parameters[index].value;
                if (index < function.captureParameterCount && index < function.captures.size() &&
                    function.captures[index].kind == CaptureValue)
                {
                    auto capture = std::make_unique<PlaceStorage>();
                    capture->value = values[index].cloneOwned();
                    capture->initialized = true;
                    capture->mutableValue = true;
                    PlaceStorage* const pointer = capture.get();
                    frame.localPlaces.push_back(std::move(capture));
                    frame.registers[id] = Register{Value::place(pointer), true};
                }
                else
                    frame.registers[id] = Register{values[index], true};
            }
            frames.push_back(std::move(frame));
            return ExecutionResult::success();
        };

        if (!resuming)
        {
            ExecutionResult initial = pushFrame(module.functions[functionId], arguments, {});
            if (!initial.succeeded())
                return initial;
        }

        const auto completeFrame = [&](Value returned) -> std::optional<ExecutionResult>
        {
            FrameContinuation continuation = std::move(frames.back().continuation);
            frames.pop_back();
            if (frames.empty())
                return ExecutionResult::success(std::move(returned));

            Frame& caller = frames.back();
            if (continuation.completion == FrameCompletion::ReturnValue &&
                continuation.callerResult != bytecode::InvalidIndex)
            {
                if (continuation.callerResult >= caller.registers.size())
                    return failWithStack(
                        {"WVM1034", "Call result register is invalid", caller.function->id, caller.block->id});
                caller.registers[continuation.callerResult] = Register{std::move(returned), true};
            }
            else if (continuation.completion == FrameCompletion::ContinueConstruction)
            {
                if (continuation.nextFunction >= module.functions.size())
                    return failWithStack({"WVM1074", "Construction continuation references an invalid function",
                                          caller.function->id, caller.block->id});
                ExecutionResult pushed =
                    pushFrame(module.functions[continuation.nextFunction], continuation.nextArguments,
                              FrameContinuation{.completion = FrameCompletion::Ignore});
                if (!pushed.succeeded())
                    return pushed;
            }
            return std::nullopt;
        };

        std::uint64_t& executed = state->executed;
        while (!frames.empty())
        {
            Frame& frame = frames.back();
            if (frame.instruction >= frame.block->instructions.size())
                return failWithStack(
                    {"WVM1007", "Instruction cursor escaped its basic block", frame.function->id, frame.block->id});
            if (executed++ >= options_.instructionLimit)
                return failWithStack({"WVM1008", "VM instruction limit exceeded", frame.function->id, frame.block->id,
                                      static_cast<std::uint32_t>(frame.instruction)});

            const bytecode::Instruction& instruction = frame.block->instructions[frame.instruction];
            if (options_.debugObserver)
            {
                DebugAction action = DebugAction::Abort;
                try
                {
                    action = options_.debugObserver->onInstruction(
                        {frame.function->id, frame.block->id, static_cast<std::uint32_t>(frame.instruction),
                         instruction.opcode, instruction.source, static_cast<std::uint32_t>(frames.size()),
                         state->executor});
                }
                catch (...)
                {
                    return failWithStack({"WVM1206", "VM debug observer threw across the runtime boundary",
                                          frame.function->id, frame.block->id,
                                          static_cast<std::uint32_t>(frame.instruction), instruction.source});
                }
                if (action == DebugAction::Abort)
                    return failWithStack({"WVM1205", "VM execution was aborted by the debug observer",
                                          frame.function->id, frame.block->id,
                                          static_cast<std::uint32_t>(frame.instruction), instruction.source});
            }
            const auto fail = [&](std::string code, std::string message)
            {
                return failWithStack({std::move(code), std::move(message), frame.function->id, frame.block->id,
                                      static_cast<std::uint32_t>(frame.instruction), instruction.source});
            };
            const auto read = [&](const std::uint32_t id) -> const Value*
            {
                return id < frame.registers.size() && frame.registers[id].initialized ? &frame.registers[id].value
                                                                                      : nullptr;
            };
            const auto readRegister = [&](const std::uint32_t id) -> Register*
            { return id < frame.registers.size() && frame.registers[id].initialized ? &frame.registers[id] : nullptr; };
            const auto write = [&](const std::uint32_t id, Value value) -> bool
            {
                if (id >= frame.registers.size())
                    return false;
                frame.registers[id] = Register{std::move(value), true};
                return true;
            };
            const auto storedValue = [](const Value* value) -> const Value*
            {
                if (!value || value->kind() != Value::Kind::Place)
                    return value;
                PlaceStorage* const place = value->asPlace();
                return place && place->initialized ? &place->storedValue() : nullptr;
            };
            const auto objectValue = [&](const Value* value) -> const Value*
            {
                value = storedValue(value);
                return value && (value->kind() == Value::Kind::Object || value->kind() == Value::Kind::ObjectBorrow)
                           ? value
                           : nullptr;
            };
            const auto nominalType = [&](std::uint32_t typeId)
            {
                for (std::uint32_t depth = 0; typeId < module.types.size() && depth < 32; ++depth)
                {
                    const bytecode::Type& type = module.types[typeId];
                    if (type.kind == TypeNamed)
                        return typeId;
                    if ((type.kind != TypeReference && type.kind != TypeNullable) || type.arguments.empty())
                        break;
                    typeId = type.arguments.front();
                }
                return bytecode::InvalidIndex;
            };
            const auto isObjectType = [&](const std::uint32_t concreteType, const std::uint32_t targetType)
            {
                const std::uint32_t target = nominalType(targetType);
                if (concreteType >= module.types.size() || target == bytecode::InvalidIndex)
                    return false;
                const bytecode::Type& concrete = module.types[concreteType];
                return concreteType == target || std::find(concrete.castTypes.begin(), concrete.castTypes.end(),
                                                           target) != concrete.castTypes.end();
            };
            const auto dispatchTarget =
                [&](const Value& receiver, const std::uint32_t contractType, const std::uint32_t slot)
            {
                const std::uint32_t concreteType = receiver.aggregateType();
                const std::uint32_t contract = nominalType(contractType);
                if (!isObjectType(concreteType, contract))
                    return bytecode::InvalidIndex;
                const auto& entries = module.types[concreteType].dispatchEntries;
                const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry)
                                                { return entry.contractType == contract && entry.slot == slot; });
                return found == entries.end() ? bytecode::InvalidIndex : found->implementation;
            };

            if (instruction.opcode == bytecode::Opcode::Constant)
            {
                std::optional<Value> value = constantValue(module.constants[instruction.constant], module,
                                                           module.types[instruction.resultType].kind);
                if (!value)
                    return fail("WVM1049", "Unicode text constant is not valid UTF-8");
                if (!write(instruction.result, std::move(*value)))
                    return fail("WVM1009", "Constant result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DefaultValue)
            {
                const std::uint8_t type = module.types[instruction.resultType].kind;
                Value value = defaultValue(module, instruction.resultType);
                if (value.isEmpty() && type != TypeVoid)
                    return fail("WVM1010", "Default value is not implemented for this bytecode type");
                if (!write(instruction.result, std::move(value)))
                    return fail("WVM1009", "Default-value result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Unary)
            {
                const Value* operand = instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                if (!operand)
                    return fail("WVM1011", "Unary instruction reads an unavailable value");
                Value result;
                if (instruction.unaryOperator == UnaryLogicalNot && operand->kind() == Value::Kind::Boolean)
                    result = Value::boolean(!operand->asBoolean());
                else if (instruction.unaryOperator == UnaryNegate && operand->kind() == Value::Kind::SignedInteger)
                    result = Value::signedInteger(std::bit_cast<std::int64_t>(
                        std::uint64_t{0} - std::bit_cast<std::uint64_t>(operand->asSignedInteger())));
                else if (instruction.unaryOperator == UnaryNegate && operand->kind() == Value::Kind::Float64)
                    result = Value::floating(-operand->asFloat64());
                else if (instruction.unaryOperator == UnaryBitwiseNot && operand->kind() == Value::Kind::SignedInteger)
                    result = Value::signedInteger(~operand->asSignedInteger());
                else if (instruction.unaryOperator == UnaryBitwiseNot &&
                         operand->kind() == Value::Kind::UnsignedInteger)
                    result = Value::unsignedInteger(~operand->asUnsignedInteger());
                else
                    return fail("WVM1012", "Unary operator is invalid for the runtime value kind");
                write(instruction.result, std::move(result));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Binary)
            {
                const Value* left = instruction.operands.size() == 2 ? read(instruction.operands[0]) : nullptr;
                const Value* right = instruction.operands.size() == 2 ? read(instruction.operands[1]) : nullptr;
                if (!left || !right || left->kind() != right->kind())
                    return fail("WVM1013", "Binary instruction requires two available values of the same kind");

                const std::uint8_t operation = instruction.binaryOperator;
                Value result;
                if (left->kind() == Value::Kind::SignedInteger)
                {
                    const std::int64_t a = left->asSignedInteger();
                    const std::int64_t b = right->asSignedInteger();
                    const std::uint64_t ua = std::bit_cast<std::uint64_t>(a);
                    const std::uint64_t ub = std::bit_cast<std::uint64_t>(b);
                    if (isComparison(operation))
                        result = Value::boolean(compareResult(operation, a < b, a == b));
                    else if (operation == BinaryAdd)
                        result = Value::signedInteger(std::bit_cast<std::int64_t>(ua + ub));
                    else if (operation == BinarySubtract)
                        result = Value::signedInteger(std::bit_cast<std::int64_t>(ua - ub));
                    else if (operation == BinaryMultiply)
                        result = Value::signedInteger(std::bit_cast<std::int64_t>(ua * ub));
                    else if (operation == BinaryDivide || operation == BinaryRemainder)
                    {
                        if (b == 0)
                            return fail("WVM1014", "Integer division by zero");
                        if (a == (std::numeric_limits<std::int64_t>::min)() && b == -1)
                            return fail("WVM1015", "Signed integer division overflow");
                        result = Value::signedInteger(operation == BinaryDivide ? a / b : a % b);
                    }
                    else if (operation == BinaryBitwiseAnd)
                        result = Value::signedInteger(a & b);
                    else if (operation == BinaryBitwiseOr)
                        result = Value::signedInteger(a | b);
                    else if (operation == BinaryBitwiseXor)
                        result = Value::signedInteger(a ^ b);
                    else if (operation == BinaryShiftLeft || operation == BinaryShiftRight)
                    {
                        if (b < 0 || b >= 64)
                            return fail("WVM1016", "Integer shift count is outside [0, 63]");
                        if (operation == BinaryShiftLeft)
                            result = Value::signedInteger(std::bit_cast<std::int64_t>(ua << b));
                        else
                        {
                            std::uint64_t shifted = ua >> b;
                            if (a < 0 && b != 0)
                                shifted |= (~std::uint64_t{0}) << (64 - b);
                            result = Value::signedInteger(std::bit_cast<std::int64_t>(shifted));
                        }
                    }
                    else
                        return fail("WVM1017", "Signed integer binary operator is not implemented");
                }
                else if (left->kind() == Value::Kind::UnsignedInteger)
                {
                    const std::uint64_t a = left->asUnsignedInteger();
                    const std::uint64_t b = right->asUnsignedInteger();
                    if (operation == BinaryEqual)
                        result = Value::boolean(a == b);
                    else if (operation == BinaryNotEqual)
                        result = Value::boolean(a != b);
                    else if (operation == BinaryLess)
                        result = Value::boolean(a < b);
                    else if (operation == BinaryLessEqual)
                        result = Value::boolean(a <= b);
                    else if (operation == BinaryGreater)
                        result = Value::boolean(a > b);
                    else if (operation == BinaryGreaterEqual)
                        result = Value::boolean(a >= b);
                    else if (operation == BinaryAdd)
                        result = Value::unsignedInteger(a + b);
                    else if (operation == BinarySubtract)
                        result = Value::unsignedInteger(a - b);
                    else if (operation == BinaryMultiply)
                        result = Value::unsignedInteger(a * b);
                    else if (operation == BinaryDivide || operation == BinaryRemainder)
                    {
                        if (b == 0)
                            return fail("WVM1014", "Integer division by zero");
                        result = Value::unsignedInteger(operation == BinaryDivide ? a / b : a % b);
                    }
                    else if (operation == BinaryBitwiseAnd)
                        result = Value::unsignedInteger(a & b);
                    else if (operation == BinaryBitwiseOr)
                        result = Value::unsignedInteger(a | b);
                    else if (operation == BinaryBitwiseXor)
                        result = Value::unsignedInteger(a ^ b);
                    else if (operation == BinaryShiftLeft || operation == BinaryShiftRight)
                    {
                        if (b >= 64)
                            return fail("WVM1016", "Integer shift count is outside [0, 63]");
                        result = Value::unsignedInteger(operation == BinaryShiftLeft ? a << b : a >> b);
                    }
                    else
                        return fail("WVM1018", "Unsigned integer binary operator is not implemented");
                }
                else if (left->kind() == Value::Kind::Float64)
                {
                    const double a = left->asFloat64();
                    const double b = right->asFloat64();
                    if (isComparison(operation))
                        result = Value::boolean(compareResult(operation, a < b, a == b));
                    else if (operation == BinaryAdd)
                        result = Value::floating(a + b);
                    else if (operation == BinarySubtract)
                        result = Value::floating(a - b);
                    else if (operation == BinaryMultiply)
                        result = Value::floating(a * b);
                    else if (operation == BinaryDivide)
                        result = Value::floating(a / b);
                    else if (operation == BinaryRemainder)
                        result = Value::floating(std::fmod(a, b));
                    else
                        return fail("WVM1019", "Floating-point binary operator is not implemented");
                }
                else if (left->kind() == Value::Kind::Boolean &&
                         (operation == BinaryEqual || operation == BinaryNotEqual))
                    result = Value::boolean(operation == BinaryEqual ? *left == *right : !(*left == *right));
                else if (left->kind() == Value::Kind::String)
                {
                    if (operation == BinaryAdd)
                        result = Value::string(std::string{left->asString()} + std::string{right->asString()});
                    else if (isComparison(operation))
                        result = Value::boolean(compareResult(operation, left->asString() < right->asString(),
                                                              left->asString() == right->asString()));
                    else
                        return fail("WVM1020", "String binary operator is not implemented");
                }
                else if (left->kind() == Value::Kind::Text)
                {
                    if (operation == BinaryAdd)
                    {
                        std::u32string combined{left->asText()};
                        combined.append(right->asText());
                        result = Value::text(std::move(combined));
                    }
                    else if (isComparison(operation))
                        result = Value::boolean(compareResult(operation, left->asText() < right->asText(),
                                                              left->asText() == right->asText()));
                    else
                        return fail("WVM1050", "Unicode text binary operator is not implemented");
                }
                else
                    return fail("WVM1021", "Binary operator is invalid for the runtime value kind");
                write(instruction.result, std::move(result));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ArrayCreate)
            {
                if (module.types[instruction.resultType].kind != TypeArray)
                    return fail("WVM1051", "Array creation result type is not an array");
                std::vector<Value> values;
                values.reserve(instruction.operands.size());
                for (const std::uint32_t operandId : instruction.operands)
                {
                    const Value* operand = read(operandId);
                    if (!operand)
                        return fail("WVM1052", "Array creation reads an unavailable element");
                    values.push_back(*operand);
                }
                if (!write(instruction.result, Value::array(std::move(values))))
                    return fail("WVM1009", "Array result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ArrayLength)
            {
                const Value* array = instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                if (!array || array->kind() != Value::Kind::Array)
                    return fail("WVM1053", "Array length requires an available array");
                write(instruction.result, Value::unsignedInteger(array->elementCount()));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ArrayGet ||
                instruction.opcode == bytecode::Opcode::ArrayElement)
            {
                const Value* array = instruction.operands.size() == 2 ? read(instruction.operands[0]) : nullptr;
                const Value* rawIndex = instruction.operands.size() == 2 ? read(instruction.operands[1]) : nullptr;
                const std::optional<std::size_t> index = rawIndex ? valueIndex(*rawIndex) : std::nullopt;
                const Value* element = array && index ? array->element(*index) : nullptr;
                if (!element)
                    return fail("WVM1054", "Array index is invalid or outside the array bounds");
                write(instruction.result, *element);
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DictionaryCreate)
            {
                if (module.types[instruction.resultType].kind != TypeDictionary || instruction.operands.size() % 2 != 0)
                    return fail("WVM1078", "Dictionary creation has an invalid result type or entry shape");
                std::vector<Value> entries;
                entries.reserve(instruction.operands.size());
                for (const std::uint32_t operandId : instruction.operands)
                {
                    const Value* operand = read(operandId);
                    if (!operand)
                        return fail("WVM1079", "Dictionary creation reads an unavailable key or value");
                    entries.push_back(*operand);
                }
                const bool ordered = module.string(instruction.selector) == "ordered";
                if (!write(instruction.result, Value::dictionary(instruction.resultType, std::move(entries), ordered)))
                    return fail("WVM1009", "Dictionary result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DictionaryGet)
            {
                const Value* dictionary = instruction.operands.size() == 2 ? read(instruction.operands[0]) : nullptr;
                const Value* key = instruction.operands.size() == 2 ? read(instruction.operands[1]) : nullptr;
                const Value* value = dictionary && key ? dictionary->dictionaryValue(*key) : nullptr;
                if (!value)
                    return fail("WVM1080", "Dictionary key was not found");
                write(instruction.result, *value);
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DictionaryPlace)
            {
                const Value* base = instruction.operands.size() == 2 ? read(instruction.operands[0]) : nullptr;
                const Value* key = instruction.operands.size() == 2 ? read(instruction.operands[1]) : nullptr;
                if (!base || !key)
                    return fail("WVM1081", "Dictionary place requires an available base and key");
                const Value* dictionary = base;
                if (base->kind() == Value::Kind::Place)
                {
                    PlaceStorage* const basePlace = base->asPlace();
                    dictionary = basePlace && basePlace->initialized ? &basePlace->storedValue() : nullptr;
                }
                if (!dictionary || dictionary->kind() != Value::Kind::Dictionary)
                    return fail("WVM1082", "Dictionary place base is not an initialized dictionary");

                auto storage = std::make_unique<PlaceStorage>();
                storage->owner = *dictionary;
                storage->alias = storage->owner.mutableDictionaryValue(*key);
                storage->initialized = storage->alias != nullptr;
                storage->mutableValue = (module.types[instruction.resultType].flags & 0x01u) != 0;
                if (!storage->alias)
                    return fail("WVM1080", "Dictionary key was not found");
                PlaceStorage* const pointer = storage.get();
                frame.localPlaces.push_back(std::move(storage));
                write(instruction.result, Value::place(pointer));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Interpolate)
            {
                if (instruction.stringSegments.size() != instruction.operands.size() + 1)
                    return fail("WVM1055", "Interpolation segment shape is invalid");
                std::string utf8;
                for (std::size_t index = 0; index < instruction.stringSegments.size(); ++index)
                {
                    utf8.append(module.string(instruction.stringSegments[index]));
                    if (index < instruction.operands.size())
                    {
                        const Value* operand = read(instruction.operands[index]);
                        if (!operand)
                            return fail("WVM1056", "Interpolation reads an unavailable value");
                        const std::optional<std::string> formatted = formatUtf8(*operand);
                        if (!formatted)
                            return fail("WVM1062", "Interpolation value has no VM string representation");
                        utf8.append(*formatted);
                    }
                }
                Value result;
                if (module.types[instruction.resultType].kind == TypeText)
                {
                    Utf8DecodeResult decoded = decodeUtf8(utf8);
                    if (!decoded.succeeded())
                        return fail("WVM1057", "Unicode interpolation produced invalid UTF-8");
                    result = Value::text(std::move(decoded.value));
                }
                else
                    result = Value::string(std::move(utf8));
                write(instruction.result, std::move(result));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::EnumConstant)
            {
                const bytecode::Type& enumType = module.types[instruction.targetType];
                const std::string_view selector = module.string(instruction.selector);
                const auto enumCase = std::find_if(enumType.enumCases.begin(), enumType.enumCases.end(),
                                                   [&](const bytecode::Type::EnumCase& candidate)
                                                   { return module.string(candidate.name) == selector; });
                if (enumCase == enumType.enumCases.end() || enumType.enumUnderlyingType >= module.types.size())
                    return fail("WVM1178", "Enum constant does not match its canonical type metadata");
                const std::uint8_t underlying = module.types[enumType.enumUnderlyingType].kind;
                Value result = isSignedType(underlying)
                                   ? Value::signedInteger(signExtend(enumCase->rawValue, integerWidth(underlying)))
                                   : Value::unsignedInteger(lowBits(enumCase->rawValue, integerWidth(underlying)));
                if (!write(instruction.result, std::move(result)))
                    return fail("WVM1009", "Enum constant result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::IntrinsicCall &&
                (instruction.intrinsicFamily == IntrinsicEnum || instruction.intrinsicFamily == IntrinsicFlagset))
            {
                const Value* receiver =
                    instruction.operands.empty() ? nullptr : storedValue(read(instruction.operands[0]));
                if (!receiver || instruction.targetType >= module.types.size())
                    return fail("WVM1179", "Enum intrinsic has no available receiver or target type");
                const bytecode::Type& enumType = module.types[instruction.targetType];
                if (enumType.kind != TypeNamed ||
                    (enumType.nominalKind != NominalEnum && enumType.nominalKind != NominalFlagset) ||
                    enumType.enumUnderlyingType >= module.types.size())
                    return fail("WVM1179", "Enum intrinsic target metadata is invalid");

                const auto rawValue = [&](const Value& value) -> std::optional<std::uint64_t>
                {
                    if (value.kind() == Value::Kind::SignedInteger)
                        return std::bit_cast<std::uint64_t>(value.asSignedInteger());
                    if (value.kind() == Value::Kind::UnsignedInteger)
                        return value.asUnsignedInteger();
                    return std::nullopt;
                };
                const std::optional<std::uint64_t> receiverBits = rawValue(*receiver);
                if (!receiverBits)
                    return fail("WVM1179", "Enum intrinsic receiver is not an integer value");
                const std::uint8_t underlying = module.types[enumType.enumUnderlyingType].kind;
                const std::uint32_t width = integerWidth(underlying);
                const std::uint64_t rawReceiver = lowBits(*receiverBits, width);
                const std::string_view selector = module.string(instruction.selector);
                Value result;
                if (selector == "Name")
                {
                    const auto enumCase = std::find_if(enumType.enumCases.begin(), enumType.enumCases.end(),
                                                       [&](const bytecode::Type::EnumCase& candidate)
                                                       { return lowBits(candidate.rawValue, width) == rawReceiver; });
                    result = Value::string(enumCase == enumType.enumCases.end()
                                               ? std::string{}
                                               : std::string{module.string(enumCase->name)});
                }
                else if (selector == "Value")
                {
                    result = isSignedType(underlying) ? Value::signedInteger(signExtend(rawReceiver, width))
                                                      : Value::unsignedInteger(rawReceiver);
                }
                else if (selector == "IsValid")
                {
                    const bool valid =
                        std::ranges::any_of(enumType.enumCases, [&](const auto& candidate)
                                            { return lowBits(candidate.rawValue, width) == rawReceiver; });
                    result = Value::boolean(valid);
                }
                else if (selector == "Clear")
                    result = isSignedType(underlying) ? Value::signedInteger(0) : Value::unsignedInteger(0);
                else
                {
                    const Value* mask =
                        instruction.operands.size() == 2 ? storedValue(read(instruction.operands[1])) : nullptr;
                    const std::optional<std::uint64_t> maskBits = mask ? rawValue(*mask) : std::nullopt;
                    if (!maskBits)
                        return fail("WVM1180", "Flagset intrinsic requires an integer mask operand");
                    const std::uint64_t rawMask = lowBits(*maskBits, width);
                    if (selector == "Has")
                        result = Value::boolean((rawReceiver & rawMask) == rawMask);
                    else if (selector == "HasAny")
                        result = Value::boolean((rawReceiver & rawMask) != 0);
                    else
                    {
                        std::uint64_t bits = 0;
                        if (selector == "With")
                            bits = rawReceiver | rawMask;
                        else if (selector == "Without")
                            bits = rawReceiver & ~rawMask;
                        else if (selector == "Toggle")
                            bits = rawReceiver ^ rawMask;
                        else
                            return fail("WVM1181",
                                        "Enum or flagset intrinsic is not implemented: " + std::string{selector});
                        bits = lowBits(bits, width);
                        result = isSignedType(underlying) ? Value::signedInteger(signExtend(bits, width))
                                                          : Value::unsignedInteger(bits);
                    }
                }
                if (instruction.result != bytecode::InvalidIndex && !write(instruction.result, std::move(result)))
                    return fail("WVM1009", "Enum intrinsic result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::IntrinsicCall &&
                (instruction.intrinsicFamily == IntrinsicArray || instruction.intrinsicFamily == IntrinsicDictionary ||
                 instruction.intrinsicFamily == IntrinsicString || instruction.intrinsicFamily == IntrinsicText))
            {
                const Value* rawReceiver = instruction.operands.empty() ? nullptr : read(instruction.operands.front());
                const std::string_view selector = module.string(instruction.selector);
                if (!rawReceiver)
                    return fail("WVM1058", "Intrinsic call has no available receiver");
                PlaceStorage* const receiverPlace =
                    rawReceiver->kind() == Value::Kind::Place ? rawReceiver->asPlace() : nullptr;
                const Value* receiver =
                    receiverPlace && receiverPlace->initialized ? &receiverPlace->storedValue() : rawReceiver;
                Value* const mutableReceiver =
                    receiverPlace && receiverPlace->initialized && receiverPlace->mutableValue
                        ? &receiverPlace->storedValue()
                        : nullptr;
                const auto argument = [&](const std::size_t index) -> const Value*
                { return index < instruction.operands.size() ? read(instruction.operands[index]) : nullptr; };
                const auto indexArgument = [&](const std::size_t index) -> std::optional<std::size_t>
                {
                    const Value* value = argument(index);
                    return value ? valueIndex(*value) : std::nullopt;
                };

                if (instruction.intrinsicFamily == IntrinsicString)
                {
                    std::vector<const Value*> intrinsicArguments;
                    intrinsicArguments.reserve(instruction.operands.size() - 1);
                    for (std::size_t index = 1; index < instruction.operands.size(); ++index)
                        intrinsicArguments.push_back(read(instruction.operands[index]));
                    const std::uint8_t resultType = instruction.resultType < module.types.size()
                                                        ? module.types[instruction.resultType].kind
                                                        : TypeVoid;
                    detail::StringIntrinsicResult intrinsicResult = detail::executeStringIntrinsic(
                        selector, *receiver, mutableReceiver, intrinsicArguments, resultType);
                    if (!intrinsicResult.recognized)
                        return fail("WVM1061", "String intrinsic is not implemented: " + std::string{selector});
                    if (!intrinsicResult.succeeded)
                        return fail(std::move(intrinsicResult.code), std::move(intrinsicResult.message));
                    if (instruction.result != bytecode::InvalidIndex &&
                        !write(instruction.result, std::move(intrinsicResult.value)))
                        return fail("WVM1009", "Intrinsic result register is invalid");
                    ++frame.instruction;
                    continue;
                }
                if (instruction.intrinsicFamily == IntrinsicText)
                {
                    std::vector<const Value*> intrinsicArguments;
                    intrinsicArguments.reserve(instruction.operands.size() - 1);
                    for (std::size_t index = 1; index < instruction.operands.size(); ++index)
                        intrinsicArguments.push_back(read(instruction.operands[index]));
                    detail::TextIntrinsicResult intrinsicResult =
                        detail::executeTextIntrinsic(selector, *receiver, intrinsicArguments);
                    if (!intrinsicResult.recognized)
                        return fail("WVM1061", "Text intrinsic is not implemented: " + std::string{selector});
                    if (!intrinsicResult.succeeded)
                        return fail(std::move(intrinsicResult.code), std::move(intrinsicResult.message));
                    if (instruction.result != bytecode::InvalidIndex &&
                        !write(instruction.result, std::move(intrinsicResult.value)))
                        return fail("WVM1009", "Intrinsic result register is invalid");
                    ++frame.instruction;
                    continue;
                }

                Value result;
                if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                    selector == "Count")
                    result = Value::unsignedInteger(receiver->elementCount());
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Empty")
                    result = Value::boolean(receiver->elementCount() == 0);
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Contains" && instruction.operands.size() == 2)
                {
                    const Value* needle = read(instruction.operands[1]);
                    if (!needle)
                        return fail("WVM1083", "Array Contains reads an unavailable value");
                    bool contains = false;
                    for (std::size_t index = 0; index < receiver->elementCount(); ++index)
                        contains = contains || *receiver->element(index) == *needle;
                    result = Value::boolean(contains);
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Capacity")
                    result = Value::unsignedInteger(receiver->elementCapacity());
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         (selector == "IndexOf" || selector == "LastIndexOf") && instruction.operands.size() == 2)
                {
                    const Value* needle = argument(1);
                    if (!needle)
                        return fail("WVM1094", "Array index search reads an unavailable value");
                    std::int64_t found = -1;
                    if (selector == "IndexOf")
                    {
                        for (std::size_t index = 0; index < receiver->elementCount(); ++index)
                        {
                            if (*receiver->element(index) == *needle)
                            {
                                found = static_cast<std::int64_t>(index);
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (std::size_t index = receiver->elementCount(); index > 0; --index)
                        {
                            if (*receiver->element(index - 1) == *needle)
                            {
                                found = static_cast<std::int64_t>(index - 1);
                                break;
                            }
                        }
                    }
                    result = Value::signedInteger(found);
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         (selector == "First" || selector == "Last"))
                {
                    if (receiver->elementCount() == 0)
                        return fail("WVM1095", "Array endpoint intrinsic requires a non-empty array");
                    result = *receiver->element(selector == "First" ? 0 : receiver->elementCount() - 1);
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         (selector == "Get" || selector == "At") && instruction.operands.size() == 2)
                {
                    const std::optional<std::size_t> index = indexArgument(1);
                    const Value* value = index ? receiver->element(*index) : nullptr;
                    if (!value)
                        return fail("WVM1096", "Array intrinsic index is outside the array bounds");
                    result = *value;
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "GetOr" && instruction.operands.size() == 3)
                {
                    const std::optional<std::size_t> index = indexArgument(1);
                    const Value* fallback = argument(2);
                    if (!fallback)
                        return fail("WVM1097", "Array GetOr reads an unavailable fallback");
                    const Value* value = index ? receiver->element(*index) : nullptr;
                    result = value ? *value : *fallback;
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Clone")
                    result = receiver->cloneOwned();
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Slice" && (instruction.operands.size() == 2 || instruction.operands.size() == 3))
                {
                    const std::optional<std::size_t> start = indexArgument(1);
                    const std::optional<std::size_t> count = instruction.operands.size() == 3
                                                                 ? indexArgument(2)
                                                                 : std::optional<std::size_t>{receiver->elementCount()};
                    if (!start || !count)
                        return fail("WVM1098", "Array Slice requires non-negative start and count operands");
                    result = receiver->arraySlice(*start, *count);
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         (selector == "Take" || selector == "Skip") && instruction.operands.size() == 2)
                {
                    const std::optional<std::size_t> count = indexArgument(1);
                    if (!count)
                        return fail("WVM1099", "Array Take/Skip requires a non-negative count");
                    result = selector == "Take" ? receiver->arraySlice(0, *count)
                                                : receiver->arraySlice(*count, receiver->elementCount());
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Concat" && instruction.operands.size() == 2)
                {
                    const Value* other = argument(1);
                    if (!other || other->kind() != Value::Kind::Array)
                        return fail("WVM1100", "Array Concat requires another array");
                    result = receiver->arrayConcat(*other);
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Reversed")
                {
                    result = receiver->cloneOwned();
                    result.arrayReverse();
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Join" && instruction.operands.size() == 2)
                {
                    const Value* separator = argument(1);
                    if (!separator || separator->kind() != Value::Kind::String)
                        return fail("WVM1101", "Array Join requires a string separator");
                    std::string joined;
                    for (std::size_t index = 0; index < receiver->elementCount(); ++index)
                    {
                        const Value* value = receiver->element(index);
                        if (!value || value->kind() != Value::Kind::String)
                            return fail("WVM1102", "Array Join requires string elements");
                        if (index != 0)
                            joined.append(separator->asString());
                        joined.append(value->asString());
                    }
                    result = Value::string(std::move(joined));
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         (selector == "Push" || selector == "PushFront") && instruction.operands.size() == 2)
                {
                    const Value* value = argument(1);
                    if (!mutableReceiver || !value)
                        return fail("WVM1103", "Array Push requires a mutable receiver and available value");
                    mutableReceiver->arrayPush(*value, selector == "PushFront");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         (selector == "Pop" || selector == "PopFront"))
                {
                    if (!mutableReceiver || !mutableReceiver->arrayPop(result, selector == "PopFront"))
                        return fail("WVM1104", "Array Pop requires a mutable non-empty array");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Insert" && instruction.operands.size() == 3)
                {
                    const std::optional<std::size_t> index = indexArgument(1);
                    const Value* value = argument(2);
                    if (!mutableReceiver || !index || !value || !mutableReceiver->arrayInsert(*index, *value))
                        return fail("WVM1105", "Array Insert index is invalid or receiver is not mutable");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "RemoveAt" && instruction.operands.size() == 2)
                {
                    const std::optional<std::size_t> index = indexArgument(1);
                    if (!mutableReceiver || !index || !mutableReceiver->arrayRemoveAt(*index))
                        return fail("WVM1106", "Array RemoveAt index is invalid or receiver is not mutable");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Remove" && instruction.operands.size() == 2)
                {
                    const Value* value = argument(1);
                    if (!mutableReceiver || !value)
                        return fail("WVM1107", "Array Remove requires a mutable receiver and available value");
                    result = Value::boolean(mutableReceiver->arrayRemove(*value));
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Clear")
                {
                    if (!mutableReceiver)
                        return fail("WVM1108", "Array Clear requires a mutable receiver");
                    mutableReceiver->arrayClear();
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Extend" && instruction.operands.size() == 2)
                {
                    const Value* other = argument(1);
                    if (!mutableReceiver || !other || !mutableReceiver->arrayExtend(*other))
                        return fail("WVM1109", "Array Extend requires a mutable receiver and another array");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Reserve" && instruction.operands.size() == 2)
                {
                    const std::optional<std::size_t> capacity = indexArgument(1);
                    if (!mutableReceiver || !capacity || !mutableReceiver->arrayReserve(*capacity))
                        return fail("WVM1110", "Array Reserve requires a mutable receiver and valid capacity");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "ShrinkToFit")
                {
                    if (!mutableReceiver)
                        return fail("WVM1111", "Array ShrinkToFit requires a mutable receiver");
                    mutableReceiver->arrayShrinkToFit();
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Fill" && instruction.operands.size() == 2)
                {
                    const Value* value = argument(1);
                    if (!mutableReceiver || !value)
                        return fail("WVM1112", "Array Fill requires a mutable receiver and available value");
                    mutableReceiver->arrayFill(*value);
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Reverse")
                {
                    if (!mutableReceiver)
                        return fail("WVM1113", "Array Reverse requires a mutable receiver");
                    mutableReceiver->arrayReverse();
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Sort")
                {
                    if (!mutableReceiver || !mutableReceiver->arraySort())
                        return fail("WVM1114", "Array Sort requires a mutable array of orderable values");
                }
                else if (instruction.intrinsicFamily == IntrinsicArray && receiver->kind() == Value::Kind::Array &&
                         selector == "Sorted")
                {
                    result = receiver->cloneOwned();
                    if (!result.arraySort())
                        return fail("WVM1115", "Array Sorted requires orderable values");
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Count")
                    result = Value::unsignedInteger(receiver->dictionaryCount());
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Empty")
                    result = Value::boolean(receiver->dictionaryCount() == 0);
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "ContainsKey" &&
                         instruction.operands.size() == 2)
                {
                    const Value* key = read(instruction.operands[1]);
                    result = Value::boolean(key && receiver->dictionaryValue(*key));
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "ContainsValue" &&
                         instruction.operands.size() == 2)
                {
                    const Value* value = read(instruction.operands[1]);
                    result = Value::boolean(value && receiver->dictionaryContainsValue(*value));
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && (selector == "Get" || selector == "At") &&
                         instruction.operands.size() == 2)
                {
                    const Value* key = read(instruction.operands[1]);
                    const Value* value = key ? receiver->dictionaryValue(*key) : nullptr;
                    if (!value)
                        return fail("WVM1080", "Dictionary key was not found");
                    result = *value;
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "GetOr" &&
                         instruction.operands.size() == 3)
                {
                    const Value* key = read(instruction.operands[1]);
                    const Value* fallback = read(instruction.operands[2]);
                    if (!key || !fallback)
                        return fail("WVM1084", "Dictionary GetOr reads unavailable operands");
                    const Value* value = receiver->dictionaryValue(*key);
                    result = value ? *value : *fallback;
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "TryGet" &&
                         instruction.operands.size() == 3)
                {
                    const Value* key = read(instruction.operands[1]);
                    const Value* out = read(instruction.operands[2]);
                    PlaceStorage* const outPlace = out ? out->asPlace() : nullptr;
                    if (!key || !outPlace || !outPlace->initialized || !outPlace->mutableValue)
                        return fail("WVM1085", "Dictionary TryGet requires a mutable initialized output place");
                    const Value* value = receiver->dictionaryValue(*key);
                    if (value)
                        outPlace->storedValue() = *value;
                    result = Value::boolean(value != nullptr);
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Set" &&
                         instruction.operands.size() == 3)
                {
                    const Value* key = read(instruction.operands[1]);
                    const Value* value = read(instruction.operands[2]);
                    if (!receiverPlace || !receiverPlace->mutableValue || !key || !value)
                        return fail("WVM1086", "Dictionary Set requires a mutable receiver and available operands");
                    receiverPlace->storedValue().dictionarySet(*key, *value);
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "GetOrAdd" &&
                         instruction.operands.size() == 3)
                {
                    const Value* key = read(instruction.operands[1]);
                    const Value* fallback = read(instruction.operands[2]);
                    if (!receiverPlace || !receiverPlace->mutableValue || !key || !fallback)
                        return fail("WVM1087", "Dictionary GetOrAdd requires a mutable receiver and operands");
                    if (const Value* existing = receiver->dictionaryValue(*key))
                        result = *existing;
                    else
                    {
                        receiverPlace->storedValue().dictionarySet(*key, *fallback);
                        result = *fallback;
                    }
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && (selector == "Keys" || selector == "Values"))
                {
                    std::vector<Value> values;
                    values.reserve(receiver->dictionaryCount());
                    for (std::size_t index = 0; index < receiver->dictionaryCount(); ++index)
                        values.push_back(selector == "Keys" ? *receiver->dictionaryKey(index)
                                                            : *receiver->dictionaryValue(index));
                    result = Value::array(std::move(values));
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Clone")
                    result = receiver->cloneOwned();
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Merge" &&
                         instruction.operands.size() == 2)
                {
                    const Value* other = read(instruction.operands[1]);
                    if (!other || other->kind() != Value::Kind::Dictionary)
                        return fail("WVM1088", "Dictionary Merge requires another dictionary");
                    result = receiver->cloneOwned();
                    for (std::size_t index = 0; index < other->dictionaryCount(); ++index)
                        result.dictionarySet(*other->dictionaryKey(index), *other->dictionaryValue(index));
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Extend" &&
                         instruction.operands.size() == 2)
                {
                    const Value* other = read(instruction.operands[1]);
                    if (!receiverPlace || !receiverPlace->mutableValue || !other ||
                        other->kind() != Value::Kind::Dictionary)
                        return fail("WVM1089", "Dictionary Extend requires a mutable receiver and dictionary");
                    for (std::size_t index = 0; index < other->dictionaryCount(); ++index)
                        receiverPlace->storedValue().dictionarySet(*other->dictionaryKey(index),
                                                                   *other->dictionaryValue(index));
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Clear")
                {
                    if (!receiverPlace || !receiverPlace->mutableValue)
                        return fail("WVM1090", "Dictionary Clear requires a mutable receiver");
                    receiverPlace->storedValue().dictionaryClear();
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary && selector == "Remove" &&
                         instruction.operands.size() == 2)
                {
                    const Value* key = read(instruction.operands[1]);
                    if (!receiverPlace || !receiverPlace->mutableValue || !key)
                        return fail("WVM1091", "Dictionary Remove requires a mutable receiver and key");
                    result = Value::boolean(receiverPlace->storedValue().dictionaryRemove(*key));
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary &&
                         (selector == "FirstKey" || selector == "FirstValue" || selector == "LastKey" ||
                          selector == "LastValue"))
                {
                    if (!receiver->dictionaryIsOrdered() || receiver->dictionaryCount() == 0)
                        return fail("WVM1092", "Ordered dictionary endpoint requires a non-empty ordered dictionary");
                    const std::size_t index = selector.starts_with("First") ? 0 : receiver->dictionaryCount() - 1;
                    result =
                        selector.ends_with("Key") ? *receiver->dictionaryKey(index) : *receiver->dictionaryValue(index);
                }
                else if (instruction.intrinsicFamily == IntrinsicDictionary &&
                         receiver->kind() == Value::Kind::Dictionary &&
                         (selector == "FloorKeyOr" || selector == "CeilKeyOr") && instruction.operands.size() == 3)
                {
                    const Value* key = read(instruction.operands[1]);
                    const Value* fallback = read(instruction.operands[2]);
                    if (!key || !fallback || !receiver->dictionaryIsOrdered())
                        return fail("WVM1093", "Ordered dictionary bound lookup has invalid operands");
                    const Value* found = selector == "FloorKeyOr" ? receiver->dictionaryFloorKey(*key)
                                                                  : receiver->dictionaryCeilKey(*key);
                    result = found ? *found : *fallback;
                }
                else
                    return fail("WVM1061", "Container or text intrinsic is not implemented: " + std::string{selector});
                if (instruction.result != bytecode::InvalidIndex && !write(instruction.result, std::move(result)))
                    return fail("WVM1009", "Intrinsic result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::AnyBox)
            {
                const Value* payload =
                    instruction.operands.size() == 1 ? storedValue(read(instruction.operands.front())) : nullptr;
                if (!payload)
                    return fail("WVM1182", "Any boxing requires an available payload");
                if (!write(instruction.result, Value::any(instruction.targetType, payload->cloneOwned())))
                    return fail("WVM1009", "Any box result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::AnyTypeTest ||
                instruction.opcode == bytecode::Opcode::AnyCheckedCast)
            {
                const Value* boxed =
                    instruction.operands.size() == 1 ? storedValue(read(instruction.operands.front())) : nullptr;
                const Value* payload = boxed ? boxed->anyPayload() : nullptr;
                if (!boxed || boxed->kind() != Value::Kind::Any || !payload)
                    return fail("WVM1183", "Any test or cast requires a live boxed value");

                bool matches = boxed->anyType() == instruction.targetType;
                const std::uint32_t targetNominal = nominalType(instruction.targetType);
                if (const Value* object = objectValue(payload); object && targetNominal != bytecode::InvalidIndex)
                    matches = isObjectType(object->aggregateType(), targetNominal);
                if (instruction.opcode == bytecode::Opcode::AnyTypeTest)
                {
                    if (!write(instruction.result, Value::boolean(matches)))
                        return fail("WVM1009", "Any type-test result register is invalid");
                    ++frame.instruction;
                    continue;
                }
                if (!matches)
                    return fail("WVM1184", "Checked any cast failed for the requested target type");

                Value result = payload->cloneOwned();
                if (instruction.resultType < module.types.size() &&
                    module.types[instruction.resultType].kind == TypeReference)
                {
                    const Value* object = objectValue(payload);
                    if (!object)
                        return fail("WVM1184", "Checked any cast cannot borrow a non-object payload");
                    result = Value::objectBorrow(object->scalar_.aggregate);
                }
                if (!write(instruction.result, std::move(result)))
                    return fail("WVM1009", "Any checked-cast result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::NullableWrap)
            {
                const Value* payload =
                    instruction.operands.size() == 1 ? storedValue(read(instruction.operands.front())) : nullptr;
                if (!payload)
                    return fail("WVM1185", "Nullable wrapping requires an available payload");
                if (!write(instruction.result, payload->cloneOwned()))
                    return fail("WVM1009", "Nullable wrap result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::NullableUnwrap)
            {
                const Value* nullable =
                    instruction.operands.size() == 1 ? storedValue(read(instruction.operands.front())) : nullptr;
                if (!nullable)
                    return fail("WVM1186", "Nullable unwrap requires an available value");
                if (nullable->kind() == Value::Kind::Null)
                    return fail("WVM1187", "Nullable value does not contain a payload");
                if (!write(instruction.result, nullable->cloneOwned()))
                    return fail("WVM1009", "Nullable unwrap result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::VariantTest ||
                instruction.opcode == bytecode::Opcode::VariantPayload ||
                instruction.opcode == bytecode::Opcode::ResultIsError ||
                instruction.opcode == bytecode::Opcode::ResultValue ||
                instruction.opcode == bytecode::Opcode::ResultUnwrap)
            {
                const Value* variant =
                    instruction.operands.size() == 1 ? storedValue(read(instruction.operands.front())) : nullptr;
                const Value* present = variant ? variant->field(0) : nullptr;
                if (!variant || !present || present->kind() != Value::Kind::Boolean)
                    return fail("WVM1188", "Variant operation requires canonical option/result storage");
                const bool hasValue = present->asBoolean();
                if (instruction.opcode == bytecode::Opcode::VariantTest)
                {
                    const std::string_view selector = module.string(instruction.selector);
                    const bool positive = selector == "Some" || selector == "Ok";
                    if (!write(instruction.result, Value::boolean(positive ? hasValue : !hasValue)))
                        return fail("WVM1009", "Variant-test result register is invalid");
                }
                else if (instruction.opcode == bytecode::Opcode::ResultIsError)
                {
                    if (!write(instruction.result, Value::boolean(!hasValue)))
                        return fail("WVM1009", "Result error-test register is invalid");
                }
                else
                {
                    const bool errorPayload = instruction.opcode == bytecode::Opcode::VariantPayload &&
                                              module.string(instruction.selector) == "Err";
                    if (instruction.opcode == bytecode::Opcode::ResultUnwrap && !hasValue)
                        return fail("WVM1189", "Result does not contain a success value");
                    const Value* payload = variant->field(errorPayload ? 2 : 1);
                    if (!payload)
                        return fail("WVM1190", "Variant payload is outside its canonical field layout");
                    if (!write(instruction.result, payload->cloneOwned()))
                        return fail("WVM1009", "Variant payload result register is invalid");
                }
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::IteratorCreate)
            {
                const std::string_view selector = module.string(instruction.selector);
                if (instruction.resultType >= module.types.size() ||
                    module.types[instruction.resultType].kind != TypeIterator)
                    return fail("WVM1191", "Iterator creation result type is not an iterator");
                if (selector == "range.inclusive" || selector == "range.exclusive")
                {
                    const Value* start = instruction.operands.size() == 3 ? read(instruction.operands[0]) : nullptr;
                    const Value* end = instruction.operands.size() == 3 ? read(instruction.operands[1]) : nullptr;
                    const Value* step = instruction.operands.size() == 3 ? read(instruction.operands[2]) : nullptr;
                    if (!start || !end || !step || start->kind() != end->kind() || start->kind() != step->kind() ||
                        (start->kind() != Value::Kind::SignedInteger && start->kind() != Value::Kind::UnsignedInteger))
                        return fail("WVM1192", "Range iterator requires three compatible integer values");
                    const bool zero = step->kind() == Value::Kind::SignedInteger ? step->asSignedInteger() == 0
                                                                                 : step->asUnsignedInteger() == 0;
                    if (zero)
                        return fail("WVM1193", "Range iterator step cannot be zero");
                    if (!write(instruction.result, Value::rangeIterator(instruction.resultType, *start, *end, *step,
                                                                        selector == "range.inclusive")))
                        return fail("WVM1009", "Range iterator result register is invalid");
                }
                else if (selector == "array" || selector == "dictionary")
                {
                    const Value* source = instruction.operands.empty() ? nullptr : read(instruction.operands[0]);
                    const Value* container = storedValue(source);
                    if (!source || !container || (selector == "array" && container->kind() != Value::Kind::Array) ||
                        (selector == "dictionary" && container->kind() != Value::Kind::Dictionary))
                        return fail("WVM1194", "Container iterator source does not match its selector");
                    std::size_t step = 1;
                    if (instruction.operands.size() == 2)
                    {
                        const Value* stepValue = read(instruction.operands[1]);
                        const std::optional<std::size_t> converted = stepValue ? valueIndex(*stepValue) : std::nullopt;
                        if (!converted || *converted == 0)
                            return fail("WVM1195", "Container iterator step must be a positive integer");
                        step = *converted;
                    }
                    else if (instruction.operands.size() != 1)
                        return fail("WVM1194", "Container iterator has an invalid operand shape");
                    if (!write(instruction.result, Value::containerIterator(instruction.resultType, *source, step)))
                        return fail("WVM1009", "Container iterator result register is invalid");
                }
                else
                    return fail("WVM1196", "Iterator selector is not implemented: " + std::string{selector});
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::IteratorHasNext ||
                instruction.opcode == bytecode::Opcode::IteratorValue ||
                instruction.opcode == bytecode::Opcode::IteratorAdvance)
            {
                Register* iteratorRegister =
                    instruction.operands.size() == 1 ? readRegister(instruction.operands.front()) : nullptr;
                Value* iterator = iteratorRegister ? &iteratorRegister->value : nullptr;
                if (!iterator || iterator->kind() != Value::Kind::Iterator)
                    return fail("WVM1197", "Iterator operation requires a live iterator value");

                const auto containerSource = [&]() -> Value*
                {
                    Value* source = iterator->mutableIteratorState(0);
                    if (!source || source->kind() != Value::Kind::Place)
                        return source;
                    PlaceStorage* place = source->asPlace();
                    return place && place->initialized ? &place->storedValue() : nullptr;
                };
                const auto rangeHasNext = [&]() -> std::optional<bool>
                {
                    if (iterator->iteratorFinished())
                        return false;
                    const Value* current = iterator->iteratorState(0);
                    const Value* end = iterator->iteratorState(1);
                    const Value* step = iterator->iteratorState(2);
                    if (!current || !end || !step || current->kind() != end->kind() || current->kind() != step->kind())
                        return std::nullopt;
                    if (current->kind() == Value::Kind::SignedInteger)
                    {
                        const std::int64_t value = current->asSignedInteger();
                        const std::int64_t limit = end->asSignedInteger();
                        return step->asSignedInteger() > 0
                                   ? (iterator->iteratorInclusive() ? value <= limit : value < limit)
                                   : (iterator->iteratorInclusive() ? value >= limit : value > limit);
                    }
                    if (current->kind() == Value::Kind::UnsignedInteger)
                    {
                        const std::uint64_t value = current->asUnsignedInteger();
                        const std::uint64_t limit = end->asUnsignedInteger();
                        return iterator->iteratorInclusive() ? value <= limit : value < limit;
                    }
                    return std::nullopt;
                };
                const auto containerCount = [&]() -> std::optional<std::size_t>
                {
                    const Value* source = containerSource();
                    if (!source)
                        return std::nullopt;
                    if (source->kind() == Value::Kind::Array)
                        return source->elementCount();
                    if (source->kind() == Value::Kind::Dictionary)
                        return source->dictionaryCount();
                    return std::nullopt;
                };

                if (instruction.opcode == bytecode::Opcode::IteratorHasNext)
                {
                    std::optional<bool> hasNext;
                    if (iterator->iteratorIsRange())
                        hasNext = rangeHasNext();
                    else if (const std::optional<std::size_t> count = containerCount())
                        hasNext = !iterator->iteratorFinished() && iterator->iteratorPosition() < *count;
                    if (!hasNext)
                        return fail("WVM1198", "Iterator state is incompatible with its source");
                    if (!write(instruction.result, Value::boolean(*hasNext)))
                        return fail("WVM1009", "Iterator has-next result register is invalid");
                    ++frame.instruction;
                    continue;
                }

                if (instruction.opcode == bytecode::Opcode::IteratorValue)
                {
                    Value* selected = nullptr;
                    bool mutableSelection = false;
                    if (iterator->iteratorIsRange())
                    {
                        const std::optional<bool> hasNext = rangeHasNext();
                        if (!hasNext || !*hasNext)
                            return fail("WVM1199", "Range iterator has no current value");
                        selected = iterator->mutableIteratorState(0);
                    }
                    else
                    {
                        Value* source = containerSource();
                        const std::optional<std::size_t> count = containerCount();
                        const std::size_t position = iterator->iteratorPosition();
                        if (!source || !count || position >= *count)
                            return fail("WVM1199", "Container iterator has no current value");
                        const std::string_view selector = module.string(instruction.selector);
                        if (selector == "__index__")
                        {
                            if (!write(instruction.result, Value::unsignedInteger(position)))
                                return fail("WVM1009", "Iterator index result register is invalid");
                            ++frame.instruction;
                            continue;
                        }
                        if (source->kind() == Value::Kind::Array)
                        {
                            selected = source->mutableElement(position);
                            mutableSelection = true;
                            if (selector != "__value__")
                            {
                                const bytecode::Type& iteratorType = module.types[iterator->aggregateType()];
                                const bytecode::Type* arrayType =
                                    !iteratorType.arguments.empty() &&
                                            iteratorType.arguments.front() < module.types.size()
                                        ? &module.types[iteratorType.arguments.front()]
                                        : nullptr;
                                const bytecode::Type* itemType =
                                    arrayType && !arrayType->arguments.empty() &&
                                            arrayType->arguments.front() < module.types.size()
                                        ? &module.types[arrayType->arguments.front()]
                                        : nullptr;
                                if (!itemType || !selected)
                                    return fail("WVM1200", "Array iterator field projection is invalid");
                                const auto field = std::find_if(itemType->fields.begin(), itemType->fields.end(),
                                                                [&](const bytecode::Type::Field& candidate)
                                                                { return module.string(candidate.name) == selector; });
                                if (field == itemType->fields.end())
                                    return fail("WVM1200", "Array iterator field projection is invalid");
                                const std::size_t fieldIndex =
                                    static_cast<std::size_t>(std::distance(itemType->fields.begin(), field));
                                selected = selected->mutableField(fieldIndex);
                            }
                        }
                        else
                        {
                            if (selector == "first")
                            {
                                selected = const_cast<Value*>(source->dictionaryKey(position));
                                mutableSelection = false;
                            }
                            else if (selector == "second")
                            {
                                selected = const_cast<Value*>(source->dictionaryValue(position));
                                mutableSelection = true;
                            }
                            else
                                return fail("WVM1201", "Dictionary iterator projection is invalid");
                        }
                    }
                    if (!selected)
                        return fail("WVM1199", "Iterator current value is unavailable");

                    if (instruction.resultType < module.types.size() &&
                        module.types[instruction.resultType].kind == TypeReference)
                    {
                        auto place = std::make_unique<PlaceStorage>();
                        place->alias = selected;
                        if (Value* source = containerSource())
                            place->owner = *source;
                        place->initialized = true;
                        place->mutableValue =
                            mutableSelection && (module.types[instruction.resultType].flags & 0x01u) != 0;
                        PlaceStorage* pointer = place.get();
                        frame.localPlaces.push_back(std::move(place));
                        if (!write(instruction.result, Value::place(pointer)))
                            return fail("WVM1009", "Iterator reference result register is invalid");
                    }
                    else if (!write(instruction.result, selected->cloneOwned()))
                        return fail("WVM1009", "Iterator value result register is invalid");
                    ++frame.instruction;
                    continue;
                }

                if (iterator->iteratorIsRange())
                {
                    Value* current = iterator->mutableIteratorState(0);
                    const Value* step = iterator->iteratorState(2);
                    const bytecode::Type& iteratorType = module.types[iterator->aggregateType()];
                    const std::uint32_t sourceType =
                        iteratorType.arguments.empty() ? bytecode::InvalidIndex : iteratorType.arguments.front();
                    const std::uint8_t sourceKind =
                        sourceType < module.types.size() ? module.types[sourceType].kind : TypeVoid;
                    const std::uint32_t width = integerWidth(sourceKind);
                    if (!current || !step)
                        return fail("WVM1198", "Range iterator state is incomplete");
                    if (current->kind() == Value::Kind::SignedInteger && step->kind() == Value::Kind::SignedInteger)
                    {
                        const std::int64_t value = current->asSignedInteger();
                        const std::int64_t amount = step->asSignedInteger();
                        const std::int64_t maximum = width == 64 ? (std::numeric_limits<std::int64_t>::max)()
                                                                 : (std::int64_t{1} << (width - 1)) - 1;
                        const std::int64_t minimum = width == 64 ? (std::numeric_limits<std::int64_t>::min)()
                                                                 : -(std::int64_t{1} << (width - 1));
                        if ((amount > 0 && value > maximum - amount) || (amount < 0 && value < minimum - amount))
                            iterator->finishIterator();
                        else
                            *current = Value::signedInteger(value + amount);
                    }
                    else if (current->kind() == Value::Kind::UnsignedInteger &&
                             step->kind() == Value::Kind::UnsignedInteger)
                    {
                        const std::uint64_t value = current->asUnsignedInteger();
                        const std::uint64_t amount = step->asUnsignedInteger();
                        const std::uint64_t maximum =
                            width == 64 ? (std::numeric_limits<std::uint64_t>::max)() : (std::uint64_t{1} << width) - 1;
                        if (value > maximum - amount)
                            iterator->finishIterator();
                        else
                            *current = Value::unsignedInteger(value + amount);
                    }
                    else
                        return fail("WVM1198", "Range iterator state is not integer-compatible");
                }
                else
                {
                    const std::optional<std::size_t> count = containerCount();
                    if (!count)
                        return fail("WVM1198", "Container iterator source is unavailable");
                    if (iterator->iteratorStep() >
                        (std::numeric_limits<std::size_t>::max)() - iterator->iteratorPosition())
                        iterator->finishIterator();
                    else
                    {
                        iterator->advanceIteratorPosition();
                        if (iterator->iteratorPosition() >= *count)
                            iterator->finishIterator();
                    }
                }
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::RangeContains)
            {
                if (instruction.operands.size() != 3)
                    return fail("WVM1022", "Range instruction has an invalid operand shape");
                const Value* value = read(instruction.operands[0]);
                const Value* start = read(instruction.operands[1]);
                const Value* end = read(instruction.operands[2]);
                if (!value || !start || !end || value->kind() != start->kind() || value->kind() != end->kind())
                    return fail("WVM1023", "Range instruction reads incompatible values");
                const bool inclusive = module.string(instruction.selector) == "inclusive";
                bool contained = false;
                if (value->kind() == Value::Kind::SignedInteger)
                    contained = value->asSignedInteger() >= start->asSignedInteger() &&
                                (inclusive ? value->asSignedInteger() <= end->asSignedInteger()
                                           : value->asSignedInteger() < end->asSignedInteger());
                else if (value->kind() == Value::Kind::UnsignedInteger)
                    contained = value->asUnsignedInteger() >= start->asUnsignedInteger() &&
                                (inclusive ? value->asUnsignedInteger() <= end->asUnsignedInteger()
                                           : value->asUnsignedInteger() < end->asUnsignedInteger());
                else if (value->kind() == Value::Kind::Float64)
                    contained =
                        value->asFloat64() >= start->asFloat64() &&
                        (inclusive ? value->asFloat64() <= end->asFloat64() : value->asFloat64() < end->asFloat64());
                else
                    return fail("WVM1024", "Range containment requires numeric runtime values");
                write(instruction.result, Value::boolean(contained));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Convert)
            {
                const Value* operand = instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                if (!operand)
                    return fail("WVM1025", "Conversion reads an unavailable value");
                const std::uint8_t target = module.types[instruction.resultType].kind;
                Value converted;
                if (isSignedType(target))
                {
                    std::uint64_t bits = 0;
                    if (operand->kind() == Value::Kind::SignedInteger)
                        bits = std::bit_cast<std::uint64_t>(operand->asSignedInteger());
                    else if (operand->kind() == Value::Kind::UnsignedInteger)
                        bits = operand->asUnsignedInteger();
                    else if (operand->kind() == Value::Kind::Float64 &&
                             operand->asFloat64() >= -9'223'372'036'854'775'808.0 &&
                             operand->asFloat64() < 9'223'372'036'854'775'808.0)
                        bits = std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(operand->asFloat64()));
                    else
                        return fail("WVM1026", "Numeric conversion is out of range or invalid");
                    converted = Value::signedInteger(signExtend(bits, integerWidth(target)));
                }
                else if (isUnsignedType(target))
                {
                    std::uint64_t bits = 0;
                    if (operand->kind() == Value::Kind::SignedInteger)
                        bits = std::bit_cast<std::uint64_t>(operand->asSignedInteger());
                    else if (operand->kind() == Value::Kind::UnsignedInteger)
                        bits = operand->asUnsignedInteger();
                    else if (operand->kind() == Value::Kind::Float64 && operand->asFloat64() >= 0.0 &&
                             operand->asFloat64() < 18'446'744'073'709'551'616.0)
                        bits = static_cast<std::uint64_t>(operand->asFloat64());
                    else
                        return fail("WVM1026", "Numeric conversion is out of range or invalid");
                    converted = Value::unsignedInteger(lowBits(bits, integerWidth(target)));
                }
                else if (isFloatType(target))
                {
                    double value = 0.0;
                    if (operand->kind() == Value::Kind::SignedInteger)
                        value = static_cast<double>(operand->asSignedInteger());
                    else if (operand->kind() == Value::Kind::UnsignedInteger)
                        value = static_cast<double>(operand->asUnsignedInteger());
                    else if (operand->kind() == Value::Kind::Float64)
                        value = operand->asFloat64();
                    else
                        return fail("WVM1026", "Numeric conversion is out of range or invalid");
                    converted =
                        Value::floating(target == TypeF32 ? static_cast<double>(static_cast<float>(value)) : value);
                }
                else
                    return fail("WVM1027", "Conversion target is not a numeric bytecode type");
                write(instruction.result, std::move(converted));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::LocalPlace)
            {
                auto storage = std::make_unique<PlaceStorage>();
                storage->mutableValue = (module.types[instruction.resultType].flags & 0x01u) != 0;
                PlaceStorage* const pointer = storage.get();
                frame.localPlaces.push_back(std::move(storage));
                if (!write(instruction.result, Value::place(pointer)))
                    return fail("WVM1038", "Local place result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::GlobalPlace)
            {
                if (instruction.global >= program_->globals.size() ||
                    !write(instruction.result, Value::place(program_->globals[instruction.global].get())))
                    return fail("WVM1039", "Global place id or result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::PlaceInit || instruction.opcode == bytecode::Opcode::Store ||
                instruction.opcode == bytecode::Opcode::Replace)
            {
                const Value* placeValue = instruction.operands.size() == 2 ? read(instruction.operands[0]) : nullptr;
                const Value* source = instruction.operands.size() == 2 ? read(instruction.operands[1]) : nullptr;
                PlaceStorage* const place = placeValue ? placeValue->asPlace() : nullptr;
                if (!place || !source)
                    return fail("WVM1040", "Place write requires an available place and source value");
                if (instruction.opcode == bytecode::Opcode::PlaceInit && place->initialized)
                    return fail("WVM1041", "Place can only be initialized once");
                if (instruction.opcode != bytecode::Opcode::PlaceInit && (!place->initialized || !place->mutableValue))
                    return fail("WVM1042", "Store requires an initialized mutable place");
                place->storedValue() = *source;
                place->initialized = true;
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Load)
            {
                const Value* placeValue = instruction.operands.size() == 1 ? read(instruction.operands[0]) : nullptr;
                PlaceStorage* const place = placeValue ? placeValue->asPlace() : nullptr;
                if (!place || !place->initialized)
                    return fail("WVM1043", "Load requires an initialized place");
                write(instruction.result, place->storedValue());
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::FieldPlace)
            {
                const Value* base = instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                if (!base)
                    return fail("WVM1063", "Field place reads an unavailable base");
                const Value* aggregate = base;
                if (base->kind() == Value::Kind::Place)
                {
                    PlaceStorage* const basePlace = base->asPlace();
                    aggregate = basePlace && basePlace->initialized ? &basePlace->storedValue() : nullptr;
                }
                if (!aggregate)
                    return fail("WVM1064", "Field place base is not initialized");

                auto storage = std::make_unique<PlaceStorage>();
                storage->owner = *aggregate;
                storage->alias = storage->owner.mutableField(instruction.projectionIndex);
                storage->initialized = storage->alias && !storage->alias->isEmpty();
                storage->mutableValue = (module.types[instruction.resultType].flags & 0x01u) != 0;
                if (!storage->alias)
                    return fail("WVM1065", "Field projection is outside the aggregate layout");
                PlaceStorage* const pointer = storage.get();
                frame.localPlaces.push_back(std::move(storage));
                write(instruction.result, Value::place(pointer));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ArrayPlace)
            {
                const Value* base = instruction.operands.size() == 2 ? read(instruction.operands[0]) : nullptr;
                const Value* rawIndex = instruction.operands.size() == 2 ? read(instruction.operands[1]) : nullptr;
                const std::optional<std::size_t> index = rawIndex ? valueIndex(*rawIndex) : std::nullopt;
                if (!base || !index)
                    return fail("WVM1066", "Array place requires an available base and non-negative index");
                const Value* array = base;
                if (base->kind() == Value::Kind::Place)
                {
                    PlaceStorage* const basePlace = base->asPlace();
                    array = basePlace && basePlace->initialized ? &basePlace->storedValue() : nullptr;
                }
                if (!array || array->kind() != Value::Kind::Array)
                    return fail("WVM1067", "Array place base is not an initialized array");

                auto storage = std::make_unique<PlaceStorage>();
                storage->owner = *array;
                storage->alias = storage->owner.mutableElement(*index);
                storage->initialized = storage->alias != nullptr;
                storage->mutableValue = (module.types[instruction.resultType].flags & 0x01u) != 0;
                if (!storage->alias)
                    return fail("WVM1068", "Array place index is outside the array bounds");
                PlaceStorage* const pointer = storage.get();
                frame.localPlaces.push_back(std::move(storage));
                write(instruction.result, Value::place(pointer));
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Borrow)
            {
                const Value* source = instruction.operands.size() == 1 ? read(instruction.operands[0]) : nullptr;
                if (!source)
                    return fail("WVM1044", "Borrow requires an available place or object value");
                if (source->kind() == Value::Kind::Place)
                    write(instruction.result, *source);
                else if (const Value* object = objectValue(source))
                    write(instruction.result, Value::objectBorrow(object->scalar_.aggregate));
                else
                    return fail("WVM1044", "Borrow requires an available place or object value");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ConstructComponent ||
                instruction.opcode == bytecode::Opcode::ConstructObject)
            {
                if (instruction.resultType >= module.types.size())
                    return fail("WVM1069", "Aggregate construction references an invalid type");
                const bytecode::Type& type = module.types[instruction.resultType];
                const bool object = instruction.opcode == bytecode::Opcode::ConstructObject;
                const std::uint8_t expected = object ? NominalObject : NominalComponent;
                if (type.kind != TypeNamed || type.nominalKind != expected)
                    return fail("WVM1070", "Aggregate construction kind does not match its result type");

                if (!write(instruction.result, constructAggregate(module, instruction.resultType, object)))
                    return fail("WVM1009", "Aggregate result register is invalid");
                Register& constructedRegister = frame.registers[instruction.result];
                Value self;
                if (object)
                    self = Value::objectBorrow(constructedRegister.value.scalar_.aggregate);
                else
                {
                    auto selfPlace = std::make_unique<PlaceStorage>();
                    selfPlace->alias = &constructedRegister.value;
                    selfPlace->initialized = true;
                    selfPlace->mutableValue = true;
                    PlaceStorage* const pointer = selfPlace.get();
                    frame.localPlaces.push_back(std::move(selfPlace));
                    self = Value::place(pointer);
                }

                std::vector<Value> constructorArguments;
                constructorArguments.reserve(instruction.operands.size() + 1);
                constructorArguments.push_back(self);
                for (const std::uint32_t operandId : instruction.operands)
                {
                    const Value* operand = read(operandId);
                    if (!operand)
                        return fail("WVM1071", "Aggregate constructor reads an unavailable argument");
                    constructorArguments.push_back(*operand);
                }

                const std::uint32_t initializer = type.fieldInitializer;
                const std::uint32_t constructor = instruction.callee;
                ++frame.instruction;
                if (initializer != bytecode::InvalidIndex)
                {
                    FrameContinuation continuation{.completion = constructor != bytecode::InvalidIndex
                                                                     ? FrameCompletion::ContinueConstruction
                                                                     : FrameCompletion::Ignore,
                                                   .nextFunction = constructor,
                                                   .nextArguments = constructorArguments};
                    const Value initializerArguments[] = {self};
                    ExecutionResult pushed =
                        pushFrame(module.functions[initializer], initializerArguments, std::move(continuation));
                    if (!pushed.succeeded())
                        return pushed;
                    continue;
                }
                if (constructor != bytecode::InvalidIndex)
                {
                    ExecutionResult pushed = pushFrame(module.functions[constructor], constructorArguments,
                                                       FrameContinuation{.completion = FrameCompletion::Ignore});
                    if (!pushed.succeeded())
                        return pushed;
                    continue;
                }
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::CopyValue || instruction.opcode == bytecode::Opcode::Retain)
            {
                const Value* source = instruction.operands.size() == 1 ? read(instruction.operands[0]) : nullptr;
                if (!source)
                    return fail("WVM1045", "Copy/retain reads an unavailable value");
                write(instruction.result,
                      instruction.opcode == bytecode::Opcode::CopyValue ? source->cloneOwned() : *source);
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::MoveValue)
            {
                const Value* placeValue = instruction.operands.size() == 1 ? read(instruction.operands[0]) : nullptr;
                PlaceStorage* const place = placeValue ? placeValue->asPlace() : nullptr;
                if (!place || !place->initialized)
                    return fail("WVM1046", "Move requires an initialized place");
                write(instruction.result, std::move(place->storedValue()));
                place->initialized = false;
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Release || instruction.opcode == bytecode::Opcode::ReleasePlace)
            {
                Register* cleanupRegister = nullptr;
                PlaceStorage* cleanupPlace = nullptr;
                Value* released = nullptr;
                if (instruction.opcode == bytecode::Opcode::Release)
                {
                    cleanupRegister =
                        instruction.operands.size() == 1 ? readRegister(instruction.operands.front()) : nullptr;
                    released = cleanupRegister ? &cleanupRegister->value : nullptr;
                }
                else
                {
                    const Value* placeValue =
                        instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                    cleanupPlace = placeValue ? placeValue->asPlace() : nullptr;
                    released = cleanupPlace && cleanupPlace->initialized ? &cleanupPlace->storedValue() : nullptr;
                }
                if (!released)
                    return fail("WVM1072", "Reference release reads an unavailable value");
                if (released->kind() != Value::Kind::Null && released->kind() != Value::Kind::Object &&
                    released->kind() != Value::Kind::Callable && released->kind() != Value::Kind::AsyncTask &&
                    released->kind() != Value::Kind::Any && released->kind() != Value::Kind::Iterator)
                    return fail("WVM1073", "Reference release requires an owned object or callable handle");

                ExecutionError cleanupError;
                cleanupValue(*released, cleanupError);
                if (!cleanupError.cleanupFailures.empty())
                {
                    ExecutionError error{"WVM1211", "Reference cleanup failed while running a destructor"};
                    error.cleanupFailures = std::move(cleanupError.cleanupFailures);
                    return failWithStack(std::move(error));
                }
                if (cleanupRegister)
                    cleanupRegister->initialized = false;
                else
                    cleanupPlace->initialized = false;
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DropValue)
            {
                Register* const source =
                    instruction.operands.size() == 1 ? readRegister(instruction.operands[0]) : nullptr;
                if (!source)
                    return fail("WVM1047", "Value cleanup reads an unavailable value");
                ExecutionError cleanupError;
                cleanupValue(source->value, cleanupError);
                if (!cleanupError.cleanupFailures.empty())
                {
                    ExecutionError error{"WVM1211", "Value cleanup failed while running a destructor"};
                    error.cleanupFailures = std::move(cleanupError.cleanupFailures);
                    return failWithStack(std::move(error));
                }
                source->initialized = false;
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DropPlace)
            {
                const Value* placeValue = instruction.operands.size() == 1 ? read(instruction.operands[0]) : nullptr;
                PlaceStorage* const place = placeValue ? placeValue->asPlace() : nullptr;
                if (!place || !place->initialized)
                    return fail("WVM1048", "Place cleanup requires an initialized place");
                ExecutionError cleanupError;
                cleanupValue(place->storedValue(), cleanupError);
                place->initialized = false;
                if (!cleanupError.cleanupFailures.empty())
                {
                    ExecutionError error{"WVM1211", "Place cleanup failed while running a destructor"};
                    error.cleanupFailures = std::move(cleanupError.cleanupFailures);
                    return failWithStack(std::move(error));
                }
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::FunctionReference)
            {
                if (instruction.callee >= module.functions.size() || instruction.resultType >= module.types.size() ||
                    module.types[instruction.resultType].kind != TypeFunction)
                    return fail("WVM1139", "Function reference requires a known function and callable type");
                if (!write(instruction.result, Value::callable(instruction.callee, {})))
                    return fail("WVM1140", "Function reference result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ClosureCreate)
            {
                if (instruction.callee >= module.functions.size() || instruction.resultType >= module.types.size() ||
                    module.types[instruction.resultType].kind != TypeFunction ||
                    instruction.operands.size() != instruction.captureKinds.size())
                    return fail("WVM1141", "Closure creation does not match a known callable capture layout");

                std::vector<Value> captures;
                captures.reserve(instruction.operands.size());
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    const Value* source = read(instruction.operands[index]);
                    if (!source)
                        return fail("WVM1142", "Closure creation reads an unavailable capture value");

                    switch (instruction.captureKinds[index])
                    {
                    case CaptureValue:
                        captures.push_back(source->cloneOwned());
                        break;
                    case CaptureReference:
                        if (source->kind() != Value::Kind::Place)
                            return fail("WVM1143", "Reference capture requires a live borrowed place");
                        captures.push_back(*source);
                        break;
                    case CaptureRetainedSelf:
                    {
                        Value retained;
                        if (source->kind() == Value::Kind::Place)
                        {
                            PlaceStorage* const place = source->asPlace();
                            if (place && place->initialized)
                                retained = place->storedValue().cloneOwned();
                        }
                        else
                            retained = source->retainObject();
                        if (retained.isEmpty())
                            return fail("WVM1144", "Retained-self capture requires a live component or object");
                        captures.push_back(std::move(retained));
                        break;
                    }
                    default:
                        return fail("WVM1145", "Closure creation contains an unknown capture kind");
                    }
                }
                if (!write(instruction.result, Value::callable(instruction.callee, std::move(captures))))
                    return fail("WVM1146", "Closure result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::IndirectCall)
            {
                const Value* callable = instruction.operands.empty() ? nullptr : read(instruction.operands.front());
                if (!callable || callable->kind() != Value::Kind::Callable)
                    return fail("WVM1147", "Indirect call requires an available function value");
                const std::uint32_t target = callable->callableFunction();
                if (target >= module.functions.size())
                    return fail("WVM1148", "Indirect call target is outside the module");

                std::vector<Value> callArguments;
                callArguments.reserve(callable->captureCount() + instruction.operands.size() - 1);
                for (std::size_t index = 0; index < callable->captureCount(); ++index)
                {
                    const Value* capture = callable->capture(index);
                    if (!capture)
                        return fail("WVM1149", "Indirect call lost a closure capture");
                    callArguments.push_back(*capture);
                }
                for (std::size_t index = 1; index < instruction.operands.size(); ++index)
                {
                    const Value* argument = read(instruction.operands[index]);
                    if (!argument)
                        return fail("WVM1150", "Indirect call reads an unavailable argument value");
                    callArguments.push_back(*argument);
                }

                const std::uint32_t result = instruction.result;
                if ((module.functions[target].flags & 0x0001u) != 0)
                {
                    if (!write(result, startTask(target, callArguments)))
                        return fail("WVM1168", "Async indirect-call result register is invalid");
                    ++frame.instruction;
                    continue;
                }
                ++frame.instruction;
                ExecutionResult pushed =
                    pushFrame(module.functions[target], callArguments,
                              FrameContinuation{.completion = FrameCompletion::ReturnValue, .callerResult = result});
                if (!pushed.succeeded())
                    return pushed;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Upcast || instruction.opcode == bytecode::Opcode::CheckedCast)
            {
                const Value* source = instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                const Value* object = objectValue(source);
                if (!object)
                    return fail("WVM1151", "Object cast requires a live object value");
                if (!isObjectType(object->aggregateType(), instruction.targetType))
                    return fail(instruction.opcode == bytecode::Opcode::Upcast ? "WVM1152" : "WVM1153",
                                instruction.opcode == bytecode::Opcode::Upcast
                                    ? "Upcast target is incompatible with the concrete object type"
                                    : "Checked object cast failed for the concrete object type");

                Value result;
                if (instruction.resultType < module.types.size() &&
                    module.types[instruction.resultType].kind == TypeReference)
                    result = Value::objectBorrow(object->scalar_.aggregate);
                else
                    result = object->retainObject();
                if (result.isEmpty() || !write(instruction.result, std::move(result)))
                    return fail("WVM1154", "Object cast result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::TypeTest)
            {
                const Value* source = instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                const Value* object = objectValue(source);
                const bool matches = object && isObjectType(object->aggregateType(), instruction.targetType);
                if (!write(instruction.result, Value::boolean(matches)))
                    return fail("WVM1155", "Object type-test result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::IdentityEqual)
            {
                if (instruction.operands.size() != 2)
                    return fail("WVM1156", "Object identity comparison requires two operands");
                const Value* left = storedValue(read(instruction.operands[0]));
                const Value* right = storedValue(read(instruction.operands[1]));
                if (!left || !right)
                    return fail("WVM1157", "Object identity comparison reads an unavailable value");

                const bool leftNull = left->kind() == Value::Kind::Null;
                const bool rightNull = right->kind() == Value::Kind::Null;
                const Value* leftObject = objectValue(left);
                const Value* rightObject = objectValue(right);
                if ((!leftNull && !leftObject) || (!rightNull && !rightObject))
                    return fail("WVM1158", "Object identity comparison requires object or null operands");
                bool equal = leftNull && rightNull;
                if (leftObject && rightObject)
                    equal = leftObject->scalar_.aggregate == rightObject->scalar_.aggregate;
                if (instruction.binaryOperator == BinaryNotEqual)
                    equal = !equal;
                else if (instruction.binaryOperator != BinaryEqual)
                    return fail("WVM1159", "Object identity comparison has an invalid comparison operator");
                if (!write(instruction.result, Value::boolean(equal)))
                    return fail("WVM1160", "Object identity comparison result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::NativeInvoke)
                return fail("WVM1203", "Native invocation requires the Sprint 20 VM bridge");
            if (instruction.opcode == bytecode::Opcode::Call || instruction.opcode == bytecode::Opcode::ExtensionCall ||
                instruction.opcode == bytecode::Opcode::MethodCall)
            {
                if (instruction.callee >= module.functions.size())
                    return fail("WVM1028", "Call target is outside the module");
                std::vector<Value> callArguments;
                callArguments.reserve(instruction.operands.size());
                for (const std::uint32_t operandId : instruction.operands)
                {
                    const Value* operand = read(operandId);
                    if (!operand)
                        return fail("WVM1029", "Call reads an unavailable argument value");
                    callArguments.push_back(*operand);
                }
                const std::uint32_t result = instruction.result;
                if ((module.functions[instruction.callee].flags & 0x0001u) != 0)
                {
                    if (!write(result, startTask(instruction.callee, callArguments)))
                        return fail("WVM1169", "Async call result register is invalid");
                    ++frame.instruction;
                    continue;
                }
                ++frame.instruction;
                ExecutionResult pushed =
                    pushFrame(module.functions[instruction.callee], callArguments,
                              FrameContinuation{.completion = FrameCompletion::ReturnValue, .callerResult = result});
                if (!pushed.succeeded())
                    return pushed;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::VirtualCall ||
                instruction.opcode == bytecode::Opcode::InterfaceCall)
            {
                const Value* receiver =
                    instruction.operands.empty() ? nullptr : objectValue(read(instruction.operands.front()));
                if (!receiver)
                    return fail("WVM1161", "Dynamic dispatch requires a live object receiver");
                const std::uint32_t target =
                    dispatchTarget(*receiver, instruction.targetType, instruction.projectionIndex);
                if (target == bytecode::InvalidIndex)
                    return fail("WVM1162", "Concrete object type has no implementation for the requested method slot");
                if (target >= module.functions.size())
                    return fail("WVM1163", "Dynamic dispatch resolved outside the module function table");

                std::vector<Value> callArguments;
                callArguments.reserve(instruction.operands.size());
                for (const std::uint32_t operandId : instruction.operands)
                {
                    const Value* operand = read(operandId);
                    if (!operand)
                        return fail("WVM1029", "Dynamic call reads an unavailable argument value");
                    callArguments.push_back(*operand);
                }
                const std::uint32_t result = instruction.result;
                if ((module.functions[target].flags & 0x0001u) != 0)
                {
                    if (!write(result, startTask(target, callArguments)))
                        return fail("WVM1170", "Async dynamic-call result register is invalid");
                    ++frame.instruction;
                    continue;
                }
                ++frame.instruction;
                ExecutionResult pushed =
                    pushFrame(module.functions[target], callArguments,
                              FrameContinuation{.completion = FrameCompletion::ReturnValue, .callerResult = result});
                if (!pushed.succeeded())
                    return pushed;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::CancellationCheck)
            {
                if (stateTask && stateTask->taskState() == AsyncTaskState::Cancelled)
                    return fail("WVM1165", "Async task was cancelled at a cooperative suspension point");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::CoroutineSuspend)
            {
                if (!frame.function->hasCoroutine ||
                    instruction.projectionIndex >= frame.function->coroutine.states.size() ||
                    instruction.targets.size() != 1)
                    return fail("WVM1171", "Coroutine suspension does not match its function state table");
                const bytecode::Function::CoroutineState& coroutineState =
                    frame.function->coroutine.states[instruction.projectionIndex];
                if (coroutineState.suspendBlock != frame.block->id ||
                    coroutineState.resumeBlock != instruction.targets.front().block)
                    return fail("WVM1172", "Coroutine suspension state does not match its resume edge");

                frame.resumedValue = {};
                frame.hasResumedValue = false;
                if (instruction.asyncOperation == AsyncAwaitTask)
                {
                    const Value* awaited =
                        instruction.operands.size() == 1 ? read(instruction.operands.front()) : nullptr;
                    if (!awaited || awaited->kind() != Value::Kind::AsyncTask)
                        return fail("WVM1173", "Coroutine await requires an async task operand");
                    const Value awaitedTask = *awaited;
                    bool attemptedStart = false;
                    for (;;)
                    {
                        const AsyncTaskState awaitedState = awaitedTask.taskState();
                        if (awaitedState == AsyncTaskState::Ready)
                        {
                            frame.resumedValue = awaitedTask.taskResult();
                            frame.hasResumedValue = true;
                            break;
                        }
                        if (awaitedState == AsyncTaskState::Cancelled)
                            return failWithStack({"WVM1165", "Awaited async task was cancelled"});
                        if (awaitedState == AsyncTaskState::Faulted)
                            return failWithStack(awaitedTask.taskError());

                        if (!attemptedStart && awaitedState == AsyncTaskState::Pending &&
                            awaitedTask.taskFunction() != bytecode::InvalidIndex)
                        {
                            attemptedStart = true;
                            ExecutionResult started = driveTask(awaitedTask);
                            if (!started.succeeded() && !started.isSuspended())
                                return failWithStack(started.error());
                            continue;
                        }

                        if (!stateTask)
                            return fail("WVM1209", "Coroutine await has no owning async task");
                        state->awaitedState = instruction.projectionIndex;
                        state->awaitedResumeBlock = coroutineState.resumeBlock;
                        if (state->executor == ExecutorKind::Inherit)
                            state->executor =
                                activeExecutor != ExecutorKind::Inherit
                                    ? activeExecutor
                                    : (program_->executors->isMainThread() ? ExecutorKind::Main : ExecutorKind::Worker);
                        program_->retainContinuation(*stateTask, state);
                        const std::weak_ptr<ExecutionState> weakState = state;
                        const std::weak_ptr<Program> weakProgram = program_;
                        const bool registered = awaitedTask.registerTaskWaiter(
                            [weakProgram, weakState](const AsyncTaskState terminalState, const Value& result,
                                                     const ExecutionError& error)
                            {
                                if (const std::shared_ptr<Program> program = weakProgram.lock())
                                {
                                    Machine* const owner = program->acquireOwner();
                                    if (!owner)
                                        return;
                                    try
                                    {
                                        owner->wakeTask(weakState, terminalState, result, error);
                                    }
                                    catch (...)
                                    {
                                        try
                                        {
                                            if (const std::shared_ptr<ExecutionState> state = weakState.lock())
                                                (void)state->activeTask.failTask(
                                                    {"WVM1210", "VM await continuation dispatch failed"});
                                        }
                                        catch (...)
                                        {
                                        }
                                    }
                                    program->releaseOwner();
                                }
                            });
                        if (registered)
                        {
                            executionLock.unlock();
                            return ExecutionResult::suspended();
                        }
                        program_->releaseContinuation(*stateTask);
                    }
                }
                else if (instruction.asyncOperation != AsyncSwitchExecutor || !instruction.operands.empty())
                    return fail("WVM1174", "Coroutine suspension carries an unsupported async operation");

                const auto resume = program_->functions[frame.function->id].blocks.find(coroutineState.resumeBlock);
                if (resume == program_->functions[frame.function->id].blocks.end())
                    return fail("WVM1175", "Coroutine resume block does not exist");
                frame.resumedState = instruction.projectionIndex;
                frame.block = resume->second;
                frame.instruction = 0;

                if (instruction.asyncOperation == AsyncSwitchExecutor)
                {
                    const std::optional<ExecutorKind> target = executorKind(instruction.asyncExecutor);
                    if (!target || !stateTask || !program_->executors)
                        return fail("WVM1208", "Coroutine executor switch has no runnable target");
                    executionLock.unlock();
                    const bool scheduled = scheduleTask(state, *target);
                    if (!scheduled)
                        return failWithStack({"WVM1208", "Coroutine executor queue rejected its continuation",
                                              frame.function->id, frame.block->id,
                                              static_cast<std::uint32_t>(frame.instruction), instruction.source});
                    return ExecutionResult::suspended();
                }
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::CoroutineResume)
            {
                if (!frame.function->hasCoroutine || frame.resumedState != instruction.projectionIndex ||
                    !frame.hasResumedValue)
                    return fail("WVM1176", "Coroutine resume has no matching suspended payload");
                if (!write(instruction.result, std::move(frame.resumedValue)))
                    return fail("WVM1177", "Coroutine resume result register is invalid");
                frame.resumedState = bytecode::InvalidIndex;
                frame.hasResumedValue = false;
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::ResultPropagate)
            {
                const Value* source =
                    instruction.operands.size() == 1 ? storedValue(read(instruction.operands.front())) : nullptr;
                const Value* present = source ? source->field(0) : nullptr;
                const Value* error = source ? source->field(2) : nullptr;
                if (!source || !present || present->kind() != Value::Kind::Boolean || present->asBoolean() || !error ||
                    instruction.targetType >= module.types.size())
                    return fail("WVM1202", "Result propagation requires a canonical error payload and target type");
                const bytecode::Type& target = module.types[instruction.targetType];
                const bool object = target.nominalKind == NominalObject;
                if (target.kind != TypeNamed || (!object && target.nominalKind != NominalComponent) ||
                    target.fields.size() < 3)
                    return fail("WVM1202", "Result propagation target has no canonical result layout");
                Value propagated = constructAggregate(module, instruction.targetType, object);
                Value* targetPresent = propagated.mutableField(0);
                Value* targetError = propagated.mutableField(2);
                if (!targetPresent || !targetError)
                    return fail("WVM1202", "Result propagation target fields are unavailable");
                *targetPresent = Value::boolean(false);
                *targetError = error->cloneOwned();
                if (std::optional<ExecutionResult> completion = completeFrame(std::move(propagated)))
                    return std::move(*completion);
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Jump || instruction.opcode == bytecode::Opcode::CondJump)
            {
                std::size_t targetIndex = 0;
                if (instruction.opcode == bytecode::Opcode::CondJump)
                {
                    const Value* condition = instruction.operands.size() == 1 ? read(instruction.operands[0]) : nullptr;
                    if (!condition || condition->kind() != Value::Kind::Boolean)
                        return fail("WVM1030", "Conditional jump requires an available bool value");
                    targetIndex = condition->asBoolean() ? 0 : 1;
                }
                const bytecode::BranchTarget& target = instruction.targets[targetIndex];
                const auto& blocks = program_->functions[frame.function->id].blocks;
                const auto blockEntry = blocks.find(target.block);
                if (blockEntry == blocks.end())
                    return fail("WVM1031", "Branch target block does not exist");
                const bytecode::Block* block = blockEntry->second;
                std::vector<Value> branchArguments;
                branchArguments.reserve(target.arguments.size());
                for (const std::uint32_t argument : target.arguments)
                {
                    const Value* value = read(argument);
                    if (!value)
                        return fail("WVM1032", "Branch reads an unavailable argument value");
                    branchArguments.push_back(*value);
                }
                for (std::size_t index = 0; index < branchArguments.size(); ++index)
                    frame.registers[block->parameters[index].value] = Register{std::move(branchArguments[index]), true};
                frame.block = block;
                frame.instruction = 0;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Return ||
                instruction.opcode == bytecode::Opcode::CoroutineComplete)
            {
                Value returned;
                if (!instruction.operands.empty())
                {
                    const Value* value = read(instruction.operands.front());
                    if (!value)
                        return fail("WVM1033", "Return reads an unavailable value");
                    returned = *value;
                }
                if (std::optional<ExecutionResult> completion = completeFrame(std::move(returned)))
                    return std::move(*completion);
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Unreachable)
                return fail("WVM1035", "Execution reached an unreachable instruction");

            return fail("WVM1036", "Opcode is not implemented by this VM runtime slice: " +
                                       std::string{bytecode::opcodeName(instruction.opcode)});
        }
        return failWithStack({"WVM1037", "VM exited without producing a result"});
    }
} // namespace wio::vm
