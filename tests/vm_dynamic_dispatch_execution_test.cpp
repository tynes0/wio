#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    namespace bytecode = wio::bytecode;
    namespace vm = wio::vm;

    constexpr std::uint32_t Void = 0;
    constexpr std::uint32_t Bool = 1;
    constexpr std::uint32_t I32 = 2;
    constexpr std::uint32_t Base = 3;
    constexpr std::uint32_t Derived = 4;
    constexpr std::uint32_t Readable = 5;
    constexpr std::uint32_t RefBase = 6;
    constexpr std::uint32_t RefDerived = 7;
    constexpr std::uint32_t RefReadable = 8;
    constexpr std::uint32_t ReadSlot = 4;

    bool expect(const bool condition, const std::string_view message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    bytecode::Instruction instruction(const bytecode::Opcode opcode)
    {
        bytecode::Instruction result;
        result.opcode = opcode;
        result.constant = 0;
        return result;
    }

    bytecode::Instruction constant(const std::uint32_t result, const bytecode::ConstantId value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Constant);
        output.result = result;
        output.resultType = I32;
        output.constant = value;
        return output;
    }

    bytecode::Instruction returnValue(const std::uint32_t value)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::Return);
        output.operands = {value};
        return output;
    }

    bytecode::Parameter parameter(const std::uint32_t value, const std::uint32_t type)
    {
        return bytecode::Parameter{.value = value, .type = type};
    }

    bytecode::Function function(const std::uint32_t id, const std::uint32_t returnType,
                                std::vector<bytecode::Parameter> parameters,
                                std::vector<bytecode::Instruction> instructions)
    {
        bytecode::Function output;
        output.id = id;
        output.returnType = returnType;
        output.parameters = std::move(parameters);
        bytecode::Block entry;
        entry.id = 0;
        entry.instructions = std::move(instructions);
        output.blocks.push_back(std::move(entry));
        return output;
    }

    bytecode::Instruction constructObject(const std::uint32_t result, const std::uint32_t type)
    {
        bytecode::Instruction output = instruction(bytecode::Opcode::ConstructObject);
        output.result = result;
        output.resultType = type;
        return output;
    }

    bytecode::Instruction dynamicCall(const bytecode::Opcode opcode, const std::uint32_t result,
                                      const std::uint32_t receiver, const std::uint32_t contract,
                                      const std::uint32_t declaration)
    {
        bytecode::Instruction output = instruction(opcode);
        output.result = result;
        output.resultType = I32;
        output.operands = {receiver};
        output.callee = declaration;
        output.targetType = contract;
        output.projectionIndex = ReadSlot;
        output.signatureTypes = {contract == Base ? RefBase : RefReadable};
        return output;
    }

    bytecode::Instruction cast(const bytecode::Opcode opcode, const std::uint32_t result,
                               const std::uint32_t resultType, const std::uint32_t operand,
                               const std::uint32_t targetType)
    {
        bytecode::Instruction output = instruction(opcode);
        output.result = result;
        output.resultType = resultType;
        output.operands = {operand};
        output.targetType = targetType;
        return output;
    }

    bytecode::Module makeModule()
    {
        bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.constants = {
            bytecode::Constant{},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 1},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 2},
            bytecode::Constant{.kind = bytecode::ConstantKind::SignedInteger, .bits = 7},
        };

        bytecode::Type base{.kind = 28, .nominalKind = 2, .ownership = 2, .cleanup = 2};
        base.castTypes = {Base};
        base.methods = {
            bytecode::Type::Method{.parameterTypes = {}, .returnType = I32, .function = 0, .slot = ReadSlot}};
        base.dispatchEntries = {{.contractType = Base, .slot = ReadSlot, .implementation = 0}};

        bytecode::Type derived{.kind = 28, .nominalKind = 2, .ownership = 2, .cleanup = 2};
        derived.baseTypes = {Base, Readable};
        derived.castTypes = {Derived, Base, Readable};
        derived.methods = {
            bytecode::Type::Method{.parameterTypes = {}, .returnType = I32, .function = 2, .slot = ReadSlot}};
        derived.dispatchEntries = {
            {.contractType = Derived, .slot = ReadSlot, .implementation = 2},
            {.contractType = Base, .slot = ReadSlot, .implementation = 2},
            {.contractType = Readable, .slot = ReadSlot, .implementation = 2},
        };

        bytecode::Type readable{.kind = 28, .nominalKind = 3};
        readable.castTypes = {Readable};
        readable.methods = {bytecode::Type::Method{
            .parameterTypes = {}, .returnType = I32, .function = 1, .slot = ReadSlot, .isAbstract = true}};

        module.types = {
            bytecode::Type{.kind = 1},
            bytecode::Type{.kind = 2},
            bytecode::Type{.kind = 5},
            std::move(base),
            std::move(derived),
            std::move(readable),
            bytecode::Type{.kind = 29, .arguments = {Base}, .flags = 0x01u},
            bytecode::Type{.kind = 29, .arguments = {Derived}, .flags = 0x01u},
            bytecode::Type{.kind = 29, .arguments = {Readable}, .flags = 0x01u},
        };

        module.functions.push_back(function(0, I32, {parameter(0, RefBase)}, {constant(1, 1), returnValue(1)}));
        module.functions.back().ownerType = Base;
        module.functions.back().methodSlot = ReadSlot;

        module.functions.push_back(function(1, I32, {parameter(0, RefReadable)}, {constant(1, 2), returnValue(1)}));
        module.functions.back().ownerType = Readable;
        module.functions.back().methodSlot = ReadSlot;

        bytecode::Instruction borrow = instruction(bytecode::Opcode::Borrow);
        borrow.result = 1;
        borrow.resultType = RefDerived;
        borrow.operands = {0};
        module.functions.push_back(
            function(2, I32, {parameter(0, RefDerived)}, {borrow, constant(2, 3), returnValue(2)}));
        module.functions.back().ownerType = Derived;
        module.functions.back().methodSlot = ReadSlot;

        bytecode::Instruction add = instruction(bytecode::Opcode::Binary);
        add.result = 3;
        add.resultType = I32;
        add.operands = {1, 2};
        add.binaryOperator = 0;
        module.functions.push_back(
            function(3, I32, {},
                     {constructObject(0, Derived), dynamicCall(bytecode::Opcode::VirtualCall, 1, 0, Base, 0),
                      dynamicCall(bytecode::Opcode::InterfaceCall, 2, 0, Readable, 1), add, returnValue(3)}));

        bytecode::Instruction typeTest = cast(bytecode::Opcode::TypeTest, 1, Bool, 0, RefReadable);
        module.functions.push_back(function(4, Bool, {}, {constructObject(0, Derived), typeTest, returnValue(1)}));

        bytecode::Instruction identity = instruction(bytecode::Opcode::IdentityEqual);
        identity.result = 2;
        identity.resultType = Bool;
        identity.operands = {0, 1};
        identity.binaryOperator = 5;
        module.functions.push_back(
            function(5, Bool, {},
                     {constructObject(0, Derived), cast(bytecode::Opcode::Upcast, 1, RefBase, 0, RefBase), identity,
                      returnValue(2)}));

        module.functions.push_back(
            function(6, I32, {},
                     {constructObject(0, Derived), cast(bytecode::Opcode::CheckedCast, 1, RefBase, 0, RefBase),
                      dynamicCall(bytecode::Opcode::VirtualCall, 2, 1, Base, 0), returnValue(2)}));

        module.functions.push_back(
            function(7, Void, {},
                     {constructObject(0, Base), cast(bytecode::Opcode::CheckedCast, 1, RefDerived, 0, RefDerived),
                      instruction(bytecode::Opcode::Return)}));

        module.functions.push_back(function(
            8, Bool, {},
            {constructObject(0, Base), cast(bytecode::Opcode::TypeTest, 1, Bool, 0, RefDerived), returnValue(1)}));

        bytecode::Instruction distinctIdentity = instruction(bytecode::Opcode::IdentityEqual);
        distinctIdentity.result = 2;
        distinctIdentity.resultType = Bool;
        distinctIdentity.operands = {0, 1};
        distinctIdentity.binaryOperator = 5;
        module.functions.push_back(function(
            9, Bool, {}, {constructObject(0, Derived), constructObject(1, Derived), distinctIdentity, returnValue(2)}));
        return module;
    }
} // namespace

