#pragma once

#include <unordered_map>
#include <string>

#include "../utils/frenum.h"
#include "../base/location.h"

namespace wio
{
    enum class token_type : uint32_t
    {
        kw_var, kw_const, kw_array, kw_dict, kw_func, kw_realm, kw_omni, kw_local, kw_global, kw_enum,
        kw_if, kw_else, kw_while, kw_for, kw_foreach, 
        kw_break, kw_continue, kw_return, kw_import,
        kw_null, kw_true, kw_false,
        kw_unit, kw_exposed, kw_shared, kw_hidden, kw_trust, kw_final, kw_from, kw_override, kw_super, kw_outer, kw_access,
        kw_typeof, kw_ref, kw_pure, kw_as, kw_in, kw_forward,
        kw_try, kw_catch, kw_throw, kw_rethrow, kw_finally,
        
        KW_COUNT /* JUST A COUNTER! NOT A KW*/,
        
        identifier, number, string, character, op,
        
        left_bracket, right_bracket,
        left_curly_bracket, right_curly_bracket,
        left_parenthesis, right_parenthesis,
        
        dot, comma, colon, semicolon, at_sign,

        SPESIFICS_START/* JUST LOC! NOT A KW*/,

        s_ctor, s_dtor,

        SPESIFICS_END/* JUST LOC! NOT A KW*/,
        
        end_of_file
    };

    MakeFrenumInNamespace(wio, token_type,
        kw_var, kw_const, kw_array, kw_dict, kw_func, kw_realm, kw_omni, kw_local, kw_global, kw_enum,
        kw_if, kw_else, kw_while, kw_for, kw_foreach,
        kw_break, kw_continue, kw_return, kw_import,
        kw_null, kw_true, kw_false,
        kw_unit, kw_exposed, kw_shared, kw_hidden, kw_trust, kw_final, kw_from, kw_override, kw_super, kw_outer, kw_access,
        kw_typeof, kw_ref, kw_pure, kw_as, kw_in, kw_forward,
        kw_try, kw_catch, kw_throw, kw_rethrow, kw_finally,
        KW_COUNT,
        identifier, number, string, character, op,
        left_bracket, right_bracket,
        left_curly_bracket, right_curly_bracket,
        left_parenthesis, right_parenthesis,
        dot, comma, colon, semicolon, at_sign,
        SPESIFICS_START,
        s_ctor, s_dtor,
        SPESIFICS_END,
        end_of_file)

    struct token
    {
        token_type type = token_type::end_of_file;
        std::string value;
        location loc;
    };

    namespace token_constants
    {
        static constexpr const char* ctor_str = ".ctor";
        static constexpr const char* dtor_str = ".dtor";
    }

    // TODO: maybe constexpr std::array<std::pair<std::string or const char*, token_type>>
    static const std::unordered_map<std::string, token_type> token_map =
    {
        {"var", token_type::kw_var},
        {"const", token_type::kw_const},
        {"array", token_type::kw_array},
        {"dict", token_type::kw_dict},
        {"func", token_type::kw_func},
        {"realm", token_type::kw_realm},
        {"omni", token_type::kw_omni},
        {"local", token_type::kw_local},
        {"global", token_type::kw_global},
        {"enum", token_type::kw_enum},
        {"if", token_type::kw_if},
        {"else", token_type::kw_else},
        {"while", token_type::kw_while},
        {"for", token_type::kw_for},
        {"foreach", token_type::kw_foreach},
        {"break", token_type::kw_break},
        {"continue", token_type::kw_continue},
        {"return", token_type::kw_return},
        {"import", token_type::kw_import},
        {"null", token_type::kw_null},
        {"true", token_type::kw_true},
        {"false", token_type::kw_false},
        {"unit", token_type::kw_unit},
        {"exposed", token_type::kw_exposed},
        {"shared", token_type::kw_shared},
        {"hidden", token_type::kw_hidden},
        {"trust", token_type::kw_trust},
        {"final", token_type::kw_final},
        {"from", token_type::kw_from},
        {"override", token_type::kw_override},
        {"super", token_type::kw_super},
        {"outer", token_type::kw_outer},
        {"access", token_type::kw_access},
        {"typeof", token_type::kw_typeof},
        {"ref", token_type::kw_ref},
        {"pure", token_type::kw_pure},
        {"as", token_type::kw_as},
        {"in", token_type::kw_in},
        {"forward", token_type::kw_forward},
        {"try", token_type::kw_try},
        {"catch", token_type::kw_catch},
        {"throw", token_type::kw_throw},
        {"rethrow", token_type::kw_rethrow},
        {"finally", token_type::kw_finally},

        {"+", token_type::op},
        {"-", token_type::op},
        {"*", token_type::op},
        {"/", token_type::op},
        {"%", token_type::op},

        {"=", token_type::op},
        {"+=", token_type::op},
        {"-=", token_type::op},
        {"*=", token_type::op},
        {"/=", token_type::op},
        {"%=", token_type::op},

        {"<", token_type::op},
        {">", token_type::op},
        {"<=", token_type::op},
        {">=", token_type::op},
        {"==", token_type::op},
        {"!=", token_type::op},
        {"=?", token_type::op}, // Exclusive to wio.(type equal)
        {"<=>", token_type::op}, // Exclusive to wio. (compare all)
        {"&&", token_type::op},
        {"||", token_type::op},
        {"^^", token_type::op},

        {"&", token_type::op},
        {"|", token_type::op},
        {"^", token_type::op},
        {"&=", token_type::op},
        {"|=", token_type::op},
        {"^=", token_type::op},

        {"<<", token_type::op},
        {">>", token_type::op},
        {"<<=", token_type::op},
        {">>=", token_type::op},

        {"++", token_type::op},
        {"--", token_type::op},
        {"~", token_type::op},
        {"!", token_type::op},
        {"?", token_type::op},  // Exclusive to wio. (is not null)

        {"->", token_type::op}, // Exclusive to wio. (to right)
        {"<-", token_type::op}, // Exclusive to wio. (to left)

        {".", token_type::dot},
        {",", token_type::comma},
        {":", token_type::colon},
        {";", token_type::semicolon},
        {"@", token_type::at_sign},

        {"{", token_type::left_curly_bracket},
        {"}", token_type::right_curly_bracket},
        {"[", token_type::left_bracket},
        {"]", token_type::right_bracket},
        {"(", token_type::left_parenthesis},
        {")", token_type::right_parenthesis},

        {token_constants::ctor_str, token_type::s_ctor},
        {token_constants::dtor_str, token_type::s_dtor},

        {"eof", token_type::end_of_file } // \0
    };

    inline bool is_specific(token_type tt)
    {
        auto idx = frenum::index(tt);

        if (!idx.has_value())
            return false;

        constexpr size_t ssi = frenum::index(token_type::SPESIFICS_START).value();
        constexpr size_t sei = frenum::index(token_type::SPESIFICS_END).value();
        size_t tti = idx.value();

        return (tti > ssi && tti < sei);
    }

    inline bool is_specific(const std::string& str)
    {
        if (token_map.count(str) == 0)
            return false;

        auto idx = frenum::index(token_map.at(str));

        constexpr size_t ssi = frenum::index(token_type::SPESIFICS_START).value();
        constexpr size_t sei = frenum::index(token_type::SPESIFICS_END).value();
        size_t tti = idx.value();

        return (tti > ssi && tti < sei);
    }
}