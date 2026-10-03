#pragma once

#include "wio/bytecode/module.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace wio::vm
{
    struct StackFrame
    {
        std::uint32_t function = bytecode::InvalidIndex;
        std::uint32_t block = bytecode::InvalidIndex;
        std::uint32_t instruction = bytecode::InvalidIndex;
        bytecode::SourceSpan source;
    };

    struct ExecutionError
    {
        ExecutionError() = default;

        ExecutionError(std::string errorCode, std::string errorMessage,
                       const std::uint32_t functionId = bytecode::InvalidIndex,
                       const std::uint32_t blockId = bytecode::InvalidIndex,
                       const std::uint32_t instructionIndex = bytecode::InvalidIndex,
                       bytecode::SourceSpan sourceSpan = {})
            : code(std::move(errorCode)), message(std::move(errorMessage)), function(functionId), block(blockId),
              instruction(instructionIndex), source(sourceSpan)
        {
        }

        std::string code;
        std::string message;
        std::uint32_t function = bytecode::InvalidIndex;
        std::uint32_t block = bytecode::InvalidIndex;
        std::uint32_t instruction = bytecode::InvalidIndex;
        bytecode::SourceSpan source;
        std::vector<StackFrame> stack;
    };
} // namespace wio::vm
