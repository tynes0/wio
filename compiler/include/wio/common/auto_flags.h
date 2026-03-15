#pragma once

#include <cstdint>
namespace wio
{
    namespace common
    {
        /* ============================================================
        PACKED STORAGE
        ============================================================ */
        template <size_t N>
        struct packed_flags {
            static constexpr size_t byte_count = (N + 7) / 8;
            uint8_t data[byte_count]{};

            constexpr bool get(size_t bit) const {
                return (data[bit >> 3] >> (bit & 7)) & 1u;
            }

            constexpr void set(size_t bit, bool v) {
                uint8_t& b = data[bit >> 3];
                uint8_t mask = static_cast<uint8_t>(1u << (bit & 7));
                if (v) b |= mask;
                else   b &= ~mask;
            }

            constexpr void fill0()
            {
                std::memset(data, 0, byte_count);
            }

            constexpr void fill1()
            {
                std::memset(data, 1, byte_count);
            }
        };
    }
}

#define FLAG_ENUM(name) name,
#define FLAG_METHOD(name)                                       \
    constexpr bool get_##name() const {                         \
        return flags.get(static_cast<size_t>(name));            \
    }                                                           \
    constexpr void set_##name(bool v) {                         \
        flags.set(static_cast<size_t>(name), v);                \
    }
#define FLAG_SET_ALL(name) set_##name(v);

#define DEFINE_FLAGS(TypeName, FLAG_LIST)                       \
struct TypeName {                                               \
    enum Flag : size_t {                                        \
        FLAG_LIST(FLAG_ENUM)                                    \
        _count,                                                 \
        Unknown                                                 \
    };                                                          \
    static TypeName createAllFalse()                            \
    {                                                           \
        TypeName result;                                        \
        result.flags.fill0();                                   \
        return result;                                          \
    }                                                           \
    static TypeName createAllTrue()                             \
    {                                                           \
        TypeName result;                                        \
        result.flags.fill1();                                   \
        return result;                                          \
    }                                                           \
    void setAll(bool v)                                         \
    {                                                           \
        FLAG_LIST(FLAG_SET_ALL)                                 \
    }                                                           \
                                                                \
    static_assert(_count >= 1, "At least one flag required");   \
    static_assert(_count <= 64, "Max 64 flags supported");      \
                                                                \
    wio::common::packed_flags<_count> flags;                    \
                                                                \
    FLAG_LIST(FLAG_METHOD)                                      \
}

namespace wio::sema
{
#define FLAGS8(X) X(f0) X(f1) X(f2) X(f3) X(f4) X(f5) X(f6) X(f7)
    DEFINE_FLAGS(Flags8, FLAGS8);
#undef FLAGS8

#define FLAGS16(X) X(f0) X(f1) X(f2) X(f3) X(f4) X(f5) X(f6) X(f7) X(f8) X(f9) X(f10) X(f11) X(f12) X(f13) X(f14) X(f15)
    DEFINE_FLAGS(Flags16, FLAGS16);
#undef FLAGS16 
}