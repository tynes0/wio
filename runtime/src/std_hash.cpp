#include "std_hash.h"

#include <array>
#include <bit>
#include <cstddef>
#include <iomanip>
#include <sstream>

namespace wio::runtime::std_hash
{
    namespace
    {
        constexpr std::array<std::uint32_t, 64> RoundConstants = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
        };

        std::vector<std::uint8_t> sha256Digest(const std::uint8_t* data, const std::size_t size)
        {
            std::vector<std::uint8_t> message;
            if (size > 0u)
                message.assign(data, data + size);
            message.push_back(0x80u);
            while ((message.size() % 64u) != 56u)
                message.push_back(0u);

            const std::uint64_t bitLength = static_cast<std::uint64_t>(size) * 8u;
            for (int shift = 56; shift >= 0; shift -= 8)
                message.push_back(static_cast<std::uint8_t>(bitLength >> shift));

            std::array<std::uint32_t, 8> state = {
                0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
            };

            for (std::size_t offset = 0; offset < message.size(); offset += 64u)
            {
                std::array<std::uint32_t, 64> words{};
                for (std::size_t i = 0; i < 16u; ++i)
                {
                    const std::size_t p = offset + i * 4u;
                    words[i] =
                        (static_cast<std::uint32_t>(message[p]) << 24u) |
                        (static_cast<std::uint32_t>(message[p + 1u]) << 16u) |
                        (static_cast<std::uint32_t>(message[p + 2u]) << 8u) |
                        static_cast<std::uint32_t>(message[p + 3u]);
                }

                for (std::size_t i = 16u; i < 64u; ++i)
                {
                    const std::uint32_t s0 =
                        std::rotr(words[i - 15u], 7) ^
                        std::rotr(words[i - 15u], 18) ^
                        (words[i - 15u] >> 3u);
                    const std::uint32_t s1 =
                        std::rotr(words[i - 2u], 17) ^
                        std::rotr(words[i - 2u], 19) ^
                        (words[i - 2u] >> 10u);
                    words[i] = words[i - 16u] + s0 + words[i - 7u] + s1;
                }

                std::uint32_t a = state[0];
                std::uint32_t b = state[1];
                std::uint32_t c = state[2];
                std::uint32_t d = state[3];
                std::uint32_t e = state[4];
                std::uint32_t f = state[5];
                std::uint32_t g = state[6];
                std::uint32_t h = state[7];

                for (std::size_t i = 0; i < 64u; ++i)
                {
                    const std::uint32_t sum1 =
                        std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
                    const std::uint32_t choose = (e & f) ^ (~e & g);
                    const std::uint32_t temp1 = h + sum1 + choose + RoundConstants[i] + words[i];
                    const std::uint32_t sum0 =
                        std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
                    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temp2 = sum0 + majority;

                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }

                state[0] += a;
                state[1] += b;
                state[2] += c;
                state[3] += d;
                state[4] += e;
                state[5] += f;
                state[6] += g;
                state[7] += h;
            }

            std::vector<std::uint8_t> digest;
            digest.reserve(32u);
            for (const std::uint32_t word : state)
            {
                digest.push_back(static_cast<std::uint8_t>(word >> 24u));
                digest.push_back(static_cast<std::uint8_t>(word >> 16u));
                digest.push_back(static_cast<std::uint8_t>(word >> 8u));
                digest.push_back(static_cast<std::uint8_t>(word));
            }
            return digest;
        }

        std::string toHex(const std::vector<std::uint8_t>& digest)
        {
            std::ostringstream stream;
            stream << std::hex << std::setfill('0');
            for (const auto value : digest)
                stream << std::setw(2) << static_cast<unsigned int>(value);
            return stream.str();
        }

        template <typename TValue>
        TValue fnv1a(const std::uint8_t* data, const std::size_t size, TValue hash, const TValue prime) noexcept
        {
            for (std::size_t i = 0; i < size; ++i)
            {
                hash ^= static_cast<TValue>(data[i]);
                hash *= prime;
            }
            return hash;
        }
    }

    std::uint32_t Fnv1a32(const std::string_view value) noexcept
    {
        return fnv1a(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size(),
            std::uint32_t{2166136261u},
            std::uint32_t{16777619u});
    }

    std::uint64_t Fnv1a64(const std::string_view value) noexcept
    {
        return fnv1a(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size(),
            std::uint64_t{14695981039346656037ull},
            std::uint64_t{1099511628211ull});
    }

    std::uint32_t Fnv1a32Bytes(const std::vector<std::uint8_t>& value) noexcept
    {
        return fnv1a(value.data(), value.size(), std::uint32_t{2166136261u}, std::uint32_t{16777619u});
    }

    std::uint64_t Fnv1a64Bytes(const std::vector<std::uint8_t>& value) noexcept
    {
        return fnv1a(value.data(), value.size(), std::uint64_t{14695981039346656037ull}, std::uint64_t{1099511628211ull});
    }

    std::vector<std::uint8_t> Sha256Digest(const std::string_view value)
    {
        return sha256Digest(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    }

    std::vector<std::uint8_t> Sha256DigestBytes(const std::vector<std::uint8_t>& value)
    {
        return sha256Digest(value.data(), value.size());
    }

    std::string Sha256(const std::string_view value)
    {
        return toHex(Sha256Digest(value));
    }

    std::string Sha256Bytes(const std::vector<std::uint8_t>& value)
    {
        return toHex(Sha256DigestBytes(value));
    }
}
