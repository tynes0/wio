#pragma once

#include "../general/core.h"

#include <cstdint>

namespace wio
{
    enum class IntegerType : uint8_t
    {
        Unknown,
        i8,
        i16,
        i32,
        i64,
        u8,
        u16,
        u32,
        u64,
        isize,
        usize
    };
    
    enum class FloatType : uint8_t
    {
        Unknown,
        f32,
        f64
    };
    
    struct IntegerResult
    {
        IntegerType type = IntegerType::Unknown;
        
        union {
            i8  v_i8;
            i16 v_i16;
            i32 v_i32;
            i64 v_i64;
            u8  v_u8;
            u16 v_u16;
            u32 v_u32;
            u64 v_u64;
            isize  v_isize;
            usize v_usize;
        } value;
                
        bool isValid = false;
    };

    struct FloatResult
    {
        FloatType type = FloatType::Unknown;
        
        union Value {
            f32  v_f32;
            f64 v_f64;
        } value;
                
        bool isValid = false;
    };
}