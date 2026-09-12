#pragma once
#include "wio/wir/lowered_ir.h"
#include <functional>
#include <string>

namespace wio::codegen {
// Emits SDK/native C boundaries exclusively from the verified WIR contract.
class WirModuleEmitter final {
public:
    using TypeSpelling = std::function<std::string(wir::TypeId)>;
    static std::string emit(const wir::lowered::Module& module, const TypeSpelling& type);
};
}
