#include "wio/wir/lowering_pipeline.h"

#include "wio/wir/lowered_ir_verifier.h"
#include "wio/wir/typed_ir_verifier.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace wio::wir
{
    namespace
    {
        lowered::Parameter lowerParameter(const typed::Parameter& parameter)
        {
            return lowered::Parameter{
                .id = parameter.id,
                .name = parameter.name,
                .type = parameter.type,
                .source = parameter.source
            };
        }

        lowered::Instruction lowerSimpleInstruction(const typed::Instruction& instruction)
        {
            lowered::Opcode opcode = lowered::Opcode::Unreachable;
            switch (instruction.opcode)
            {
            case typed::Opcode::Constant: opcode = lowered::Opcode::Constant; break;
            case typed::Opcode::Unary: opcode = lowered::Opcode::Unary; break;
            case typed::Opcode::Binary: opcode = lowered::Opcode::Binary; break;
            case typed::Opcode::Convert: opcode = lowered::Opcode::Convert; break;
            case typed::Opcode::Call: opcode = lowered::Opcode::Call; break;
            case typed::Opcode::FunctionReference: opcode = lowered::Opcode::FunctionReference; break;
            case typed::Opcode::ClosureCreate: opcode = lowered::Opcode::ClosureCreate; break;
            case typed::Opcode::IndirectCall: opcode = lowered::Opcode::IndirectCall; break;
            case typed::Opcode::ExtensionCall: opcode = lowered::Opcode::ExtensionCall; break;
            case typed::Opcode::MethodCall: opcode = lowered::Opcode::MethodCall; break;
            case typed::Opcode::VirtualCall: opcode = lowered::Opcode::VirtualCall; break;
            case typed::Opcode::InterfaceCall: opcode = lowered::Opcode::InterfaceCall; break;
            case typed::Opcode::Upcast: opcode = lowered::Opcode::Upcast; break;
            case typed::Opcode::CheckedCast: opcode = lowered::Opcode::CheckedCast; break;
            case typed::Opcode::TypeTest: opcode = lowered::Opcode::TypeTest; break;
            case typed::Opcode::IdentityEqual: opcode = lowered::Opcode::IdentityEqual; break;
            case typed::Opcode::VariantTest: opcode = lowered::Opcode::VariantTest; break;
            case typed::Opcode::VariantPayload: opcode = lowered::Opcode::VariantPayload; break;
            case typed::Opcode::ArrayLength: opcode = lowered::Opcode::ArrayLength; break;
            case typed::Opcode::ArrayElement: opcode = lowered::Opcode::ArrayElement; break;
            case typed::Opcode::ArrayCreate: opcode = lowered::Opcode::ArrayCreate; break;
            case typed::Opcode::ArrayGet: opcode = lowered::Opcode::ArrayGet; break;
            case typed::Opcode::LocalPlace: opcode = lowered::Opcode::LocalPlace; break;
            case typed::Opcode::PlaceInit: opcode = lowered::Opcode::PlaceInit; break;
            case typed::Opcode::Load: opcode = lowered::Opcode::Load; break;
            case typed::Opcode::Store: opcode = lowered::Opcode::Store; break;
            case typed::Opcode::FieldPlace: opcode = lowered::Opcode::FieldPlace; break;
            case typed::Opcode::ArrayPlace: opcode = lowered::Opcode::ArrayPlace; break;
            case typed::Opcode::Borrow: opcode = lowered::Opcode::Borrow; break;
            case typed::Opcode::ConstructComponent: opcode = lowered::Opcode::ConstructComponent; break;
            case typed::Opcode::ConstructObject: opcode = lowered::Opcode::ConstructObject; break;
            case typed::Opcode::Drop: opcode = lowered::Opcode::Drop; break;
            case typed::Opcode::Return: opcode = lowered::Opcode::Return; break;
            case typed::Opcode::Unreachable: opcode = lowered::Opcode::Unreachable; break;
            case typed::Opcode::Select:
            case typed::Opcode::Branch:
            case typed::Opcode::CondBranch:
                break;
            }
            return lowered::Instruction{
                .opcode = opcode,
                .result = instruction.result,
                .resultType = instruction.resultType,
                .operands = instruction.operands,
                .callee = instruction.callee,
                .literal = instruction.literal,
                .unaryOperator = instruction.unaryOperator,
                .binaryOperator = instruction.binaryOperator,
                .conversionKind = instruction.conversionKind,
                .selector = instruction.selector,
                .projectionIndex = instruction.projectionIndex,
                .signatureTypes = instruction.signatureTypes,
                .genericArguments = instruction.genericArguments,
                .captureKinds = instruction.captureKinds,
                .specializationKey = instruction.specializationKey,
                .targetType = instruction.targetType,
                .source = instruction.source
            };
        }
    }

    class CanonicalControlFlowLowerer final
    {
    public:
        CanonicalControlFlowLowerer(const typed::Module& source, LoweringResult& result)
            : source_(source), result_(result)
        {
        }

        void run()
        {
            result_.module_.name = source_.name;
            result_.module_.types = source_.types;
            result_.module_.functions.reserve(source_.functions.size());
            for (const typed::Function& sourceFunction : source_.functions)
                lowerFunction(sourceFunction);
        }

    private:
        const typed::Module& source_;
        LoweringResult& result_;

        void report(std::string code, std::string message, const SourceSpan& source)
        {
            result_.diagnostics_.push_back(LoweringDiagnostic{
                .code = std::move(code),
                .pass = "lower-canonical-control-flow",
                .message = std::move(message),
                .source = source
            });
        }

        void lowerFunction(const typed::Function& sourceFunction)
        {
            lowered::Function function{
                .id = sourceFunction.id,
                .name = sourceFunction.name,
                .returnType = sourceFunction.returnType,
                .callableType = sourceFunction.callableType,
                .ownerType = sourceFunction.ownerType,
                .methodSlot = sourceFunction.methodSlot,
                .captureParameterCount = sourceFunction.captureParameterCount,
                .captures = sourceFunction.captures,
                .genericParameters = sourceFunction.genericParameters,
                .source = sourceFunction.source,
                .isAsync = sourceFunction.isAsync,
                .isExternal = sourceFunction.isExternal,
                .isMethod = sourceFunction.isMethod,
                .isAbstract = sourceFunction.isAbstract,
                .isExtension = sourceFunction.isExtension,
                .isClosureBody = sourceFunction.isClosureBody
            };
            for (const typed::Parameter& parameter : sourceFunction.parameters)
                function.parameters.push_back(lowerParameter(parameter));
            if (sourceFunction.isExternal)
            {
                result_.module_.functions.push_back(std::move(function));
                return;
            }

            std::size_t selectCount = 0;
            BlockId::ValueType nextBlockId = 0;
            for (const typed::BasicBlock& block : sourceFunction.blocks)
            {
                if (block.id)
                    nextBlockId = std::max(nextBlockId, static_cast<BlockId::ValueType>(block.id.value() + 1));
                selectCount += static_cast<std::size_t>(std::ranges::count_if(
                    block.instructions,
                    [](const typed::Instruction& instruction)
                    {
                        return instruction.opcode == typed::Opcode::Select;
                    }));
            }
            function.blocks.reserve(sourceFunction.blocks.size() + selectCount * 3);

            std::unordered_map<BlockId::ValueType, std::size_t> blockIndices;
            for (const typed::BasicBlock& sourceBlock : sourceFunction.blocks)
            {
                const std::size_t index = function.blocks.size();
                blockIndices[sourceBlock.id.value()] = index;
                lowered::BasicBlock block{
                    .id = sourceBlock.id,
                    .name = sourceBlock.name,
                    .source = sourceBlock.source
                };
                for (const typed::Parameter& parameter : sourceBlock.parameters)
                    block.parameters.push_back(lowerParameter(parameter));
                function.blocks.push_back(std::move(block));
            }

            auto createBlock = [&](std::string name, const SourceSpan& source) -> std::size_t
            {
                const BlockId id{nextBlockId++};
                const std::size_t index = function.blocks.size();
                blockIndices[id.value()] = index;
                function.blocks.push_back(lowered::BasicBlock{
                    .id = id,
                    .name = std::move(name),
                    .source = source
                });
                return index;
            };

            std::size_t selectOrdinal = 0;
            for (const typed::BasicBlock& sourceBlock : sourceFunction.blocks)
            {
                std::size_t currentIndex = blockIndices.at(sourceBlock.id.value());
                for (const typed::Instruction& instruction : sourceBlock.instructions)
                {
                    if (instruction.opcode == typed::Opcode::Select)
                    {
                        if (instruction.operands.size() != 3 || !instruction.result || !instruction.resultType)
                        {
                            report("WIR3001", "Typed select instruction has an invalid shape.", instruction.source);
                            continue;
                        }

                        const std::string suffix = std::to_string(selectOrdinal++);
                        const std::size_t trueIndex = createBlock("select.true." + suffix, instruction.source);
                        const std::size_t falseIndex = createBlock("select.false." + suffix, instruction.source);
                        const std::size_t mergeIndex = createBlock("select.merge." + suffix, instruction.source);
                        const BlockId trueBlock = function.blocks[trueIndex].id;
                        const BlockId falseBlock = function.blocks[falseIndex].id;
                        const BlockId mergeBlock = function.blocks[mergeIndex].id;

                        function.blocks[currentIndex].instructions.push_back(lowered::Instruction{
                            .opcode = lowered::Opcode::CondJump,
                            .operands = {instruction.operands[0]},
                            .targets = {
                                lowered::BranchTarget{.block = trueBlock},
                                lowered::BranchTarget{.block = falseBlock}
                            },
                            .source = instruction.source
                        });
                        function.blocks[trueIndex].instructions.push_back(lowered::Instruction{
                            .opcode = lowered::Opcode::Jump,
                            .targets = {lowered::BranchTarget{
                                .block = mergeBlock,
                                .arguments = {instruction.operands[1]}
                            }},
                            .source = instruction.source
                        });
                        function.blocks[falseIndex].instructions.push_back(lowered::Instruction{
                            .opcode = lowered::Opcode::Jump,
                            .targets = {lowered::BranchTarget{
                                .block = mergeBlock,
                                .arguments = {instruction.operands[2]}
                            }},
                            .source = instruction.source
                        });
                        function.blocks[mergeIndex].parameters.push_back(lowered::Parameter{
                            .id = instruction.result,
                            .name = "select.result",
                            .type = instruction.resultType,
                            .source = instruction.source
                        });
                        currentIndex = mergeIndex;
                        continue;
                    }

                    if (instruction.opcode == typed::Opcode::Branch)
                    {
                        if (instruction.targets.size() != 1)
                        {
                            report("WIR3002", "Typed branch requires exactly one target before lowering.", instruction.source);
                            continue;
                        }
                        function.blocks[currentIndex].instructions.push_back(lowered::Instruction{
                            .opcode = lowered::Opcode::Jump,
                            .targets = {lowered::BranchTarget{
                                .block = instruction.targets.front(),
                                .arguments = instruction.operands
                            }},
                            .source = instruction.source
                        });
                        continue;
                    }

                    if (instruction.opcode == typed::Opcode::CondBranch)
                    {
                        if (instruction.targets.size() != 2 || instruction.operands.size() != 1)
                        {
                            report("WIR3003", "Typed conditional branch has an invalid shape before lowering.", instruction.source);
                            continue;
                        }
                        function.blocks[currentIndex].instructions.push_back(lowered::Instruction{
                            .opcode = lowered::Opcode::CondJump,
                            .operands = instruction.operands,
                            .targets = {
                                lowered::BranchTarget{.block = instruction.targets[0]},
                                lowered::BranchTarget{.block = instruction.targets[1]}
                            },
                            .source = instruction.source
                        });
                        continue;
                    }

                    function.blocks[currentIndex].instructions.push_back(lowerSimpleInstruction(instruction));
                }
            }
            result_.module_.functions.push_back(std::move(function));
        }
    };

    LoweringResult LoweringPipeline::lower(const typed::Module& module) const
    {
        LoweringResult result;

        const typed::VerificationResult typedVerification = typed::Verifier{}.verify(module);
        if (!typedVerification.succeeded())
        {
            for (const auto& diagnostic : typedVerification.diagnostics())
            {
                result.diagnostics_.push_back(LoweringDiagnostic{
                    .code = diagnostic.code,
                    .pass = "verify-typed-wir",
                    .message = diagnostic.message,
                    .source = diagnostic.source
                });
            }
            return result;
        }
        result.completedPasses_.push_back("verify-typed-wir");

        CanonicalControlFlowLowerer{module, result}.run();
        if (!result.diagnostics_.empty())
            return result;
        result.completedPasses_.push_back("lower-canonical-control-flow");

        const lowered::VerificationResult loweredVerification = lowered::Verifier{}.verify(result.module_);
        if (!loweredVerification.succeeded())
        {
            for (const auto& diagnostic : loweredVerification.diagnostics())
            {
                result.diagnostics_.push_back(LoweringDiagnostic{
                    .code = diagnostic.code,
                    .pass = "verify-lowered-wir",
                    .message = diagnostic.message,
                    .source = diagnostic.source
                });
            }
            return result;
        }
        result.completedPasses_.push_back("verify-lowered-wir");
        return result;
    }
}
