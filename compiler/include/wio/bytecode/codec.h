#pragma once

#include "wio/bytecode/module.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace wio::bytecode
{
    struct DecodeLimits
    {
        std::uint64_t maximumFileBytes = 512ull * 1024ull * 1024ull;
        std::uint32_t maximumSections = 64;
        std::uint32_t maximumStrings = 4'000'000;
        std::uint32_t maximumRecords = 4'000'000;
        std::uint32_t maximumListElements = 16'000'000;
    };

    struct DecodeResult
    {
        Module module;
        std::string error;

        [[nodiscard]] bool succeeded() const noexcept { return error.empty(); }
    };

    [[nodiscard]] std::vector<std::byte> encode(const Module& module);
    [[nodiscard]] DecodeResult decode(std::span<const std::byte> bytes, const DecodeLimits& limits = {});
    [[nodiscard]] std::uint64_t payloadChecksum(std::span<const std::byte> bytes) noexcept;
} // namespace wio::bytecode
