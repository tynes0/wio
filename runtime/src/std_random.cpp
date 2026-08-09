#include "std_random.h"

#include <bit>
#include <chrono>
#include <random>

namespace wio::runtime::std_random
{
    namespace
    {
        std::uint64_t splitMix64(std::uint64_t& state) noexcept
        {
            state += 0x9e3779b97f4a7c15ull;
            std::uint64_t value = state;
            value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
            value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
            return value ^ (value >> 31u);
        }

        double unitDouble(const std::uint64_t value) noexcept
        {
            return static_cast<double>(value >> 11u) * 0x1.0p-53;
        }

        void twistMt19937(std::vector<std::uint32_t>& state)
        {
            for (std::size_t i = 0; i < 624u; ++i)
            {
                const std::uint32_t joined =
                    (state[i] & 0x80000000u) |
                    (state[(i + 1u) % 624u] & 0x7fffffffu);
                state[i] = state[(i + 397u) % 624u] ^ (joined >> 1u);
                if ((joined & 1u) != 0u)
                    state[i] ^= 0x9908b0dfu;
            }
        }

        std::uint64_t mixLea64(std::uint64_t value) noexcept
        {
            value = (value ^ (value >> 32u)) * 0xdaba0b6eb09322e3ull;
            value = (value ^ (value >> 32u)) * 0xdaba0b6eb09322e3ull;
            return value ^ (value >> 32u);
        }
    }

    std::uint64_t SystemSeed() noexcept
    {
        std::uint64_t seed = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        try
        {
            std::random_device device;
            seed ^= (static_cast<std::uint64_t>(device()) << 32u) ^ device();
        }
        catch (...)
        {
        }
        return splitMix64(seed);
    }

    std::vector<std::uint8_t> SecureBytes(const std::size_t count)
    {
        std::random_device device;
        std::vector<std::uint8_t> output(count);
        std::uint32_t word = 0;
        unsigned remaining = 0;
        for (auto& byte : output)
        {
            if (remaining == 0) { word = device(); remaining = 4; }
            byte = static_cast<std::uint8_t>(word & 0xffu);
            word >>= 8u;
            --remaining;
        }
        return output;
    }

    void Mt19937Seed(
        const std::uint64_t seed,
        std::vector<std::uint32_t>& state,
        std::size_t& index)
    {
        state.resize(624u);
        state[0] = static_cast<std::uint32_t>(seed) ^ static_cast<std::uint32_t>(seed >> 32u);
        for (std::size_t i = 1u; i < state.size(); ++i)
        {
            state[i] = 1812433253u * (state[i - 1u] ^ (state[i - 1u] >> 30u)) +
                       static_cast<std::uint32_t>(i);
        }
        index = state.size();
    }

    std::uint32_t Mt19937Next(std::vector<std::uint32_t>& state, std::size_t& index)
    {
        if (state.size() != 624u)
            Mt19937Seed(5489u, state, index);
        if (index >= state.size())
        {
            twistMt19937(state);
            index = 0u;
        }

        std::uint32_t value = state[index++];
        value ^= value >> 11u;
        value ^= (value << 7u) & 0x9d2c5680u;
        value ^= (value << 15u) & 0xefc60000u;
        value ^= value >> 18u;
        return value;
    }

    std::uint32_t Mt19937NextBounded(
        std::vector<std::uint32_t>& state,
        std::size_t& index,
        const std::uint32_t maxExclusive)
    {
        if (maxExclusive == 0u)
            return 0u;

        const std::uint32_t threshold = static_cast<std::uint32_t>(-maxExclusive) % maxExclusive;
        std::uint32_t value = 0u;
        do
        {
            value = Mt19937Next(state, index);
        }
        while (value < threshold);
        return value % maxExclusive;
    }

    double Mt19937NextF64(std::vector<std::uint32_t>& state, std::size_t& index)
    {
        const std::uint64_t high = static_cast<std::uint64_t>(Mt19937Next(state, index) >> 5u);
        const std::uint64_t low = static_cast<std::uint64_t>(Mt19937Next(state, index) >> 6u);
        return static_cast<double>(high * 67108864u + low) * (1.0 / 9007199254740992.0);
    }

    void Xoroshiro128PlusSeed(
        const std::uint64_t seed,
        std::uint64_t& state0,
        std::uint64_t& state1) noexcept
    {
        std::uint64_t mixer = seed;
        state0 = splitMix64(mixer);
        state1 = splitMix64(mixer);
        if ((state0 | state1) == 0u)
            state1 = 1u;
    }

    std::uint64_t Xoroshiro128PlusNext(
        std::uint64_t& state0,
        std::uint64_t& state1) noexcept
    {
        const std::uint64_t left = state0;
        std::uint64_t right = state1;
        const std::uint64_t result = left + right;
        right ^= left;
        state0 = std::rotl(left, 24) ^ right ^ (right << 16u);
        state1 = std::rotl(right, 37);
        return result;
    }

    double Xoroshiro128PlusNextF64(
        std::uint64_t& state0,
        std::uint64_t& state1) noexcept
    {
        return unitDouble(Xoroshiro128PlusNext(state0, state1));
    }

    void LxmSeed(
        const std::uint64_t seed,
        std::uint64_t& lcg,
        std::uint64_t& state0,
        std::uint64_t& state1) noexcept
    {
        std::uint64_t mixer = seed;
        lcg = splitMix64(mixer);
        state0 = splitMix64(mixer);
        state1 = splitMix64(mixer);
        if ((state0 | state1) == 0u)
            state1 = 1u;
    }

    std::uint64_t LxmNext(
        std::uint64_t& lcg,
        std::uint64_t& state0,
        std::uint64_t& state1) noexcept
    {
        const std::uint64_t result = mixLea64(lcg + state0);
        lcg = lcg * 0xd1342543de82ef95ull + 0x9e3779b97f4a7c15ull;

        const std::uint64_t left = state0;
        std::uint64_t right = state1;
        right ^= left;
        state0 = std::rotl(left, 24) ^ right ^ (right << 16u);
        state1 = std::rotl(right, 37);
        return result;
    }

    double LxmNextF64(
        std::uint64_t& lcg,
        std::uint64_t& state0,
        std::uint64_t& state1) noexcept
    {
        return unitDouble(LxmNext(lcg, state0, state1));
    }

    void WichmannHillSeed(
        const std::uint64_t seed,
        std::uint32_t& x,
        std::uint32_t& y,
        std::uint32_t& z) noexcept
    {
        std::uint64_t mixer = seed;
        x = static_cast<std::uint32_t>(splitMix64(mixer) % 30268u) + 1u;
        y = static_cast<std::uint32_t>(splitMix64(mixer) % 30306u) + 1u;
        z = static_cast<std::uint32_t>(splitMix64(mixer) % 30322u) + 1u;
    }

    double WichmannHillNextF64(
        std::uint32_t& x,
        std::uint32_t& y,
        std::uint32_t& z) noexcept
    {
        x = (171u * x) % 30269u;
        y = (172u * y) % 30307u;
        z = (170u * z) % 30323u;
        const double sum =
            static_cast<double>(x) / 30269.0 +
            static_cast<double>(y) / 30307.0 +
            static_cast<double>(z) / 30323.0;
        return sum - static_cast<std::uint64_t>(sum);
    }

    std::uint32_t WichmannHillNext(
        std::uint32_t& x,
        std::uint32_t& y,
        std::uint32_t& z) noexcept
    {
        return static_cast<std::uint32_t>(WichmannHillNextF64(x, y, z) * 4294967296.0);
    }
}
