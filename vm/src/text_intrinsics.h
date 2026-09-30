#pragma once

#include "wio/vm/value.h"

#include <span>
#include <string>
#include <string_view>

namespace wio::vm::detail
{
    struct TextIntrinsicResult final
    {
        bool recognized = false;
        bool succeeded = false;
        Value value;
        std::string code;
        std::string message;
    };

    [[nodiscard]] TextIntrinsicResult executeTextIntrinsic(std::string_view selector, const Value& receiver,
                                                           std::span<const Value* const> arguments);
} // namespace wio::vm::detail
