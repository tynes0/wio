#pragma once

#include <cstdint>

#define BIT(x) (1 << (x))
#define UBIT(x) (1u << (x))

#ifdef _MSC_VER
    /* MSVC Compiler */
    #define WIO_DEBUG_BREAK() __debugbreak()
#else
    /* GCC/Clang Compilers */
    #if defined(__i386__) || defined(__x86_64__)
        #define WIO_DEBUG_BREAK() __asm__ volatile("int $0x03")

    #elif defined(__thumb__)
        /* 'eabi_linux_thumb_le_breakpoint' */
        #define WIO_DEBUG_BREAK() __asm__ volatile(".inst 0xde01")

    #elif defined(__arm__)
        /* 'eabi_linux_arm_le_breakpoint' */
        #define WIO_DEBUG_BREAK() __asm__ volatile(".inst 0xe7f001f0")

    #elif defined(__aarch64__) && defined(__APPLE__)
        #define WIO_DEBUG_BREAK() __builtin_debugtrap()

    #elif defined(__aarch64__)
        /* 'aarch64_default_breakpoint' */
        #define WIO_DEBUG_BREAK() __asm__ volatile(".inst 0xd4200000")

    #elif defined(__powerpc__)
        /* 'rs6000_breakpoint' */
        #define WIO_DEBUG_BREAK() __asm__ volatile(".4byte 0x7d821008")

    #elif defined(__riscv)
        /* 'riscv_sw_breakpoint_from_kind' */
        #define WIO_DEBUG_BREAK() __asm__ volatile(".4byte 0x00100073")

    #else
        /* Fallback for other architectures */
        #include <signal.h>
        #define WIO_DEBUG_BREAK() raise(SIGTRAP)
    #endif

#endif /* _MSC_VER */

#define WIO_UNUSED(x) (void)(x)

namespace wio
{
    static constexpr size_t WIO_BUFSIZ = 2048; // 2 Kib
    
    using i8   = int8_t;
    using i16  = int16_t;
    using i32  = int32_t;
    using i64  = int64_t;

    using u8   = uint8_t;
    using u16  = uint16_t;
    using u32  = uint32_t;
    using u64  = uint64_t;

    using f32  = float;
    using f64  = double;

    using usize = size_t;
    using isize = ptrdiff_t;

    using byte   = unsigned char;
    using boolT  = bool;
    using charT  = char;
    using ucharT = unsigned char;
    using durationT = int64_t;

    struct voidT {};
}
