#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_hash
{
    [[nodiscard]] std::uint32_t Fnv1a32(std::string_view value) noexcept;
    [[nodiscard]] std::uint64_t Fnv1a64(std::string_view value) noexcept;
    [[nodiscard]] std::uint32_t Fnv1a32Bytes(const std::vector<std::uint8_t>& value) noexcept;
    [[nodiscard]] std::uint64_t Fnv1a64Bytes(const std::vector<std::uint8_t>& value) noexcept;

    [[nodiscard]] std::string Sha256(std::string_view value);
    [[nodiscard]] std::string Sha256Bytes(const std::vector<std::uint8_t>& value);
    [[nodiscard]] std::vector<std::uint8_t> Sha256Digest(std::string_view value);
    [[nodiscard]] std::vector<std::uint8_t> Sha256DigestBytes(const std::vector<std::uint8_t>& value);
}
