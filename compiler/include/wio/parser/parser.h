#pragma once

#include "wio/ast/ast.h"

#include <string_view>

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
        struct GenericParameterList
        {
            std::vector<NodePtr<Identifier>> parameters;
            bool hasParameterPack = false;
        };

        std::vector<Token> tokens_;
        size_t currentTokenIndex_;
        size_t asyncScopeCounter_ = 0;
        std::vector<std::string> asyncScopeNames_;
        bool requiresAsyncModule_ = false;

        Token peek(int offset = 0) const;
        Token previous() const;
        Token advance();
        void multiAdvance(int count);
        bool match(TokenType type, bool consume = false);
        bool match(TokenType type, std::string_view value, bool consume = false);
        bool multiMatch(const std::initializer_list<TokenType>& types, bool consume = false);
        bool matchOneOf(const std::initializer_list<TokenType>& types, bool consume = false);
        Token consume(TokenType type, std::string_view value = "");
        void consumeGenericClose();
        [[nodiscard]] bool matchIdentifier(bool consume = false);
        Token consumeIdentifier();
        [[nodiscard]] common::Location previousLocation() const;
        [[nodiscard]] common::Location currentOrPreviousLocation() const;
        void expectElementAfterComma(TokenType closingType, std::string_view elementDescription);
        void synchronize();

        NodePtr<Expression> parseExpression(int minPrecedence = 0, bool stopAtFit = false);
        NodePtr<Expression> parsePrimary();
        NodePtr<Expression> parseStringLiteral();
        NodePtr<Expression> parseArrayLiteral();
        NodePtr<Expression> parseDictionaryLiteral();
        NodePtr<Expression> parseLambdaExpression();
        NodePtr<Expression> parseMatchExpression();
        std::vector<NodePtr<TypeSpecifier>> parseExplicitTypeArgumentList();
        Token parseAttributeArgumentToken();
        
        NodePtr<TypeSpecifier> parseType();
        NodePtr<TypeSpecifier> parseGenericArgument();
        GenericParameterList parseGenericParameterList();
        NodePtr<AttributeStatement> parseWhereClause(const std::vector<NodePtr<Identifier>>& genericParameters,
                                                     bool hasGenericParameterPack);
        
        NodePtr<Statement> parseStatement();
        NodePtr<Statement> parseBlockStatement();
        NodePtr<Statement> parseAsyncScopeStatement();
        NodePtr<AttributeStatement> parseAttributeStatement(bool legacyAtSyntax = true);
        void parseBracketAttributeList(std::vector<NodePtr<AttributeStatement>>& attributes);
        void parseLeadingAttributes(std::vector<NodePtr<AttributeStatement>>& attributes,
                                    bool acceptBracketSyntax = true);
        [[nodiscard]] bool bracketAttributeListPrecedesDeclaration() const;
        void parseWithAttributeClause(std::vector<NodePtr<AttributeStatement>>& attributes);
        NodePtr<AttributeDeclaration> parseAttributeDeclaration();
        NodePtr<Statement> parseApplicationDeclaration();
        NodePtr<Statement> parseSystemDeclaration();
        NodePtr<VariableDeclaration> parseVariableDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<TypeAliasDeclaration> parseTypeAliasDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<FunctionDeclaration> parseFunctionDeclaration(std::vector<NodePtr<AttributeStatement>> attributes, bool isLifecycle = false, bool isStructMethod = false, bool isAsync = false);
        NodePtr<Statement> parseInterfaceDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseComponentDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseExtensionDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseObjectDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseEnumDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseFlagsetDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseFlagDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);
        NodePtr<Statement> parseIfStatement();
        NodePtr<Statement> parseWhileStatement();
        NodePtr<Statement> parseForInStatement();
        NodePtr<Statement> parseCForStatement(common::Location startLoc);
        NodePtr<Statement> parseBreakStatement();
        NodePtr<Statement> parseContinueStatement();
        NodePtr<Statement> parseReturnStatement();
        NodePtr<Statement> parseUseStatement();
        NodePtr<Statement> parseUsingStatement();
        NodePtr<Statement> parseRealmDeclaration(std::vector<NodePtr<AttributeStatement>> attributes);

        [[nodiscard]] static int getPrecedence(TokenType type);
        [[nodiscard]] bool canParseExplicitTypeArgumentCall() const;
        static void validateOrdinaryVariableDeclaration(Mutability mutability,
                                                        bool hasExplicitType,
                                                        bool hasInitializer,
                                                        common::Location location);

        [[noreturn]] static void utError(const std::string& message, common::Location location);
        [[noreturn]] static void ucError(common::Location location);
    };
}
