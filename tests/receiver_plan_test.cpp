#include "wio/codegen/wir_cpp_receiver_plan.h"

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
        const auto root = module.types.internNominal({.kind = TypeKind::Named,
                                                      .name = "object",
                                                      .nominalKind = NominalKind::Object,
                                                      .ownership = OwnershipModel::ReferenceCounted});
        const auto object = module.types.internNominal({.kind = TypeKind::Named,
                                                        .name = "Counter",
                                                        .nominalKind = NominalKind::Object,
                                                        .baseTypes = {root},
                                                        .fields = {{.name = "count", .type = scalar}},
                                                        .ownership = OwnershipModel::ReferenceCounted});
        const auto self = module.types.intern({.kind = TypeKind::Reference,
                                               .arguments = {object},
                                               .isMutable = true,
                                               .ownership = OwnershipModel::Borrowed});
        const auto view = module.types.intern(
            {.kind = TypeKind::Reference, .arguments = {object}, .ownership = OwnershipModel::Borrowed});
        const auto field = module.types.intern({.kind = TypeKind::Reference,
                                                .arguments = {scalar},
                                                .isMutable = true,
                                                .ownership = OwnershipModel::Borrowed});
        Function function{
            .id = FunctionId{0}, .returnType = module.types.stringType(), .ownerType = object, .isMethod = true};
        function.parameters = {{.id = ValueId{0}, .type = self, .ownership = typed::ValueOwnership::Borrowed}};
        BasicBlock block{.id = BlockId{0}};
        block.instructions = {
            {.opcode = Opcode::Borrow, .result = ValueId{1}, .resultType = view, .operands = {ValueId{0}}},
            {.opcode = Opcode::FieldPlace,
             .result = ValueId{2},
             .resultType = field,
             .operands = {ValueId{1}},
             .targetType = object},
            {.opcode = Opcode::Load, .result = ValueId{3}, .resultType = scalar, .operands = {ValueId{2}}},
            {.opcode = Opcode::Constant, .result = ValueId{4}, .resultType = scalar, .literal = std::int64_t{1}},
            {.opcode = Opcode::Binary,
             .result = ValueId{5},
             .resultType = scalar,
             .operands = {ValueId{3}, ValueId{4}},
             .binaryOperator = typed::BinaryOperator::Add},
            {.opcode = Opcode::Store, .operands = {ValueId{2}, ValueId{5}}},
            {.opcode = Opcode::Interpolate,
             .result = ValueId{6},
             .resultType = module.types.stringType(),
             .operands = {ValueId{5}}},
            {.opcode = Opcode::Return, .operands = {ValueId{6}}}};
        function.blocks = {std::move(block)};
        module.functions = {std::move(function)};
        return module;
    }

    bool eligible(const Module& module)
    {
        const auto plan = wio::codegen::WirCppReceiverPlan::build(module, module.functions[0]);
        return plan.eligible() && plan.receiverValues.contains(0) && plan.receiverValues.contains(1);
    }

    template <class Edit> void rejects(Edit edit, const char* message)
    {
        auto module = fixture();
        edit(module);
        if (eligible(module))
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    try
    {
        const auto module = fixture();
        if (!eligible(module) || module.functions[0].blocks[0].instructions.size() != 8)
            throw std::runtime_error("bounded receiver plan missing or changed IR");
        rejects([](auto& m) { m.functions[0].isMethod = false; }, "free function accepted");
        rejects([](auto& m) { m.functions[0].isAsync = true; }, "async receiver accepted");
        rejects([](auto& m) { m.functions[0].coroutine = CoroutineLayout{}; }, "coroutine accepted");
        rejects([](auto& m) { m.functions[0].isExtension = true; }, "extension accepted");
        rejects([](auto& m) { m.functions[0].nativeBinding = NativeBinding{}; }, "native body accepted");
        rejects([](auto& m) { m.functions[0].genericOrigin = FunctionId{1}; }, "generic body accepted");
        rejects([](auto& m) { m.functions[0].captureParameterCount = 1; }, "capture accepted");
        rejects([](auto& m) { m.functions[0].parameters.push_back(m.functions[0].parameters[0]); },
                "extra argument accepted");
        rejects([](auto& m) { m.types.getMutable(m.functions[0].ownerType).nativeBinding = NativeTypeBinding{}; },
                "native owner accepted");
        rejects([](auto& m) { m.types.getMutable(m.functions[0].ownerType).baseTypes.push_back(m.types.i32Type()); },
                "inherited body accepted");
        rejects([](auto& m) { m.types.getMutable(m.functions[0].ownerType).arguments.push_back(m.types.i32Type()); },
                "generic owner accepted");
        rejects([](auto& m) { m.functions[0].returnType = m.functions[0].parameters[0].type; },
                "reference result accepted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].opcode = Opcode::Load; }, "root load accepted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[0].opcode = Opcode::Store; },
                "root rebind accepted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[4].opcode = Opcode::Call; }, "call accepted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[4].opcode = Opcode::NativeInvoke; },
                "native call accepted");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions[4].borrowOrigin = ValueId{0}; },
                "receiver metadata escaped");
        rejects(
            [](auto& m)
            {
                m.contract.attributes.push_back(
                    {.targetFunction = m.functions[0].id, .processors = {AttributeProcessorDescriptor{}}});
            },
            "behavioral processor bypassed");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                auto alias = code[0];
                alias.result = ValueId{8};
                code.insert(code.end() - 1, alias);
            },
            "receiver alias crossed formatting barrier");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                auto read = code[2];
                read.result = ValueId{7};
                code.insert(code.end() - 1, read);
            },
            "field load after potentially reentrant formatting accepted");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                code.insert(code.begin(), {.opcode = Opcode::Constant,
                                           .result = ValueId{8},
                                           .resultType = m.types.stringType(),
                                           .literal = std::string{"effect"}});
            },
            "receiver check crossed a string allocation");
        rejects(
            [](auto& m)
            {
                auto& code = m.functions[0].blocks[0].instructions;
                const auto type = m.types.i32Type();
                code.insert(
                    code.begin(),
                    {{.opcode = Opcode::Constant, .result = ValueId{8}, .resultType = type, .literal = std::int64_t{0}},
                     {.opcode = Opcode::Constant, .result = ValueId{9}, .resultType = type, .literal = std::int64_t{1}},
                     {.opcode = Opcode::Binary,
                      .result = ValueId{10},
                      .resultType = type,
                      .operands = {ValueId{9}, ValueId{8}},
                      .binaryOperator = typed::BinaryOperator::Divide}});
            },
            "potential arithmetic failure moved after receiver validation");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().operands = {ValueId{0}}; },
                "receiver escaped as a result");
        rejects([](auto& m) { m.functions[0].blocks[0].instructions.back().targets = {{.block = BlockId{1}}}; },
                "branch target accepted");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
