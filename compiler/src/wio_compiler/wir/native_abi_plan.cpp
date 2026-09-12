#include "wio/wir/native_abi_plan.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace wio::wir
{
    namespace
    {
        std::uint64_t stableHash(const std::string_view text)
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const unsigned char byte : text)
            {
                hash ^= byte;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::string thunkSymbol(const std::string_view stableKey)
        {
            std::ostringstream stream;
            stream << "_wio_native_" << std::hex << std::setfill('0') << std::setw(16)
                   << stableHash(stableKey);
            return stream.str();
        }
    }

    NativeAbiPlanResult NativeAbiPlanner::plan(const lowered::Module& module) const
    {
        NativeAbiPlanResult result;
        std::unordered_map<FunctionId::ValueType, const lowered::Function*> functions;
        for (const lowered::Function& function : module.functions)
            functions.emplace(function.id.value(), &function);

        std::unordered_set<std::string> plannedKeys;
        auto report = [&](std::string code, std::string message, const lowered::Function* function)
        {
            result.diagnostics_.push_back(NativeAbiPlanDiagnostic{
                .code = std::move(code),
                .message = std::move(message),
                .function = function ? function->id : FunctionId{},
                .source = function ? function->source : SourceSpan{}
            });
        };
        auto append = [&](const lowered::Function& function,
                          std::string specialization,
                          std::vector<TypeId> parameterTypes,
                          const TypeId resultType)
        {
            if (!function.nativeBinding)
            {
                report("WIRN1001", "Native thunk plan references a function without native binding metadata.", &function);
                return;
            }
            const NativeBinding& binding = *function.nativeBinding;
            std::string concreteKey = binding.stableKey;
            if (!specialization.empty())
                concreteKey += ":" + specialization;
            if (!plannedKeys.insert(concreteKey).second)
                return;
            result.thunks_.push_back(NativeThunkPlan{
                .function = function.id,
                .stableKey = std::move(concreteKey),
                .specializationKey = std::move(specialization),
                .thunkSymbol = function.genericParameters.empty()
                    ? binding.thunkSymbol
                    : std::string{},
                .nativeSymbol = binding.symbol,
                .header = binding.header,
                .kind = binding.thunkKind,
                .receiver = binding.receiver,
                .parameterTypes = std::move(parameterTypes),
                .resultType = resultType
            });
            // The concrete generic symbol must hash the complete key. The
            // initializer above cannot refer to the element being built.
            if (!function.genericParameters.empty())
                result.thunks_.back().thunkSymbol = thunkSymbol(result.thunks_.back().stableKey);
        };

        for (const lowered::Function& function : module.functions)
        {
            if (!function.nativeBinding || !function.genericParameters.empty())
                continue;
            std::vector<TypeId> parameters;
            parameters.reserve(function.nativeBinding->parameters.size());
            for (const NativeAbiValue& parameter : function.nativeBinding->parameters)
                parameters.push_back(parameter.type);
            append(function, {}, std::move(parameters), function.returnType);
        }

        for (const lowered::Function& caller : module.functions)
        {
            for (const lowered::BasicBlock& block : caller.blocks)
            {
                for (const lowered::Instruction& instruction : block.instructions)
                {
                    if (instruction.opcode != lowered::Opcode::NativeInvoke)
                        continue;
                    const auto callee = instruction.callee
                        ? functions.find(instruction.callee.value())
                        : functions.end();
                    if (callee == functions.end() || !callee->second->nativeBinding)
                    {
                        report("WIRN1002", "NativeInvoke does not resolve to a native declaration.", &caller);
                        continue;
                    }
                    if (!callee->second->genericParameters.empty())
                    {
                        if (instruction.specializationKey.empty())
                        {
                            report("WIRN1003", "Generic native invocation requires a concrete specialization key.", callee->second);
                            continue;
                        }
                        append(*callee->second, instruction.specializationKey,
                            instruction.signatureTypes, instruction.result ? instruction.resultType : callee->second->returnType);
                    }
                }
            }
        }

        std::ranges::sort(result.thunks_, {}, &NativeThunkPlan::stableKey);
        std::unordered_set<std::string> symbols;
        for (const NativeThunkPlan& thunk : result.thunks_)
        {
            if (thunk.stableKey.empty() || thunk.thunkSymbol.empty() || thunk.nativeSymbol.empty() ||
                !module.types.tryGet(thunk.resultType) ||
                !std::ranges::all_of(thunk.parameterTypes, [&](const TypeId type)
                    { return module.types.tryGet(type) != nullptr; }))
            {
                const lowered::Function* function = functions.contains(thunk.function.value())
                    ? functions.at(thunk.function.value()) : nullptr;
                report("WIRN1004", "Concrete native thunk has an invalid identity or ABI signature.", function);
            }
            if (!symbols.insert(thunk.thunkSymbol).second)
            {
                const lowered::Function* function = functions.contains(thunk.function.value())
                    ? functions.at(thunk.function.value()) : nullptr;
                report("WIRN1005", "Concrete native thunk symbol is not unique.", function);
            }
        }
        return result;
    }
}
