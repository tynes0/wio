#pragma once

#include <unordered_map>
#include <string>

#include "utils/frenum.h"

namespace wio
{
    enum class token_type
    {
        identifier, keyword, number, string, op,
        left_bracket, right_bracket,
        left_curly_bracket, right_curly_bracket,
        left_parenthesis, right_parenthesis,
        dot, comma, semicolon,
        end_of_line, end_of_file
    };

    MakeFrenumInNamespace(wio, token_type,
        identifier, keyword, number, string, op,
        left_bracket, right_bracket,
        left_curly_bracket, right_curly_bracket,
        left_parenthesis, right_parenthesis,
        dot, comma, semicolon,
        end_of_line, end_of_file)

        struct token
    {
        token_type type;
        std::string value;
    };

    static const std::unordered_map<std::string, token_type> token_map =
    {
        {"if", token_type::keyword},
        {"else", token_type::keyword},
        {"for", token_type::keyword},
        {"foreach", token_type::keyword},
        {"in", token_type::keyword},
        {"while", token_type::keyword},
        {"break", token_type::keyword},
        {"continue", token_type::keyword},
        {"return", token_type::keyword},

        {"func", token_type::keyword},
        {"var", token_type::keyword},
        {"const", token_type::keyword},
        {"array", token_type::keyword},
        {"dict", token_type::keyword},

        {"local", token_type::keyword},
        {"global", token_type::keyword},

        {"import", token_type::keyword},

        {"typeof", token_type::keyword},

        {"+", token_type::op},
        {"-", token_type::op},
        {"*", token_type::op},
        {"/", token_type::op},
        {"%", token_type::op},
        {"=", token_type::op},
        {"<", token_type::op},
        {">", token_type::op},
        {"!", token_type::op},
        {"&", token_type::op},
        {"|", token_type::op},
        {"~", token_type::op},
        {">=", token_type::op},
        {"<=", token_type::op},
        {"&&", token_type::op},
        {"||", token_type::op},
        {"==", token_type::op},
        {"!=", token_type::op},
        {"++", token_type::op},
        {"--", token_type::op},
        {"+=", token_type::op},
        {"-=", token_type::op},
        {"*=", token_type::op},
        {"/=", token_type::op},
        {"%=", token_type::op},

        {"{", token_type::left_curly_bracket},
        {"}", token_type::right_curly_bracket},
        {"[", token_type::left_bracket},
        {"]", token_type::right_bracket},
        {"(", token_type::left_parenthesis},
        {")", token_type::right_parenthesis},
        {".", token_type::dot},
        {",", token_type::comma},
        {";", token_type::semicolon},

        {"\n", token_type::end_of_line }, // \n
        {"eof", token_type::end_of_file } // \0
    };
}