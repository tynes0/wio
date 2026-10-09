#pragma once

#include "wio/codegen/wir_module_emitter.h"

#include <set>

namespace wio::codegen
{
    class WirNativeObjectEmitter final
    {
    public:
        using MethodSpelling = std::function<std::string(const wir::MethodLayout&, const wir::lowered::Function&)>;

        // Method-only surface: concrete Wio objects and qualified hierarchies,
        // with only private scalar/string/text fields, and
        // scalar/string/text/qualified-object and nullable-object values
        // (or void results), plus
        // ref/view object (including nullable) parameters with alias-preserving
        // handle copy-back. Borrowed results require caller-rooted provenance.
        [[nodiscard]] static std::set<wir::TypeId> eligibleTypes(const wir::lowered::Module& module);
        // Ancestors also need virtual native wrappers and hosted lifecycle
        // suppression, but do not necessarily need materialization adapters.
        [[nodiscard]] static std::set<wir::TypeId> hostedHierarchyTypes(const wir::lowered::Module& module,
                                                                        const std::set<wir::TypeId>& objects);
        // Emit before any function instantiates native codecs. Views preserve
        // concrete wire identity and use checked generated C++ upcasts.
        [[nodiscard]] static std::string emitCodecs(const wir::lowered::Module& module,
                                                   const std::set<wir::TypeId>& objects,
                                                   const WirModuleEmitter::TypeSpelling& cpp);
        [[nodiscard]] static std::string emit(const wir::lowered::Module& module, const std::set<wir::TypeId>& objects,
                                              const WirModuleEmitter::TypeSpelling& cpp,
                                              const MethodSpelling& legacyMethod);
    };
} // namespace wio::codegen
