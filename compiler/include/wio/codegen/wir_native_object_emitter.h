#pragma once

#include "wio/codegen/wir_module_emitter.h"

#include <set>

namespace wio::codegen
{
    class WirNativeObjectEmitter final
    {
    public:
        using MethodSpelling = std::function<std::string(const wir::MethodLayout&, const wir::lowered::Function&)>;

        // Initial method-only surface: concrete, non-generic Wio objects with
        // no custom bases, only private scalar/string/text fields, and
        // scalar/string/text/qualified-object and nullable-object values
        // (or void results), plus
        // ref/view object parameters with alias-preserving handle copy-back.
        [[nodiscard]] static std::set<wir::TypeId> eligibleTypes(const wir::lowered::Module& module);
        [[nodiscard]] static std::string emit(const wir::lowered::Module& module, const std::set<wir::TypeId>& objects,
                                              const WirModuleEmitter::TypeSpelling& cpp,
                                              const MethodSpelling& legacyMethod);
    };
} // namespace wio::codegen
