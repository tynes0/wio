#include "wio/vm/machine.h"

#include "wio/bytecode/format.h"
#include "wio/bytecode/verifier.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wio::vm
{
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
        constexpr std::uint8_t TypeNullable = 30;

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

        struct Register
        {
            Value value;
            bool initialized = false;
        };

        struct Frame
        {
            const bytecode::Function* function = nullptr;
            const bytecode::Block* block = nullptr;
            std::size_t instruction = 0;
            std::vector<Register> registers;
            std::uint32_t callerResult = bytecode::InvalidIndex;
        };

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

        [[nodiscard]] Value constantValue(const bytecode::Constant& constant, const bytecode::Module& module)
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
                return Value::string(std::string{module.string(constant.string)});
            }
            return {};
        }

        [[nodiscard]] Value defaultValue(const std::uint8_t kind)
        {
            if (kind == TypeBool)
                return Value::boolean(false);
            if (isSignedType(kind))
                return Value::signedInteger(0);
            if (isUnsignedType(kind))
                return Value::unsignedInteger(0);
            if (isFloatType(kind))
                return Value::floating(0.0);
            if (kind == TypeString || kind == TypeText)
                return Value::string({});
            if (kind == TypeNullable)
                return Value::null();
            return {};
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
    } // namespace

    struct Machine::Program
    {
        struct FunctionPlan
        {
            std::uint32_t registerCount = 0;
            std::unordered_map<std::uint32_t, const bytecode::Block*> blocks;
        };

        const bytecode::Module* module = nullptr;
        ExecutionError loadError;
        std::vector<FunctionPlan> functions;
        bool valid = false;
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

    Machine::Machine(const bytecode::Module& module, const MachineOptions options)
        : program_(std::make_unique<Program>()), options_(options)
    {
        program_->module = &module;
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
        program_->valid = true;
    }

    Machine::~Machine() = default;
    Machine::Machine(Machine&&) noexcept = default;
    Machine& Machine::operator=(Machine&&) noexcept = default;

    ExecutionResult Machine::invoke(const std::uint32_t functionId, const std::span<const Value> arguments) const
    {
        if (!program_ || !program_->module)
            return ExecutionResult::failure({"WVM1000", "The virtual machine has no loaded module"});
        if (!program_->valid)
            return ExecutionResult::failure(program_->loadError);
        const bytecode::Module& module = *program_->module;
        if (functionId >= module.functions.size())
            return ExecutionResult::failure({"WVM1002", "Entry function id is outside the module"});

        std::vector<Frame> frames;
        frames.reserve(16);
        auto pushFrame = [&](const bytecode::Function& function, const std::span<const Value> values,
                             const std::uint32_t callerResult) -> ExecutionResult
        {
            if ((function.flags & 0x0002u) != 0)
                return ExecutionResult::failure(
                    {"WVM1003", "External function requires the Sprint 20 native bridge", function.id});
            if (values.size() != function.parameters.size())
                return ExecutionResult::failure(
                    {"WVM1004", "Call argument count does not match the function signature", function.id});
            if (frames.size() >= options_.callDepthLimit)
                return ExecutionResult::failure({"WVM1005", "VM call depth limit exceeded", function.id});
            const std::uint32_t count = program_->functions[function.id].registerCount;
            if (count > options_.registerLimitPerFrame)
                return ExecutionResult::failure({"WVM1006", "Function register limit exceeded", function.id});

            Frame frame;
            frame.function = &function;
            frame.block = &function.blocks.front();
            frame.registers.resize(count);
            frame.callerResult = callerResult;
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                const std::uint32_t id = function.parameters[index].value;
                frame.registers[id] = Register{values[index], true};
            }
            frames.push_back(std::move(frame));
            return ExecutionResult::success();
        };

        ExecutionResult initial = pushFrame(module.functions[functionId], arguments, bytecode::InvalidIndex);
        if (!initial.succeeded())
            return initial;

        std::uint64_t executed = 0;
        while (!frames.empty())
        {
            Frame& frame = frames.back();
            if (frame.instruction >= frame.block->instructions.size())
                return ExecutionResult::failure(
                    {"WVM1007", "Instruction cursor escaped its basic block", frame.function->id, frame.block->id});
            if (executed++ >= options_.instructionLimit)
                return ExecutionResult::failure({"WVM1008", "VM instruction limit exceeded", frame.function->id,
                                                 frame.block->id, static_cast<std::uint32_t>(frame.instruction)});

            const bytecode::Instruction& instruction = frame.block->instructions[frame.instruction];
            const auto fail = [&](std::string code, std::string message)
            {
                return ExecutionResult::failure({std::move(code), std::move(message), frame.function->id,
                                                 frame.block->id, static_cast<std::uint32_t>(frame.instruction),
                                                 instruction.source});
            };
            const auto read = [&](const std::uint32_t id) -> const Value*
            {
                return id < frame.registers.size() && frame.registers[id].initialized ? &frame.registers[id].value
                                                                                      : nullptr;
            };
            const auto write = [&](const std::uint32_t id, Value value) -> bool
            {
                if (id >= frame.registers.size())
                    return false;
                frame.registers[id] = Register{std::move(value), true};
                return true;
            };

            if (instruction.opcode == bytecode::Opcode::Constant)
            {
                if (!write(instruction.result, constantValue(module.constants[instruction.constant], module)))
                    return fail("WVM1009", "Constant result register is invalid");
                ++frame.instruction;
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::DefaultValue)
            {
                const std::uint8_t type = module.types[instruction.resultType].kind;
                Value value = defaultValue(type);
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
                else
                    return fail("WVM1021", "Binary operator is invalid for the runtime value kind");
                write(instruction.result, std::move(result));
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
            if (instruction.opcode == bytecode::Opcode::Call)
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
                ++frame.instruction;
                ExecutionResult pushed = pushFrame(module.functions[instruction.callee], callArguments, result);
                if (!pushed.succeeded())
                    return pushed;
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
            if (instruction.opcode == bytecode::Opcode::Return)
            {
                Value returned;
                if (!instruction.operands.empty())
                {
                    const Value* value = read(instruction.operands.front());
                    if (!value)
                        return fail("WVM1033", "Return reads an unavailable value");
                    returned = *value;
                }
                const std::uint32_t callerResult = frame.callerResult;
                frames.pop_back();
                if (frames.empty())
                    return ExecutionResult::success(std::move(returned));
                if (callerResult != bytecode::InvalidIndex)
                {
                    Frame& caller = frames.back();
                    if (callerResult >= caller.registers.size())
                        return ExecutionResult::failure(
                            {"WVM1034", "Call result register is invalid", caller.function->id, caller.block->id});
                    caller.registers[callerResult] = Register{std::move(returned), true};
                }
                continue;
            }
            if (instruction.opcode == bytecode::Opcode::Unreachable)
                return fail("WVM1035", "Execution reached an unreachable instruction");

            return fail("WVM1036", "Opcode is not implemented by this VM runtime slice: " +
                                       std::string{bytecode::opcodeName(instruction.opcode)});
        }
        return ExecutionResult::failure({"WVM1037", "VM exited without producing a result"});
    }
} // namespace wio::vm
