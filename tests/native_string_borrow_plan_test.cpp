#include "wio/codegen/wir_cpp_storage_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::lowered;

    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    Module fixture()
    {
        Module module;
        const auto string = module.types.stringType();
        const auto reference = module.types.intern(
            {.kind = TypeKind::Reference, .arguments = {string}, .ownership = OwnershipModel::Borrowed});
        Function caller{.id = FunctionId{0}, .returnType = module.types.i32Type()};
        BasicBlock body{.id = BlockId{0}};
        body.instructions = {{.opcode = Opcode::LocalPlace,
                              .result = ValueId{0},
                              .resultType = reference,
                              .storageClass = StorageClass::Stack,
                              .escapeClass = EscapeClass::Store},
                             {.opcode = Opcode::NativeInvoke,
                              .result = ValueId{1},
                              .resultType = module.types.i32Type(),
                              .operands = {ValueId{0}},
                              .callee = FunctionId{1},
                              .signatureTypes = {reference}},
                             {.opcode = Opcode::DropPlace, .operands = {ValueId{0}}},
                             {.opcode = Opcode::Return, .operands = {ValueId{1}}}};
        caller.blocks.push_back(std::move(body));
        Function native{.id = FunctionId{1}, .returnType = module.types.i32Type(), .isExternal = true};
        native.parameters = {{.id = ValueId{0}, .type = reference}};
        native.nativeBinding = NativeBinding{};
        native.nativeBinding->parameters = {{.type = reference,
                                             .passing = NativePassingMode::Borrow,
                                             .marshalling = NativeMarshallingKind::Utf8String}};
        native.nativeBinding->result.type = native.returnType;
        module.functions = {std::move(caller), std::move(native)};
        return module;
    }

    bool inlineCell(const Module& module)
    {
        return wio::codegen::WirCppStoragePlan::build(module, module.functions[0]).stringPlaces.contains(0);
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        expect(!inlineCell(module), message);
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        expect(inlineCell(module), "call-scoped string view kept a heap identity");
        auto mutableCase = fixture();
        auto reference = mutableCase.types.get(mutableCase.functions[1].parameters[0].type);
        reference.isMutable = true;
        const auto mutableType = mutableCase.types.intern(reference);
        mutableCase.functions[0].blocks[0].instructions[0].resultType = mutableType;
        mutableCase.functions[0].blocks[0].instructions[1].signatureTypes = {mutableType};
        mutableCase.functions[1].parameters[0].type = mutableType;
        mutableCase.functions[1].nativeBinding->parameters[0].type = mutableType;
        mutableCase.functions[1].nativeBinding->parameters[0].passing = NativePassingMode::BorrowMut;
        expect(inlineCell(mutableCase), "call-scoped mutable string borrow kept a heap identity");
        rejects([](auto& value) { value.functions[1].nativeBinding.reset(); }, "unknown call was treated as leaf");
        rejects([](auto& value) { value.functions[1].isExternal = false; }, "Wio call was treated as native leaf");
        rejects([](auto& value) { value.functions[0].isAsync = true; }, "async frame was changed");
        rejects([](auto& value) { value.functions[1].nativeBinding->receiver = NativeReceiverKind::MutableReference; },
                "native receiver escaped the leaf proof");
        rejects([](auto& value) { value.functions[1].nativeBinding->parameters[0].passing = NativePassingMode::Value; },
                "owning native transport was mistaken for a borrow");
        rejects([](auto& value) { value.functions[1].nativeBinding->parameters[0].nullable = true; },
                "nullable native transport entered the proof");
        rejects(
            [](auto& value)
            { value.functions[1].nativeBinding->parameters[0].callbackLifetime = NativeCallbackLifetime::Retained; },
            "retained transport entered the call-scoped proof");
        rejects([](auto& value)
                { value.functions[1].nativeBinding->parameters[0].callbackThread = NativeCallbackThread::Any; },
                "foreign-thread transport entered the call-scoped proof");
        rejects([](auto& value)
                { value.functions[1].nativeBinding->parameters[0].marshalling = NativeMarshallingKind::Callback; },
                "callback transport entered the leaf proof");
        rejects([](auto& value)
                { value.functions[1].nativeBinding->result.marshalling = NativeMarshallingKind::Utf8String; },
                "returning string/borrow entered the proof");
        rejects([](auto& value) { value.functions[1].nativeBinding->result.passing = NativePassingMode::Borrow; },
                "borrowed result entered the proof");
        rejects(
            [](auto& value)
            {
                value.functions[0].blocks[0].instructions.insert(
                    value.functions[0].blocks[0].instructions.begin() + 1,
                    Instruction{.opcode = Opcode::Borrow, .operands = {ValueId{0}}});
            },
            "escaping/reborrowed cell lost identity");
        rejects(
            [](auto& value)
            {
                value.functions[0].blocks[0].instructions[1].targets = {
                    BranchTarget{.block = BlockId{1}, .arguments = {ValueId{0}}}};
            },
            "cell carried on a CFG edge lost identity");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