int main(const int argc, const char* const* argv)
{
    bool ok = true;
    const bytecode::DecodeResult decoded = bytecode::decode(bytecode::encode(makeModule()));
    ok &= expect(decoded.succeeded(), "Dynamic-dispatch fixture must survive the real .wiob codec");
    if (!decoded.succeeded())
        return 1;

    vm::Machine machine{decoded.module};
    const vm::ExecutionResult dispatched = machine.invoke(3);
    if (!dispatched.succeeded())
        std::cerr << dispatched.error().code << ": " << dispatched.error().message << '\n';
    ok &= expect(dispatched.succeeded() && dispatched.value().asSignedInteger() == 14,
                 "Virtual and interface calls must use the concrete receiver implementation");

    const vm::ExecutionResult tested = machine.invoke(4);
    if (!tested.succeeded())
        std::cerr << tested.error().code << ": " << tested.error().message << '\n';
    ok &= expect(tested.succeeded() && tested.value().asBoolean(),
                 "TypeTest must use the concrete object's transitive cast table");

    const vm::ExecutionResult identity = machine.invoke(5);
    if (!identity.succeeded())
        std::cerr << identity.error().code << ": " << identity.error().message << '\n';
    ok &= expect(identity.succeeded() && identity.value().asBoolean(),
                 "Upcasts must preserve object identity across owned and borrowed handles");

    const vm::ExecutionResult checked = machine.invoke(6);
    if (!checked.succeeded())
        std::cerr << checked.error().code << ": " << checked.error().message << '\n';
    ok &= expect(checked.succeeded() && checked.value().asSignedInteger() == 7,
                 "A successful checked cast must remain dynamically dispatchable");

    const vm::ExecutionResult failed = machine.invoke(7);
    ok &= expect(!failed.succeeded() && failed.error().code == "WVM1153",
                 "An incompatible checked cast must fail with a stable VM diagnostic");

    const vm::ExecutionResult rejectedType = machine.invoke(8);
    ok &= expect(rejectedType.succeeded() && !rejectedType.value().asBoolean(),
                 "TypeTest must return false for an incompatible concrete object");

    const vm::ExecutionResult distinct = machine.invoke(9);
    ok &= expect(distinct.succeeded() && !distinct.value().asBoolean(),
                 "Separately allocated objects must have distinct identities");

    bytecode::Module malformed = decoded.module;
    malformed.types[Derived].dispatchEntries.front().slot = 99;
    vm::Machine malformedMachine{malformed};
    const vm::ExecutionResult rejectedModule = malformedMachine.invoke(3);
    ok &= expect(!rejectedModule.succeeded() && rejectedModule.error().code == "WVM1001" &&
                     rejectedModule.error().message.find("WBC1052") != std::string::npos,
                 "Malformed dispatch metadata must be rejected while the bytecode module is loaded");

    if (argc == 2)
    {
        std::ifstream stream{argv[1], std::ios::binary};
        const std::vector<char> raw{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        std::vector<std::byte> bytes;
        bytes.reserve(raw.size());
        for (const char value : raw)
            bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
        const bytecode::DecodeResult realSource = bytecode::decode(bytes);
        ok &= expect(realSource.succeeded(), "Real-source dynamic-dispatch bytecode must decode");
        if (realSource.succeeded())
        {
            const auto entry = std::ranges::find_if(realSource.module.functions,
                                                    [&](const bytecode::Function& function)
                                                    {
                                                        return function.name < realSource.module.strings.size() &&
                                                               realSource.module.strings[function.name] == "Entry";
                                                    });
            ok &= expect(entry != realSource.module.functions.end(), "Real-source bytecode must contain Entry");
            if (entry != realSource.module.functions.end())
            {
                vm::Machine realMachine{realSource.module};
                const vm::ExecutionResult result = realMachine.invoke(entry->id);
                if (!result.succeeded())
                    std::cerr << result.error().code << ": " << result.error().message << " (function "
                              << result.error().function << ", block " << result.error().block << ", instruction "
                              << result.error().instruction << ")\n";
                ok &= expect(result.succeeded() && result.value().asSignedInteger() == 14,
                             "Compiler-produced virtual and interface calls must execute in the VM");
            }
        }
    }
    return ok ? 0 : 1;
}
