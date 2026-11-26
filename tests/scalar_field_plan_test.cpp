#include "wio/codegen/wir_cpp_storage_plan.h"

#include <iostream>
#include <stdexcept>

namespace
{
    using namespace wio::wir;
    using namespace wio::wir::lowered;

    Module fixture()
    {
        Module module;
        const auto scalar = module.types.i32Type();
        const auto object = module.types.internNominal({.kind = TypeKind::Named,
                                                        .name = "Counter",
                                                        .nominalKind = NominalKind::Object,
                                                        .fields = {{.name = "count", .type = scalar}},
                                                        .ownership = OwnershipModel::ReferenceCounted});
        const auto reference = module.types.intern({.kind = TypeKind::Reference,
                                                    .arguments = {scalar},
                                                    .isMutable = true,
                                                    .ownership = OwnershipModel::Borrowed});
        Function function{.id = FunctionId{0}, .returnType = scalar};
        function.parameters = {{.id = ValueId{0}, .type = object}};
        BasicBlock block{.id = BlockId{0}};
        block.instructions = {
            {.opcode = Opcode::FieldPlace,
             .result = ValueId{1},
             .resultType = reference,
             .operands = {ValueId{0}},
             .targetType = object},
            {.opcode = Opcode::Constant, .result = ValueId{2}, .resultType = scalar, .literal = std::int64_t{1}},
            {.opcode = Opcode::Load, .result = ValueId{3}, .resultType = scalar, .operands = {ValueId{1}}},
            {.opcode = Opcode::Binary,
             .result = ValueId{4},
             .resultType = scalar,
             .operands = {ValueId{3}, ValueId{2}},
             .binaryOperator = typed::BinaryOperator::Add},
            {.opcode = Opcode::Store, .operands = {ValueId{1}, ValueId{4}}},
            {.opcode = Opcode::Return, .operands = {ValueId{4}}}};
        function.blocks = {std::move(block)};
        module.functions = {std::move(function)};
        return module;
    }

    bool promoted(const Module& module)
    {
        const auto plan = wio::codegen::WirCppStoragePlan::build(module, module.functions[0]);
        return plan.scalarFieldPlaces.contains(1) && plan.plainValues.contains(1);
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        if (promoted(module))
            throw std::runtime_error(message);
    }

    void rejectsBarrier(Instruction barrier, const char* message)
    {
        rejects(
            [&](auto& module)
            {
                module.functions[0].blocks[0].instructions.insert(
                    module.functions[0].blocks[0].instructions.begin() + 2, barrier);
            },
            message);
    }
} // namespace

int main()
{
    try
    {
        if (!promoted(fixture()))
            throw std::runtime_error("closed scalar field was not promoted");
        auto component = fixture();
        const auto owner = component.functions[0].parameters[0].type;
        component.types.getMutable(owner).nominalKind = NominalKind::Component;
        if (!promoted(component))
            throw std::runtime_error("component scalar field was not promoted");
        rejects([](auto& m) { m.functions[0].isAsync = true; }, "async frame changed");
        rejects([](auto& m) { m.functions[0].coroutine = CoroutineLayout{}; }, "coroutine frame changed");
        rejects([](auto& m)
                { m.types.getMutable(m.functions[0].parameters[0].type).nativeBinding = NativeTypeBinding{}; },
                "native field was treated as a Wio field");
        rejects(
            [](auto& m)
            {
                m.types.getMutable(m.functions[0].parameters[0].type).nominalRepresentation =
                    NominalRepresentation::NativePod;
            },
            "native POD field was promoted");
        rejectsBarrier({.opcode = Opcode::Call}, "call crossed field lifetime interval");
        rejectsBarrier({.opcode = Opcode::NativeInvoke}, "native call crossed interval");
        rejectsBarrier({.opcode = Opcode::Release, .operands = {ValueId{0}}}, "receiver release crossed interval");
        rejectsBarrier({.opcode = Opcode::Load,
                        .result = ValueId{8},
                        .resultType = fixture().types.i32Type(),
                        .operands = {ValueId{9}}},
                       "unknown proxy getter crossed interval");
        rejectsBarrier({.opcode = Opcode::Store, .operands = {ValueId{9}, ValueId{2}}},
                       "unknown proxy writer crossed interval");
        rejectsBarrier({.opcode = Opcode::Borrow, .operands = {ValueId{1}}}, "borrow escaped the field");
        rejectsBarrier({.opcode = Opcode::ClosureCreate, .operands = {ValueId{1}}}, "field capture lost identity");
        rejectsBarrier({.opcode = Opcode::Unary, .borrowOrigin = ValueId{1}}, "borrow metadata lost identity");
        rejects(
            [](auto& m)
            {
                m.functions[0].blocks[0].instructions.back().targets = {
                    BranchTarget{.block = BlockId{1}, .arguments = {ValueId{1}}}};
            },
            "CFG edge carried raw field");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                code.insert(code.begin() + 2, 33, code[1]);
            },
            "unbounded proof window was promoted");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
