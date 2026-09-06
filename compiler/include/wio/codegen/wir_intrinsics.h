#pragma once

#include "wio/wir/type.h"
#include <optional>
#include <string>
#include <string_view>

namespace wio::codegen
{
    // Maps the pinned WIR family/selector to the shared runtime contract.
    // No semantic member lookup or AST recovery is performed here.
    [[nodiscard]] std::optional<std::string> wirIntrinsicHelper(
        wir::IntrinsicFamily family, std::string_view selector);
}
