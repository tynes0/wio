#pragma once

#include "wio/ast/ast.h"

namespace wio
{
    class Parser
    {
    public:
        explicit Parser(std::vector<Token> tokens)
            : tokens_(std::move(tokens)), currentTokenIndex_(0)
        {
        }
        
        NodePtr<Program> parseProgram();
    private:
        std::vector<Token> tokens_;
        size_t currentTokenIndex_;

        Token peek(int offset = 0) const;
        Token advance();
        void multiAdvance(int count);
        bool match(TokenType type, bool consume = false);
        bool match(TokenType type, std::string_view value, bool consume = false);
        bool multiMatch(const std::initializer_list<TokenType>& types, bool consume = false);
        bool matchOneOf(const std::initializer_list<TokenType>& types, bool consume = false);
        Token consume(TokenType type, std::string_view value = "");
        void synchronize();

        NodePtr<Expression> parseExpression(int minPrecedence = 0, bool stopAtFit = false);
        NodePtr<Expression> parsePrimary();
        NodePtr<Expression> parseStringLiteral();
        NodePtr<Expression> parseArrayLiteral();
        NodePtr<Expression> parseDictionaryLiteral();
        NodePtr<Expression> parseLambdaExpression();
        NodePtr<Expression> parseMatchExpression();
        Token parseAttributeArgumentToken();
        
        NodePtr<TypeSpecifier> parseType();
        
        NodePtr<Statement> parseStatement();
        NodePtr<Statement> parseBlockStatement();
        NodePtr<AttributeStatement> parseAttributeStatement();
        NodePtr<VariableDeclaration> parseVariableDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<FunctionDeclaration> parseFunctionDeclaration(std::vector<NodePtr<AttributeStatement>> attributes, bool isLifecycle = false);
        NodePtr<Statement> parseInterfaceDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseComponentDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseObjectDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseEnumDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseFlagsetDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseFlagDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseIfStatement();
        NodePtr<Statement> parseWhileStatement();
        NodePtr<Statement> parseBreakStatement();
        NodePtr<Statement> parseContinueStatement();
        NodePtr<Statement> parseReturnStatement();
        NodePtr<Statement> parseUseStatement();
        NodePtr<Statement> parseRealmDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);

        [[nodiscard]] static int getPrecedence(TokenType type);

        [[noreturn]] static void utError(const std::string& message, common::Location location);
        [[noreturn]] static void ucError(common::Location location);
    };
}
