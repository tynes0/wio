#include "wio/wir/canonical_optimizer.h"
#include "wio/wir/lowered_ir_printer.h"
#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/lowering_pipeline.h"

#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition) return true;
        std::cerr << message << '\n';
        return false;
    }

    wio::wir::lowered::Module makeOptimizationModule()
    {
        using namespace wio::wir;
        namespace lowered = wio::wir::lowered;

        lowered::Module module;
        module.name = "canonical-optimizer-test";
        module.contract.logicalName = "canonical-optimizer-test";
        module.contract.stableKey = "program:canonical-optimizer-test";
        module.contract.stableId = stableModuleHash(module.contract.stableKey);
        module.contract.callTable.stableId = stableModuleHash(
            module.contract.stableKey + ":sdk-call-table:v" +
            std::to_string(ModuleAbiDescriptorVersion));
        const TypeId arrayType = module.types.intern(Type{
            .kind = TypeKind::Array,
            .arguments = {module.types.i32Type()},
            .staticExtent = 2
        });
        const TypeId dynamicArrayType = module.types.intern(Type{
            .kind = TypeKind::Array,
            .arguments = {module.types.i32Type()}
        });
        const TypeId callableType = module.types.intern(Type{
            .kind = TypeKind::Function,
            .arguments = {module.types.i32Type()}
        });

        lowered::Function function;
        function.id = FunctionId{0};
        function.name = "Optimize";
        function.returnType = module.types.i32Type();
        function.callableType = callableType;

        lowered::BasicBlock entry;
        entry.id = BlockId{0};
        entry.name = "entry";
        entry.instructions = {
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{0},
                .resultType = module.types.boolType(),
                .literal = true
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{1},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{2}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{2},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{3}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Binary,
                .result = ValueId{3},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{1}, ValueId{2}},
                .binaryOperator = typed::BinaryOperator::Add
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Unary,
                .result = ValueId{4},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{3}},
                .unaryOperator = typed::UnaryOperator::Negate
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::Constant,
                .result = ValueId{5},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{1}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::ArrayCreate,
                .result = ValueId{6},
                .resultType = arrayType,
                .operands = {ValueId{1}, ValueId{2}}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::ArrayGet,
                .result = ValueId{7},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{6}, ValueId{5}},
                .boundsCheck = lowered::BoundsCheckMode::Required
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::ArrayCreate,
                .result = ValueId{9},
                .resultType = dynamicArrayType,
                .operands = {ValueId{1}, ValueId{2}}
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::ArrayGet,
                .result = ValueId{10},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{9}, ValueId{5}},
                .boundsCheck = lowered::BoundsCheckMode::Required
            },
            lowered::Instruction{
                .opcode = lowered::Opcode::CondJump,
                .operands = {ValueId{0}},
                .targets = {
                    lowered::BranchTarget{.block = BlockId{1}},
                    lowered::BranchTarget{.block = BlockId{2}}
                }
            }
        };

        lowered::BasicBlock selected;
        selected.id = BlockId{1};
        selected.name = "selected";
        selected.instructions = {lowered::Instruction{
            .opcode = lowered::Opcode::Jump,
            .targets = {lowered::BranchTarget{
                .block = BlockId{3},
                .arguments = {ValueId{3}}
            }}
        }};

        lowered::BasicBlock unreachable;
        unreachable.id = BlockId{2};
        unreachable.name = "unreachable-after-fold";
        unreachable.instructions = {lowered::Instruction{
            .opcode = lowered::Opcode::Jump,
            .targets = {lowered::BranchTarget{
                .block = BlockId{3},
                .arguments = {ValueId{3}}
            }}
        }};

        lowered::BasicBlock merge;
        merge.id = BlockId{3};
        merge.name = "merge";
        merge.parameters = {lowered::Parameter{
            .id = ValueId{8},
            .name = "forwarded",
            .type = module.types.i32Type()
        }};
        merge.instructions = {lowered::Instruction{
            .opcode = lowered::Opcode::Return,
            .operands = {ValueId{8}}
        }};

        function.blocks = {
            std::move(entry),
            std::move(selected),
            std::move(unreachable),
            std::move(merge)
        };
        module.functions.push_back(std::move(function));

        const TypeId i8Type = module.types.intern(Type{.kind = TypeKind::I8});
        const auto appendUnsafeArithmetic = [&](const FunctionId id,
                                                std::string name,
                                                const TypeId type,
                                                typed::Literal left,
                                                typed::Literal right,
                                                const typed::BinaryOperator operation)
        {
            lowered::Function unsafe;
            unsafe.id = id;
            unsafe.name = std::move(name);
            unsafe.returnType = type;
            unsafe.callableType = module.types.intern(Type{
                .kind = TypeKind::Function,
                .arguments = {type}
            });
            lowered::BasicBlock unsafeEntry;
            unsafeEntry.id = BlockId{0};
            unsafeEntry.name = "entry";
            unsafeEntry.instructions = {
                lowered::Instruction{
                    .opcode = lowered::Opcode::Constant,
                    .result = ValueId{0},
                    .resultType = type,
                    .literal = std::move(left)
                },
                lowered::Instruction{
                    .opcode = lowered::Opcode::Constant,
                    .result = ValueId{1},
                    .resultType = type,
                    .literal = std::move(right)
                },
                lowered::Instruction{
                    .opcode = lowered::Opcode::Binary,
                    .result = ValueId{2},
                    .resultType = type,
                    .operands = {ValueId{0}, ValueId{1}},
                    .binaryOperator = operation
                },
                lowered::Instruction{
                    .opcode = lowered::Opcode::Return,
                    .operands = {ValueId{2}}
                }
            };
            unsafe.blocks.push_back(std::move(unsafeEntry));
            module.functions.push_back(std::move(unsafe));
        };
        appendUnsafeArithmetic(
            FunctionId{1}, "PreserveOverflow", i8Type,
            std::int64_t{127}, std::int64_t{1}, typed::BinaryOperator::Add);
        appendUnsafeArithmetic(
            FunctionId{2}, "PreserveDivisionByZero", module.types.i32Type(),
            std::int64_t{5}, std::int64_t{0}, typed::BinaryOperator::Divide);
        return module;
    }

    wio::wir::typed::Module makePipelineModule()
    {
        using namespace wio::wir;
        namespace typed = wio::wir::typed;

        typed::Module module;
        module.name = "canonical-pipeline-test";
        module.contract.logicalName = "canonical-pipeline-test";
        module.contract.stableKey = "program:canonical-pipeline-test";
        module.contract.stableId = stableModuleHash(module.contract.stableKey);
        module.contract.callTable.stableId = stableModuleHash(
            module.contract.stableKey + ":sdk-call-table:v" +
            std::to_string(ModuleAbiDescriptorVersion));
        const TypeId arrayType = module.types.intern(Type{
            .kind = TypeKind::Array,
            .arguments = {module.types.i32Type()},
            .staticExtent = 2
        });

        typed::Function function;
        function.id = FunctionId{0};
        function.name = "Fold";
        function.returnType = module.types.i32Type();
        function.callableType = module.types.intern(Type{
            .kind = TypeKind::Function,
            .arguments = {module.types.i32Type()}
        });
        typed::BasicBlock entry;
        entry.id = BlockId{0};
        entry.name = "entry";
        entry.instructions = {
            typed::Instruction{
                .opcode = typed::Opcode::Constant,
                .result = ValueId{0},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{20}
            },
            typed::Instruction{
                .opcode = typed::Opcode::Constant,
                .result = ValueId{1},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{22}
            },
            typed::Instruction{
                .opcode = typed::Opcode::Binary,
                .result = ValueId{2},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{0}, ValueId{1}},
                .binaryOperator = typed::BinaryOperator::Add
            },
            typed::Instruction{
                .opcode = typed::Opcode::Constant,
                .result = ValueId{3},
                .resultType = module.types.i32Type(),
                .literal = std::int64_t{1}
            },
            typed::Instruction{
                .opcode = typed::Opcode::ArrayCreate,
                .result = ValueId{4},
                .resultType = arrayType,
                .operands = {ValueId{0}, ValueId{1}}
            },
            typed::Instruction{
                .opcode = typed::Opcode::ArrayGet,
                .result = ValueId{5},
                .resultType = module.types.i32Type(),
                .operands = {ValueId{4}, ValueId{3}}
            },
            typed::Instruction{
                .opcode = typed::Opcode::Return,
                .operands = {ValueId{5}}
            }
        };
        function.blocks.push_back(std::move(entry));
        module.functions.push_back(std::move(function));
        return module;
    }
}

