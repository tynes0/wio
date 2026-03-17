#include "wio/common/utility.h"
#include "wio/common/exception.h"

#include <charconv>


namespace wio::common
{
    bool isNewline(char c)
    {
        return c == '\n' || c == '\r';
    }

    char getEscapeSeq(char ch, Location loc)
    {
        if (ch == '\\' || ch == '\'' || ch == '\"')
            return ch;
        if (ch == 'n') // newline
            return '\n';
        if (ch == 'r') // return
            return '\r';
        if (ch == 'a') // alert
            return '\a';
        if (ch == 'b') // backspace
            return '\b';
        if (ch == 'f') // form feed page break
            return '\f';
        if (ch == 't') // horizontal tab
            return '\t';
        if (ch == 'v') // vertical tab
            return '\v';

        if (loc)
            throw InvalidStringError(("Invalid escape sequence: \\" + std::string(1, ch)).c_str(), loc);
        
        throw InvalidStringError(("Invalid escape sequence: \\" + std::string(1, ch)).c_str());
    }

    IntegerResult getInteger(const std::string& data)
    {
        IntegerResult result;
        result.type = IntegerType::Unknown;
        result.isValid = false;
        
        std::string_view sv = data;
        if (sv.empty()) return result;
    
        std::string_view rawNumStr = sv;
        IntegerType detectedType = IntegerType::Unknown;
    
        struct SuffixMap
        {
            const char* str;
            IntegerType type;
        };
        SuffixMap suffixes[] = {
            { .str = "isz", .type = IntegerType::isize}, {.str =  "usz", .type = IntegerType::usize},
            { .str = "i64", .type = IntegerType::i64}, { .str = "u64", .type = IntegerType::u64},
            { .str = "i32", .type = IntegerType::i32}, { .str = "u32", .type = IntegerType::u32},
            { .str = "i16", .type = IntegerType::i16}, { .str = "u16", .type = IntegerType::u16},
            { .str = "i8",  .type = IntegerType::i8},  { .str = "u8",  .type = IntegerType::u8}
        };
    
        for (const auto& s : suffixes)
        {
            if (sv.ends_with(s.str))
            {
                detectedType = s.type;
                rawNumStr = sv.substr(0, sv.size() - std::char_traits<char>::length(s.str));
                break;
            }
        }
    
        int64_t signedVal = 0;
        uint64_t unsignedVal = 0;
        bool isNegative = (!rawNumStr.empty() && rawNumStr[0] == '-');
        bool parseSuccess = false;

        if (isNegative)
            rawNumStr.remove_prefix(1);

        int base = 10;
        if (rawNumStr.starts_with("0x") || rawNumStr.starts_with("0X"))
        {
            base = 16;
            rawNumStr.remove_prefix(2);
        }
        else if (rawNumStr.starts_with("0b") || rawNumStr.starts_with("0B"))
        {
            base = 2;
            rawNumStr.remove_prefix(2);
        }
        else if (rawNumStr.starts_with("0o") || rawNumStr.starts_with("0O"))
        {
            base = 8;
            rawNumStr.remove_prefix(2);
        }
    
        if (isNegative)
        {
            auto res = std::from_chars(rawNumStr.data(), rawNumStr.data() + rawNumStr.size(), signedVal, base);
            if (res.ec == std::errc() && res.ptr == rawNumStr.data() + rawNumStr.size())
            {
                parseSuccess = true;
            }
        }
        else
        {
            auto res = std::from_chars(rawNumStr.data(), rawNumStr.data() + rawNumStr.size(), unsignedVal, base);
            if (res.ec == std::errc() && res.ptr == rawNumStr.data() + rawNumStr.size())
            {
                parseSuccess = true;
                if (unsignedVal <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                {
                    signedVal = static_cast<int64_t>(unsignedVal);
                }
                else
                {
                    // An unsigned value that doesn't fit in int64 (e.g., a very large u64)
                    // Using signedVal in this case might be dangerous; we should manage it with a flag.
                }
            }
        }
    
        if (!parseSuccess)
            return result;
    
        if (detectedType != IntegerType::Unknown)
        {
            if (isNegative && (detectedType == IntegerType::u8 || detectedType == IntegerType::u16 || 
                               detectedType == IntegerType::u32 || detectedType == IntegerType::u64 || 
                               detectedType == IntegerType::usize))
            {
                return result; 
            }
    
            auto checkAndSet = [&]<typename T, typename VT>(T min, T max, VT val, T& unionMember)
            {
                if (static_cast<T>(val) >= min && val <= max) {
                    unionMember = static_cast<T>(val);
                    result.type = detectedType;
                    result.isValid = true;
                }
            };
    
            switch (detectedType)
            {
                case IntegerType::i8:  checkAndSet(std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max(), signedVal, result.value.v_i8); break;
                case IntegerType::i16: checkAndSet(std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(), signedVal, result.value.v_i16); break;
                case IntegerType::i32: checkAndSet(std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), signedVal, result.value.v_i32); break;
                case IntegerType::i64: checkAndSet(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), signedVal, result.value.v_i64); break;
                
                case IntegerType::u8:  checkAndSet(static_cast<uint8_t>(0), std::numeric_limits<uint8_t>::max(), unsignedVal, result.value.v_u8); break;
                case IntegerType::u16: checkAndSet(static_cast<uint16_t>(0), std::numeric_limits<uint16_t>::max(), unsignedVal, result.value.v_u16); break;
                case IntegerType::u32: checkAndSet(static_cast<uint32_t>(0), std::numeric_limits<uint32_t>::max(), unsignedVal, result.value.v_u32); break;
                case IntegerType::u64: 
                    result.value.v_u64 = unsignedVal; result.type = IntegerType::u64; result.isValid = true; 
                    break;
    
                case IntegerType::isize: checkAndSet(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max(), signedVal, result.value.v_isize); break;
                case IntegerType::usize: 
                    result.value.v_usize = unsignedVal; result.type = IntegerType::usize; result.isValid = true; 
                    break;
                case IntegerType::Unknown:
                break;
            }
        } 
        else {
            if (isNegative)
            {
                if (signedVal >= std::numeric_limits<int32_t>::min())
                {
                    result.type = IntegerType::i32;
                    result.value.v_i32 = static_cast<int32_t>(signedVal);
                    result.isValid = true;
                } else
                {
                    result.type = IntegerType::i64;
                    result.value.v_i64 = signedVal;
                    result.isValid = true;
                }
            }
            else
            {
                if (unsignedVal <= static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
                {
                    result.type = IntegerType::i32;
                    result.value.v_i32 = static_cast<int32_t>(unsignedVal);
                    result.isValid = true;
                } 
                else if (unsignedVal <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                {
                    result.type = IntegerType::i64;
                    result.value.v_i64 = static_cast<int64_t>(unsignedVal);
                    result.isValid = true;
                } 
                else
                {
                    result.type = IntegerType::u64;
                    result.value.v_u64 = unsignedVal;
                    result.isValid = true;
                }
            }
        }
    
        return result;
    }

    FloatResult getFloat(const std::string& data)
    {
        FloatResult result{};
        result.isValid = false;
        result.type = FloatType::Unknown;
        std::memset(&result.value, 0, sizeof(result.value));

        if (data.empty())
            return result;

        std::string_view sv = data;
        std::string_view raw = sv;
        FloatType detectedType = FloatType::Unknown;

        struct Suffix {
            const char* str;
            FloatType type;
        };

        Suffix suffixes[] = {
            { .str = "f32", .type = FloatType::f32 },
            { .str = "f64", .type = FloatType::f64 }
        };

        for (auto& [str, type] : suffixes)
        {
            if (sv.ends_with(str))
            {
                detectedType = type;
                raw = sv.substr(0, sv.size() - std::char_traits<char>::length(str));
                break;
            }
        }

        std::string temp(raw);
        char* end = nullptr;

        errno = 0;
        double val = std::strtod(temp.c_str(), &end);

        if (errno != 0 || end != temp.c_str() + temp.size())
            return result;
        
        if (detectedType == FloatType::f32)
        {
            if (val < -std::numeric_limits<float>::max() ||
                val >  std::numeric_limits<float>::max())
                return result;

            result.value.v_f32 = static_cast<float>(val);
            result.type = FloatType::f32;
            result.isValid = true;
        }
        else
        {
            result.value.v_f64 = val;
            result.type = FloatType::f64;
            result.isValid = true;
        }

        return result;
    }


    std::string wioPrimitiveTypeToCppType(const std::string& wioTypeStr)
    {
        if (wioTypeStr == "void") return "void";
        if (wioTypeStr == "char") return "char";
        if (wioTypeStr == "uchar") return "unsigned char";
        if (wioTypeStr == "i8") return "int8_t";
        if (wioTypeStr == "i16") return "int16_t";
        if (wioTypeStr == "i32") return "int32_t";
        if (wioTypeStr == "i64") return "int64_t";
        if (wioTypeStr == "u8") return "uint8_t";
        if (wioTypeStr == "u16") return "uint16_t";
        if (wioTypeStr == "u32") return "uint32_t";
        if (wioTypeStr == "u64") return "uint64_t";
        if (wioTypeStr == "f32") return "float";
        if (wioTypeStr == "f64") return "double";
        if (wioTypeStr == "usize") return "size_t";
        if (wioTypeStr == "isize") return "ptrdiff_t";
        if (wioTypeStr == "byte") return "unsigned char";
        if (wioTypeStr == "bool") return "bool";
        if (wioTypeStr == "string") return "wio::String";
        if (wioTypeStr == "duration") return "wio::Duration";
        return {};
    }
}
