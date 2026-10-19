#include "wio/codegen/wir_cpp_storage_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::lowered;
    using Plan = wio::codegen::WirCppStoragePlan;

    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    Module fixture()
    {
        Module module;
        const auto byte = module.types.intern({.kind = TypeKind::U8});
        const auto array = module.types.intern(
            {.kind = TypeKind::Array, .arguments = {byte}, .ownership = OwnershipModel::OwnedValue});
        const auto view = module.types.intern(
            {.kind = TypeKind::Reference, .arguments = {array}, .ownership = OwnershipModel::Borrowed});
        Function caller{.id = FunctionId{0}, .returnType = module.types.i32Type()};
        BasicBlock body{.id = BlockId{0}};
        body.instructions = {{.opcode = Opcode::LocalPlace,
                              .result = ValueId{0},
                              .resultType = view,
                              .storageClass = StorageClass::Stack,
                              .escapeClass = EscapeClass::Store},
                             {.opcode = Opcode::NativeInvoke,
                              .result = ValueId{1},
                              .resultType = module.types.i32Type(),
                              .operands = {ValueId{0}},
                              .callee = FunctionId{1},
                              .signatureTypes = {view}},
                             {.opcode = Opcode::DropPlace, .operands = {ValueId{0}}},
                             {.opcode = Opcode::Return, .operands = {ValueId{1}}}};
        caller.blocks.push_back(std::move(body));
        Function native{.id = FunctionId{1}, .returnType = module.types.i32Type(), .isExternal = true};
        native.parameters = {{.id = ValueId{0}, .type = view}};
        native.nativeBinding = NativeBinding{};
        native.nativeBinding->parameters = {
            {.type = view, .passing = NativePassingMode::Borrow, .marshalling = NativeMarshallingKind::RuntimeValue}};
        native.nativeBinding->result.type = native.returnType;
        module.functions = {std::move(caller), std::move(native)};
        return module;
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        expect(!Plan::build(module, module.functions[0]).byteArrayPlaces.contains(0), message);
    }

    template <class Edit> void changeArray(Module& module, Edit edit)
    {
        auto view = module.types.get(module.functions[1].parameters[0].type);
        auto array = module.types.get(view.arguments.front());
        edit(array, module);
        view.arguments = {module.types.intern(std::move(array))};
        const auto type = module.types.intern(std::move(view));
        module.functions[0].blocks[0].instructions[0].resultType = type;
        module.functions[0].blocks[0].instructions[1].signatureTypes = {type};
        module.functions[1].parameters[0].type = type;
        module.functions[1].nativeBinding->parameters[0].type = type;
    }
} // namespace

int main()
{
    try
    {
        auto module = fixture();
        expect(Plan::supportsCallScopedByteArrayAbi(module, module.functions[1]), "byte leaf ABI rejected");
        const auto plan = Plan::build(module, module.functions[0]);
        expect(plan.byteArrayPlaces.contains(0) && plan.plainValues.contains(0), "closed byte cell kept heap identity");
        expect(!Plan::supportsCallScopedStringAbi(module, module.functions[1]), "byte view entered string-only proof");
        auto byteAlias = fixture();
        changeArray(byteAlias,
                    [](auto& array, auto& m) { array.arguments = {m.types.intern({.kind = TypeKind::Byte})}; });
        expect(Plan::build(byteAlias, byteAlias.functions[0]).byteArrayPlaces.contains(0), "byte alias rejected");
        rejects([](auto& m) { changeArray(m, [](auto& a, auto&) { a.staticExtent = 8; }); },
                "fixed array entered proof");
        rejects([](auto& m) { changeArray(m, [](auto& a, auto&) { a.ownership = OwnershipModel::Generic; }); },
                "generic ownership entered proof");
        rejects([](auto& m) { changeArray(m, [](auto& a, auto& t) { a.arguments = {t.types.stringType()}; }); },
                "non-byte array entered proof");
        rejects([](auto& m) { m.functions[1].isAsync = true; }, "async native entered leaf proof");
        rejects([](auto& m) { m.functions[0].isAsync = true; }, "async frame changed");
        rejects([](auto& m) { m.functions[1].nativeBinding.reset(); }, "unknown boundary entered proof");
        rejects([](auto& m) { m.functions[1].isExternal = false; }, "Wio call entered leaf proof");
        rejects([](auto& m) { m.functions[1].isMethod = true; }, "method entered leaf proof");
        rejects([](auto& m) { m.functions[1].isExtension = true; }, "extension entered leaf proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->receiver = NativeReceiverKind::ConstReference; },
                "receiver entered leaf proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->parameters[0].passing = NativePassingMode::Value; },
                "owning native argument entered borrow proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->parameters[0].passing = NativePassingMode::BorrowMut; },
                "mutable borrow entered immutable byte proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->parameters[0].nullable = true; }, "nullable entered proof");
        rejects([](auto& m)
                { m.functions[1].nativeBinding->parameters[0].callbackLifetime = NativeCallbackLifetime::Retained; },
                "retained borrow entered proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->parameters[0].callbackThread = NativeCallbackThread::Any; },
                "foreign-thread borrow entered proof");
        rejects([](auto& m)
                { m.functions[1].nativeBinding->parameters[0].marshalling = NativeMarshallingKind::Callback; },
                "callback entered proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->result.passing = NativePassingMode::Borrow; },
                "borrowed result entered proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->result.marshalling = NativeMarshallingKind::RuntimeValue; },
                "runtime result entered leaf proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->result.nullable = true; }, "nullable result entered proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->result.type = m.types.stringType(); },
                "mismatched result contract entered proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->language = NativeSymbolLanguage::C; },
                "C ABI entered proof");
        rejects([](auto& m) { m.functions[1].nativeBinding->callingConvention = NativeCallingConvention::StdCall; },
                "non-default ABI entered proof");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].storageClass = StorageClass::Heap; },
                "heap cell was promoted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[1].signatureTypes.clear(); },
                "unproven call signature entered proof");
        for (const auto opcode :
             {Opcode::Borrow, Opcode::Call, Opcode::IndirectCall, Opcode::ClosureCreate, Opcode::Load, Opcode::Return})
        {
            rejects(
                [&](auto& m)
                {
                    m.functions[0].blocks[0].instructions.insert(
                        m.functions[0].blocks[0].instructions.begin() + 1,
                        Instruction{.opcode = opcode, .operands = {ValueId{0}}});
                },
                "identity-observing/escaping cell was promoted");
        }
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[1].borrowOrigin = ValueId{0}; },
                "borrow origin lost identity");
        rejects(
            [](auto& m)
            { m.functions[0].blocks[0].instructions[1].targets = {{.block = BlockId{1}, .arguments = {ValueId{0}}}}; },
            "cell passed on CFG edge was promoted");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