int main()
{
    using namespace wio::wir;
    namespace lowered = wio::wir::lowered;

    bool ok = true;
    lowered::Module module = makeOptimizationModule();
    const OptimizationStatistics statistics = CanonicalOptimizer{}.optimize(module);
    const auto verification = lowered::Verifier{}.verify(module);
    if (!verification.succeeded())
        for (const auto& diagnostic : verification.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    ok &= expect(verification.succeeded(), "Canonical optimizer output must verify");
    ok &= expect(statistics.constantsFolded == 2,
        "Binary and dependent unary constants must fold deterministically");
    ok &= expect(statistics.valuesPropagated == 1,
        "Common trivial block arguments must be propagated");
    ok &= expect(statistics.branchesSimplified >= 1 && statistics.blocksRemoved >= 1,
        "Constant branches must simplify and unreachable blocks must be removed");
    ok &= expect(statistics.instructionsRemoved >= 2,
        "Dead folded values and obsolete conditions must be eliminated");
    ok &= expect(statistics.heapAllocations == 2,
        "Array backing storage must receive explicit heap decisions");
    ok &= expect(statistics.boundsChecksEliminated == 2,
        "Fixed and freshly constructed arrays must eliminate proven bounds checks");

    std::size_t preservedUnsafeArithmetic = 0;
    for (const lowered::Function& optimizedFunction : module.functions)
        for (const lowered::BasicBlock& block : optimizedFunction.blocks)
            for (const lowered::Instruction& instruction : block.instructions)
                preservedUnsafeArithmetic += instruction.opcode == lowered::Opcode::Binary;
    ok &= expect(preservedUnsafeArithmetic == 2,
        "Overflowing narrow arithmetic and division by zero must remain runtime operations");

    const lowered::Function& function = module.functions.front();
    ok &= expect(function.blocks.size() == 2 && function.blocks.back().id == BlockId{3} &&
        function.blocks.back().parameters.empty(),
        "Optimized CFG must retain stable block IDs while removing trivial forwarding");
    const std::string printed = lowered::Printer{}.print(module);
    ok &= expect(printed.find("const 5") != std::string::npos &&
        printed.find("storage=heap escape=local") != std::string::npos &&
        printed.find("bounds=eliminated-static") != std::string::npos &&
        printed.find("bounds=eliminated-proven") != std::string::npos,
        "Lowered WIR printer must expose folded values and backend decisions");

    lowered::Module malformedBounds = module;
    for (lowered::BasicBlock& block : malformedBounds.functions.front().blocks)
        for (lowered::Instruction& instruction : block.instructions)
            if (instruction.opcode == lowered::Opcode::ArrayGet)
                instruction.boundsCheck = lowered::BoundsCheckMode::NotApplicable;
    ok &= expect(!lowered::Verifier{}.verify(malformedBounds).succeeded(),
        "Verifier must reject a missing array bounds-check contract");

    lowered::Module forgedProof = module;
    for (lowered::BasicBlock& block : forgedProof.functions.front().blocks)
        for (lowered::Instruction& instruction : block.instructions)
            if (instruction.opcode == lowered::Opcode::Constant && instruction.result == ValueId{5})
                instruction.literal = std::int64_t{9};
    ok &= expect(!lowered::Verifier{}.verify(forgedProof).succeeded(),
        "Verifier must reproduce and reject forged bounds-check elimination proofs");

    lowered::Module malformedStorage = module;
    for (lowered::BasicBlock& block : malformedStorage.functions.front().blocks)
        for (lowered::Instruction& instruction : block.instructions)
            if (instruction.opcode == lowered::Opcode::Constant)
                instruction.storageClass = lowered::StorageClass::Stack;
    ok &= expect(!lowered::Verifier{}.verify(malformedStorage).succeeded(),
        "Verifier must reject storage metadata on a non-allocation instruction");

    const LoweringResult pipeline = LoweringPipeline{}.lower(makePipelineModule());
    ok &= expect(pipeline.succeeded() &&
        pipeline.optimizationStatistics().constantsFolded == 1 &&
        pipeline.optimizationStatistics().instructionsRemoved == 1 &&
        pipeline.optimizationStatistics().heapAllocations == 1 &&
        pipeline.optimizationStatistics().boundsChecksEliminated == 1,
        "LoweringPipeline must run canonical optimization before final verification");
    ok &= expect(pipeline.completedPasses() == std::vector<std::string>{
            "verify-typed-wir",
            "lower-canonical-control-flow",
            "fold-canonical-constants",
            "simplify-control-flow",
            "propagate-trivial-values",
            "eliminate-dead-values",
            "classify-storage-and-escapes",
            "eliminate-proven-bounds-checks",
            "verify-lowered-wir"
        },
        "LoweringPipeline must publish deterministic optimization pass order");

    return ok ? 0 : 1;
}
