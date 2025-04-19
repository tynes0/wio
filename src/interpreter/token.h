#pragma once

#include <unordered_map>
#include <string>

#include "../utils/frenum.h"
#include "../base/location.h"

namespace wio
{
    enum class token_type
    {
        kw_if, kw_else, kw_for, kw_foreach, kw_in, kw_while, kw_break, kw_continue, kw_return, kw_func, kw_null, kw_ref, kw_pure, kw_unit,
        kw_realm, kw_as, kw_var, kw_const, kw_array, kw_dict, kw_local, kw_global, kw_import, kw_typeof, kw_true, kw_false, 
        
        KW_COUNT /* JUST A COUNTER! NOT A KW*/,
        
        identifier, number, string, character, op,
        
        left_bracket, right_bracket,
        left_curly_bracket, right_curly_bracket,
        left_parenthesis, right_parenthesis,
        
        dot, comma, colon, semicolon,
        
        end_of_file
    };

    MakeFrenumInNamespace(wio, token_type,
        kw_if, kw_else, kw_for, kw_foreach, kw_in, kw_while, kw_break, kw_continue, kw_return, kw_func, kw_null, kw_ref, kw_pure, kw_unit,
        kw_realm, kw_as, kw_var, kw_const, kw_array, kw_dict, kw_local, kw_global, kw_import, kw_typeof, kw_true, kw_false, 
        KW_COUNT,
        identifier, number, string, character, op,
        left_bracket, right_bracket,
        left_curly_bracket, right_curly_bracket,
        left_parenthesis, right_parenthesis,
        dot, comma, colon, semicolon,
        end_of_file)

    struct token
    {
        token_type type = token_type::end_of_file;
        std::string value;
        location loc;
    };

    static const std::unordered_map<std::string, token_type> token_map =
    {
        {"if", token_type::kw_if},
        {"else", token_type::kw_else},
        {"for", token_type::kw_for},
        {"foreach", token_type::kw_foreach},
        {"in", token_type::kw_in},
        {"while", token_type::kw_while},
        {"break", token_type::kw_break},
        {"continue", token_type::kw_continue},
        {"return", token_type::kw_return},
        {"func", token_type::kw_func},
        {"null", token_type::kw_null},
        {"var", token_type::kw_var},
        {"const", token_type::kw_const},
        {"array", token_type::kw_array},
        {"dict", token_type::kw_dict},
        {"local", token_type::kw_local},
        {"global", token_type::kw_global},
        {"import", token_type::kw_import},
        {"typeof", token_type::kw_typeof},
        {"true", token_type::kw_true},
        {"false", token_type::kw_false},
        {"ref", token_type::kw_ref},
        {"pure", token_type::kw_pure},
        {"unit", token_type::kw_unit},
        {"realm", token_type::kw_realm},
        {"as", token_type::kw_as},

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

        {"{", token_type::left_curly_bracket},
        {"}", token_type::right_curly_bracket},
        {"[", token_type::left_bracket},
        {"]", token_type::right_bracket},
        {"(", token_type::left_parenthesis},
        {")", token_type::right_parenthesis},

        {"eof", token_type::end_of_file } // \0
    };
}