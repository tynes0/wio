#pragma once
#include "wio/wir/lowered_ir.h"
#include <functional>
#include <span>
#include <string>

namespace wio::codegen
{
    // Emits SDK/native C boundaries exclusively from the verified WIR contract.
    class WirModuleEmitter final
    {
    public:
        using TypeSpelling = std::function<std::string(wir::TypeId)>;
        static std::string nativeValueCodec(const wir::lowered::Module&, wir::TypeId, const TypeSpelling&);
        static std::string emitCallableCodecs(const wir::lowered::Module&, const TypeSpelling&,
                                              std::span<const wir::TypeId> dynamicPayloadTypes = {});
        static std::string emit(const wir::lowered::Module& module, const TypeSpelling& type,
                                bool nativeTaskSidecars = false, std::span<const wir::TypeId> dynamicPayloadTypes = {});
    };
} // namespace wio::codegen
