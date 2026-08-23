#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace wio::runtime::std_random
{
    [[nodiscard]] std::uint64_t SystemSeed() noexcept;
    [[nodiscard]] std::vector<std::uint8_t> SecureBytes(std::size_t count);

    void Mt19937Seed(std::uint64_t seed, std::vector<std::uint32_t>& state, std::size_t& index);
    [[nodiscard]] std::uint32_t Mt19937Next(std::vector<std::uint32_t>& state, std::size_t& index);
    [[nodiscard]] std::uint32_t Mt19937NextBounded(
        std::vector<std::uint32_t>& state,
        std::size_t& index,
        std::uint32_t maxExclusive);
    [[nodiscard]] std::int32_t Mt19937NextI32(
        std::vector<std::uint32_t>& state,
        std::size_t& index,
        std::int32_t minInclusive,
        std::int32_t maxExclusive);
    [[nodiscard]] double Mt19937NextF64(std::vector<std::uint32_t>& state, std::size_t& index);

    void Xoroshiro128PlusSeed(std::uint64_t seed, std::uint64_t& state0, std::uint64_t& state1) noexcept;
    [[nodiscard]] std::uint64_t Xoroshiro128PlusNext(std::uint64_t& state0, std::uint64_t& state1) noexcept;
    [[nodiscard]] double Xoroshiro128PlusNextF64(std::uint64_t& state0, std::uint64_t& state1) noexcept;

    void LxmSeed(std::uint64_t seed, std::uint64_t& lcg, std::uint64_t& state0, std::uint64_t& state1) noexcept;
    [[nodiscard]] std::uint64_t LxmNext(std::uint64_t& lcg, std::uint64_t& state0, std::uint64_t& state1) noexcept;
    [[nodiscard]] double LxmNextF64(std::uint64_t& lcg, std::uint64_t& state0, std::uint64_t& state1) noexcept;

    void WichmannHillSeed(std::uint64_t seed, std::uint32_t& x, std::uint32_t& y, std::uint32_t& z) noexcept;
    [[nodiscard]] double WichmannHillNextF64(std::uint32_t& x, std::uint32_t& y, std::uint32_t& z) noexcept;
    [[nodiscard]] std::uint32_t WichmannHillNext(std::uint32_t& x, std::uint32_t& y, std::uint32_t& z) noexcept;
}
