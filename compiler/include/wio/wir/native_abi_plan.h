#pragma once

#include "wio/wir/lowered_ir.h"

#include <string>
#include <vector>

namespace wio::wir
{
    struct NativeThunkPlan
    {
        FunctionId function;
        std::string stableKey;
        std::string specializationKey;
        std::string thunkSymbol;
        std::string nativeSymbol;
        std::string header;
        NativeThunkKind kind = NativeThunkKind::Direct;
        NativeReceiverKind receiver = NativeReceiverKind::None;
        std::vector<TypeId> parameterTypes;
        TypeId resultType;

        auto operator<=>(const NativeThunkPlan&) const = default;
    };

    struct NativeAbiPlanDiagnostic
    {
        std::string code;
        std::string message;
        FunctionId function;
        SourceSpan source;
    };

    class NativeAbiPlanResult final
    {
    public:
        [[nodiscard]] bool succeeded() const { return diagnostics_.empty(); }
        [[nodiscard]] const std::vector<NativeThunkPlan>& thunks() const { return thunks_; }
        [[nodiscard]] const std::vector<NativeAbiPlanDiagnostic>& diagnostics() const { return diagnostics_; }

    private:
        friend class NativeAbiPlanner;
        std::vector<NativeThunkPlan> thunks_;
        std::vector<NativeAbiPlanDiagnostic> diagnostics_;
    };

    // Produces the concrete adapter/thunk inventory consumed by both the new
    // C++ backend and VM native registry. Generic declarations only become
    // entries for concrete NativeInvoke specialization keys.
    class NativeAbiPlanner final
    {
    public:
        [[nodiscard]] NativeAbiPlanResult plan(const lowered::Module& module) const;
    };
}
