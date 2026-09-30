#pragma once

#include "wio/vm/value.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace wio::vm::detail
{
    struct StringIntrinsicResult final
    {
        bool recognized = false;
        bool succeeded = false;
        Value value;
        std::string code;
        std::string message;
    };

    [[nodiscard]] StringIntrinsicResult executeStringIntrinsic(std::string_view selector, const Value& receiver,
                                                               Value* mutableReceiver,
                                                               std::span<const Value* const> arguments,
                                                               std::uint8_t resultType);
} // namespace wio::vm::detail
