#include "wio/parser/parser.h"

#include "wio/common/exception.h"
#include "wio/common/utility.h"
#include "wio/common/logger.h"

#include "general/traits/integer_traits.h"
#include "general/traits/float_traits.h"

#include <algorithm>

namespace wio
{
    using namespace common;
    
    NodePtr<Program> Parser::parseProgram()
    {
        std::vector<NodePtr<Statement>> statements;

        while (peek().isValid())
        {
            try 
            {
                if (NodePtr<Statement> statement = parseStatement(); statement)
                {
                    statements.emplace_back(std::move(statement));
                }
            }
            catch (const std::exception&)
            {
                synchronize();
            }
        }

        return makeNodePtr<Program>(std::move(statements));
    }

    Token Parser::peek(int offset) const
    {
        size_t index = currentTokenIndex_ + offset;
        if (index < tokens_.size())
            return tokens_[index];
        return Token::invalid();
    }

    Token Parser::advance()
    {
        Token current = peek();
        if (currentTokenIndex_ < tokens_.size())
            currentTokenIndex_++;
        return current;
    }

    void Parser::multiAdvance(int count)
    {
        while (count--) advance();
    }

    bool Parser::match(TokenType type, bool consume)
    {
        Token current = peek();
        
        if (current.type != type)
            return false;

        if (consume)
            advance();
        
        return true;
    }

    bool Parser::match(TokenType type, std::string_view value, bool consume)
    {
        Token current = peek();
        
        if (current.type != type)
        {
            return false;
        }

        if (value.empty() || current.value == value)
        {
            if (consume) advance();
            return true;
        }
        
        return false;
    }

    bool Parser::multiMatch(const std::initializer_list<TokenType>& types, bool consume)
    {
        for (auto type : types)
        {
            if (!match(type, false))
                return false;
        }

        if (consume)
            multiAdvance(static_cast<int>(types.size()));

        return true;
    }

    bool Parser::matchOneOf(const std::initializer_list<TokenType>& types, bool consume)
    {
        return std::ranges::any_of(types, [&](TokenType type)
        {
            return (match(type, consume));
        });
    }

    Token Parser::consume(TokenType type, std::string_view value)
    {
        if (match(type, value))
            return advance();

        Token current = peek();

        if (value.empty())
        {
            std::string formattedErrMsg = formatString(
             "Unexpected token: expected {0}, but got {1}!",
                 tokenTypeToString(type),
                 tokenTypeToString(current.type)
             );
         
            utError(formattedErrMsg, current.loc);
        }

        std::string formattedErrMsg = formatString(
            "Unexpected token: expected {} with value of {}, but got {} with value of {}.",
                tokenTypeToString(type),
                value,
                tokenTypeToString(current.type),
                current.value
            );
        
        utError(formattedErrMsg, current.loc);
    }

    void Parser::synchronize()
    {
        advance();

        while (peek().isValid())
        {
            if (peek(-1).type == TokenType::semicolon) return;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (peek().type)
            {
            case TokenType::kwFn:
            case TokenType::kwLet:
            case TokenType::kwMut:
            case TokenType::kwConst:
            case TokenType::kwComponent:
            case TokenType::kwObject:
            case TokenType::kwInterface:
            case TokenType::kwEnum:
            case TokenType::kwFlagset:
            case TokenType::kwFlag:
            case TokenType::kwRealm:
            case TokenType::kwIf:
            case TokenType::kwWhile:
            case TokenType::kwFor:
            case TokenType::kwReturn:
                return;
            default:
                advance();
            }
        }
    }

    NodePtr<Expression> Parser::parseExpression(int minPrecedence, bool stopAtFit)
    {
        NodePtr<Expression> left;
       
        if (peek().isUnary())
        {
            Token op = advance(); 
            
            if (op.type == TokenType::kwRef)
            {
                NodePtr<Expression> operand = parseExpression(getPrecedence(TokenType::kwRef), stopAtFit);
                left = makeNodePtr<RefExpression>(false, std::move(operand), op.loc);
            }
            else
            {
                int precedence = getPrecedence(op.type);
                NodePtr<Expression> operand = parseExpression(precedence, stopAtFit);
                left = makeNodePtr<UnaryExpression>(std::move(op), std::move(operand));
            }
        }
        else
        {
            left = parsePrimary();
        }

        while (true)
        {
            if (stopAtFit && peek().type == TokenType::kwFit)
                break;
            
            int precedence = getPrecedence(peek().type);
            if (precedence < minPrecedence)
                break;

            if (peek().type == TokenType::opGreater && peek(1).type == TokenType::rightBrace)
                break;
            
            if (match(TokenType::leftParen))
            {
                advance();
                std::vector<NodePtr<Expression>> args;

                if (!match(TokenType::rightParen))
                {
                    do
                    {
                        if (match(TokenType::rightParen))
                            utError("The parameter is missing.", peek(-1).loc);
                        args.push_back(parseExpression());
                    } while (match(TokenType::comma, true));
                }

                consume(TokenType::rightParen);

                left = makeNodePtr<FunctionCallExpression>(std::move(left), std::move(args));
                continue;
            }
            if (match(TokenType::leftBracket))
            {
                advance();
                NodePtr<Expression> index = parseExpression();
                consume(TokenType::rightBracket);
                left = makeNodePtr<ArrayAccessExpression>(std::move(left), std::move(index));
                continue;
            }
            if (matchOneOf({ TokenType::opDot, TokenType::opScope }))
            {
                Token op = advance();
                NodePtr<Identifier> member = makeNodePtr<Identifier>(consume(TokenType::identifier));
                left = makeNodePtr<MemberAccessExpression>(std::move(left), std::move(member), op.type);
                continue;
            }
            if (peek().isAssignment())
            {
                Token op = advance();
                NodePtr<Expression> rhs = parseExpression(precedence, stopAtFit);
                left = makeNodePtr<AssignmentExpression>(std::move(left), std::move(op), std::move(rhs));
                continue;
            }
            if (match(TokenType::kwFit, true))
            {
                Location fitLoc = peek(-1).loc;
                auto targetType = parseType(); 
            
                left = makeNodePtr<FitExpression>(std::move(left), std::move(targetType), fitLoc);
                continue;
            }
            
            Token op = advance();
            NodePtr<Expression> right = parseExpression(precedence + 1, stopAtFit);

            if (op.type == TokenType::opRangeInclusive || op.type == TokenType::opRangeExclusive)
            {
                bool isInclusive = (op.type == TokenType::opRangeInclusive);
                left = makeNodePtr<RangeExpression>(std::move(left), std::move(right), isInclusive, op.loc);
            }
            else
            {
                Location loc = op.loc;
                left = makeNodePtr<BinaryExpression>(std::move(left), std::move(op), std::move(right), loc);
            }
        }
        
        return left;
    }

    NodePtr<Expression> Parser::parsePrimary()
    {
        if (match(TokenType::integerLiteral, true))
            return makeNodePtr<IntegerLiteral>(peek(-1));

        if (match(TokenType::floatLiteral, true))
            return makeNodePtr<FloatLiteral>(peek(-1));

        if (match(TokenType::stringLiteral))
            return parseStringLiteral();

        if (match(TokenType::identifier, true))
            return makeNodePtr<Identifier>(peek(-1));

        if (match(TokenType::charLiteral, true))
            return makeNodePtr<CharLiteral>(peek(-1));

        if (matchOneOf({ TokenType::kwTrue , TokenType::kwFalse }, true))
            return makeNodePtr<BoolLiteral>(peek(-1));
        
        if (match(TokenType::kwNull, true))
            return makeNodePtr<NullExpression>(peek(-1).loc);

        if (match(TokenType::kwSelf, true))
            return makeNodePtr<SelfExpression>(peek(-1).loc);
            
        if (match(TokenType::kwSuper, true))
            return makeNodePtr<SuperExpression>(peek(-1).loc);

        if (match(TokenType::durationLiteral, true))
            return makeNodePtr<DurationLiteral>(peek(-1));
        
        if (match(TokenType::byteLiteral, true))
            return makeNodePtr<ByteLiteral>(peek(-1));

        if (match(TokenType::kwMatch))
            return parseMatchExpression();

        if (match(TokenType::leftParen))
        {
            bool isLambda = false;
            int offset = 1;
            int parenCount = 1;
            
            while (peek(offset).isValid()) 
            {
                if (peek(offset).type == TokenType::leftParen)
                    parenCount++;
                else if (peek(offset).type == TokenType::rightParen)
                    parenCount--;

                if (parenCount == 0) 
                {
                    TokenType nextType = peek(offset + 1).type;
                    if (nextType == TokenType::opFatArrow || nextType == TokenType::opArrow)
                        isLambda = true;
                    break;
                }
                offset++;
            }

            if (isLambda)
                return parseLambdaExpression();
            
            advance();
            NodePtr<Expression> expr = parseExpression();
            consume(TokenType::rightParen);
            return expr;
        }
        if (match(TokenType::kwRef))
        {
            Location startLoc = advance().loc;

            bool isMut = match(TokenType::kwMut, true);
            
            NodePtr<Expression> operand = parseExpression(getPrecedence(TokenType::kwRef)); 
            
            return makeNodePtr<RefExpression>(isMut, std::move(operand), startLoc);
        }
        if (match(TokenType::leftBracket))
            return parseArrayLiteral();

        // Dictionary Literal: { ... }
        // Note: In the context of a statement, '{' initializes a block, but since it expects a parsePrimary Expression,
        // the '{' here refers to a dictionary literal (or later, a struct init).
        if (match(TokenType::leftBrace))
        {
            return parseDictionaryLiteral();
        }

        utError("Expected expression.", peek().loc);
    }

    NodePtr<Expression> Parser::parseStringLiteral()
    {
        Token startTok = consume(TokenType::stringLiteral);

        if (!match(TokenType::dollar))
        {
            return makeNodePtr<StringLiteral>(std::move(startTok));
        }
        
        std::vector<NodePtr<Expression>> parts;
        
        parts.emplace_back(makeNodePtr<StringLiteral>(startTok));

        while (match(TokenType::dollar, true))
        {
            consume(TokenType::leftBrace);
            
            parts.push_back(parseExpression());
            
            consume(TokenType::rightBrace);
            
            Token nextPart = consume(TokenType::stringLiteral);
            
            parts.emplace_back(makeNodePtr<StringLiteral>(std::move(nextPart)));
        }

        return makeNodePtr<InterpolatedStringLiteral>(std::move(parts), startTok.loc);
    }

    NodePtr<Expression> Parser::parseArrayLiteral()
    {
        Location startLoc = peek().loc;
        consume(TokenType::leftBracket); // [

        std::vector<NodePtr<Expression>> elements;
    
        if (!match(TokenType::rightBracket))
        {
            do
            {
                elements.push_back(parseExpression());
            }
            while (match(TokenType::comma, true));
        }

        consume(TokenType::rightBracket);
        return makeNodePtr<ArrayLiteral>(std::move(elements), startLoc);
    }

    NodePtr<Expression> Parser::parseDictionaryLiteral()
    {
        Location startLoc = peek().loc;
        consume(TokenType::leftBrace);
        
        bool isOrdered = match(TokenType::opLess, true); 
        
        std::vector<std::pair<NodePtr<Expression>, NodePtr<Expression>>> pairs;
        
        bool isEmpty = isOrdered ? (peek().type == TokenType::opGreater && peek(1).type == TokenType::rightBrace) 
                                 : (peek().type == TokenType::rightBrace);

        if (!isEmpty)
        {
            do
            {
                if (isOrdered && peek().type == TokenType::opGreater) break;
                if (!isOrdered && peek().type == TokenType::rightBrace) break;
                
                NodePtr<Expression> key = parseExpression();
                consume(TokenType::opColon);
                NodePtr<Expression> value = parseExpression();

                pairs.emplace_back(std::move(key), std::move(value));
            }
            while (match(TokenType::comma, true));
        }

        if (isOrdered) consume(TokenType::opGreater); 
        consume(TokenType::rightBrace);

        return makeNodePtr<DictionaryLiteral>(std::move(pairs), isOrdered, startLoc);
    }

    NodePtr<Expression> Parser::parseLambdaExpression()
    {
        Location startLoc = peek().loc;
        consume(TokenType::leftParen);

        std::vector<Parameter> parameters;
        if (!match(TokenType::rightParen))
        {
            do
            {
                NodePtr<Identifier> paramName = makeNodePtr<Identifier>(consume(TokenType::identifier));
                NodePtr<TypeSpecifier> paramType = nullptr;

                if (match(TokenType::opColon, true))
                {
                    paramType = parseType();
                }

                parameters.emplace_back(std::move(paramName), std::move(paramType));
            } while (match(TokenType::comma, true));
        }
        consume(TokenType::rightParen);

        NodePtr<TypeSpecifier> returnType = nullptr;
        if (match(TokenType::opArrow, true)) 
        {
            returnType = parseType();
        }

        consume(TokenType::opFatArrow);

        NodePtr<Statement> body;
        if (match(TokenType::leftBrace))
        {
            body = parseBlockStatement();
        }
        else
        {
            auto expr = parseExpression();
            body = makeNodePtr<ExpressionStatement>(std::move(expr), peek(-1).loc);
        }

        return makeNodePtr<LambdaExpression>(std::move(parameters), std::move(returnType), std::move(body), startLoc);
    }

    NodePtr<Expression> Parser::parseMatchExpression()
    {
        Token startTok = consume(TokenType::kwMatch);
        consume(TokenType::leftParen);
        auto value = parseExpression();
        consume(TokenType::rightParen);
        consume(TokenType::leftBrace);

        std::vector<MatchCase> cases;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<Expression>> matchValues;

            if (match(TokenType::kwAssumed, true))
            {
            }
            else
            {
                do
                {
                    matchValues.push_back(parseExpression(3));
                }
                while (match(TokenType::comma, true) || match(TokenType::kwOr, true));
            }

            consume(TokenType::opColon);
            
            NodePtr<Statement> body = parseStatement(); 
            
            cases.emplace_back(std::move(matchValues), std::move(body));
        }
        consume(TokenType::rightBrace);

        return makeNodePtr<MatchExpression>(std::move(value), std::move(cases), startTok.loc);
    }

    NodePtr<TypeSpecifier> Parser::parseType()
    {
        if (match(TokenType::kwRef, true))
        {
            Location startLoc = peek(-1).loc;
            auto innerType = parseType();
            
            Token token { .type = TokenType::kwRef, .value = "ref", .loc = startLoc };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(innerType));
            
            return makeNodePtr<TypeSpecifier>(std::move(token), std::move(generics), 0, true, true, startLoc);
        }
        if (match(TokenType::kwView, true))
        {
            Location startLoc = peek(-1).loc;
            auto innerType = parseType();
            
            Token token { .type = TokenType::kwView, .value = "view", .loc = startLoc };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(innerType));
            
            return makeNodePtr<TypeSpecifier>(std::move(token), std::move(generics), 0, true, false, startLoc);
        }

        if (match(TokenType::leftBracket, true))
        {
            Location startLoc = peek(-1).loc;
            
            auto innerType = parseType();

            size_t size = 0;
            if (match(TokenType::semicolon, true))
            {
                if (match(TokenType::integerLiteral, true))
                {
                    size = traits::IntegerTraits<size_t>::IntegerResultCastedAs(getInteger(peek(-1).value));
                }
                else if (match(TokenType::floatLiteral, true))
                {
                    size = static_cast<size_t>(traits::FloatTraits<f64>::FloatResultCastedAs(getFloat(peek(-1).value)));
                }
            }
            
            consume(TokenType::rightBracket);

            Token arrayToken {
                .type = TokenType::StaticArray,
                .value ="",
                .loc = startLoc
            };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(innerType));

            return makeNodePtr<TypeSpecifier>(arrayToken, std::move(generics), size, false, false, startLoc);
        }

        if (match(TokenType::kwFn, true))
        {
            Location startLoc = peek(-1).loc;
            consume(TokenType::leftParen);
            
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.emplace_back(nullptr);

            if (!match(TokenType::rightParen))
            {
                do {
                    generics.push_back(parseType());
                } while (match(TokenType::comma, true));
            }
            consume(TokenType::rightParen);

            NodePtr<TypeSpecifier> retType = nullptr;
            if (match(TokenType::opArrow, true))
            {
                retType = parseType();
            }
            else
            {
                Token voidTok {
                    .type = TokenType::identifier,
                    .value = "void",
                    .loc = peek(-1).loc
                };
                retType = makeNodePtr<TypeSpecifier>(std::move(voidTok), std::vector<NodePtr<TypeSpecifier>>{}, 0, false, false, peek(-1).loc);
            }
            
            generics[0] = std::move(retType);

            Token fnTok {
                .type = TokenType::kwFn,
                .value = "fn",
                .loc = startLoc
            };
            return makeNodePtr<TypeSpecifier>(std::move(fnTok), std::move(generics), 0, false, false, startLoc);
        }

        match(TokenType::kwConst, true);

        Token typeName = Token::invalid();

        if (peek().isType() || peek().isIdentifier())
            typeName = advance();
        else
            utError("Expected type name.", peek().loc);
        
        Location startLoc = typeName.loc;
        std::vector<NodePtr<TypeSpecifier>> generics;

        if (match(TokenType::opLess, true))
        {
            do
            {
                generics.push_back(parseType());
            } while (match(TokenType::comma, true));

            consume(TokenType::opGreater);
        }

        auto result = makeNodePtr<TypeSpecifier>(std::move(typeName), std::move(generics), 0, false, false, startLoc);

        while (match(TokenType::leftBracket, true))
        {
            consume(TokenType::rightBracket);

            Token DynArrayToken{
                .type = TokenType::DynamicArray,
                .value ="",
                .loc = startLoc
            }; 
            std::vector<NodePtr<TypeSpecifier>> args;
            args.push_back(std::move(result));

            result = makeNodePtr<TypeSpecifier>(std::move(DynArrayToken), std::move(args), 0, false, false, startLoc);
        }

        return result;
    }

    NodePtr<Statement> Parser::parseStatement()
    {
        if (matchOneOf({ TokenType::semicolon, TokenType::endOfFile }, true))
            return nullptr;

        std::vector<NodePtr<AttributeStatement>> attributes;
        while (match(TokenType::atSign))
            attributes.push_back(parseAttributeStatement());
        
        if (peek().isKeyword())
        {
            if (matchOneOf({ TokenType::kwLet, TokenType::kwMut, TokenType::kwConst }))
                return parseVariableDeclaration(std::move(attributes));
            if (match(TokenType::kwFn))
                return parseFunctionDeclaration(std::move(attributes));
            if (match(TokenType::kwInterface))
                return parseInterfaceDeclaration(std::move(attributes));
            if (match(TokenType::kwComponent))
                return parseComponentDeclaration(std::move(attributes));
            if (match(TokenType::kwObject))
                return parseObjectDeclaration(std::move(attributes));
            if (match(TokenType::kwEnum))
                return parseEnumDeclaration(std::move(attributes));
            if (match(TokenType::kwFlagset))
                return parseFlagsetDeclaration(std::move(attributes));
            if (match(TokenType::kwFlag))
                return parseFlagDeclaration(std::move(attributes));
            if (match(TokenType::kwRealm))
                return parseRealmDeclaration(std::move(attributes));
            if (match(TokenType::kwIf))
                return parseIfStatement();
            if (match(TokenType::kwWhile))
                return parseWhileStatement();
            if (match(TokenType::kwBreak))
                return parseBreakStatement();
            if (match(TokenType::kwContinue))
                return parseContinueStatement();
            if (match(TokenType::kwReturn))
                return parseReturnStatement();
            if (match(TokenType::kwUse))
                return parseUseStatement();
        }
        else if (peek().isOperator())
        {
            NodePtr<Expression> expr = parseExpression();
            consume(TokenType::semicolon);
            return makeNodePtr<ExpressionStatement>(std::move(expr), peek().loc);
        }
        else if (peek().isSymbol())
        {
            if (match(TokenType::leftBrace))
                return parseBlockStatement();
        }
        
        NodePtr<Expression> expr = parseExpression();
        if (expr->is<MatchExpression>())
            match(TokenType::semicolon, true);
        else
            consume(TokenType::semicolon);
        return makeNodePtr<ExpressionStatement>(std::move(expr));
    }

    NodePtr<Statement> Parser::parseBlockStatement()
    {
        std::vector<NodePtr<Statement>> statements;

        consume(TokenType::leftBrace);

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            if (NodePtr<Statement> statement = parseStatement(); statement)
            {
                statements.emplace_back(std::move(statement));
            }
        }
        
        consume(TokenType::rightBrace);

        return makeNodePtr<BlockStatement>(std::move(statements));
    }

    NodePtr<AttributeStatement> Parser::parseAttributeStatement()
    {
        Token startTok = consume(TokenType::atSign);
        Location startLoc = startTok.loc;

        Token id = consume(TokenType::identifier);

        std::vector<Token> args;
        if (match(TokenType::leftParen, true))
        {
            if (!match(TokenType::rightParen))
            {
                do
                {
                    args.push_back(advance());
                } while (match(TokenType::comma, true));
            }
            consume(TokenType::rightParen);
        }

        if (std::optional<Attribute> attribute = frenum::cast<Attribute>(id.value); attribute.has_value())
        {
            return makeNodePtr<AttributeStatement>(attribute.value(), args, startLoc);
        }
        return makeNodePtr<AttributeStatement>(Attribute::Unknown, args, startLoc);
    }

    NodePtr<VariableDeclaration> Parser::parseVariableDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = advance();
        // NOLINTNEXTLINE
        Mutability mutability = Mutability::Immutable;
        
        // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
        switch (startTok.type)
        {
        case TokenType::kwLet:   mutability = Mutability::Immutable; break;
        case TokenType::kwMut:   mutability = Mutability::Mutable; break;
        case TokenType::kwConst: mutability = Mutability::Const; break;
        default:
            utError("Unexpected variable qualifier.", startTok.loc);
        }

        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));

        NodePtr<TypeSpecifier> specifier;
        if (match(TokenType::opColon, true))
            specifier = parseType();

        NodePtr<Expression> initializer = nullptr;

        if (match(TokenType::opAssign))
        {
            advance();
            initializer = parseExpression();
        }
        else
        {
            if (mutability == Mutability::Const)
            {
                ucError(startTok.loc);
            }
        }
        
        consume(TokenType::semicolon);

        return makeNodePtr<VariableDeclaration>(
            std::move(attributes),
            mutability,
            std::move(name), 
            std::move(specifier),
            std::move(initializer),
            startTok.loc
        );
    }

    NodePtr<FunctionDeclaration> Parser::parseFunctionDeclaration(std::vector<NodePtr<AttributeStatement>> attributes, bool isLifecycle)
    {
        Token startTok = !isLifecycle ? consume(TokenType::kwFn) : peek();
        
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));

        consume(TokenType::leftParen);
        
        std::vector<Parameter> parameters;
        if (!match(TokenType::rightParen))
        {
            do
            {
                NodePtr<Identifier> paramName = makeNodePtr<Identifier>(consume(TokenType::identifier));
                NodePtr<TypeSpecifier> paramType = nullptr;

                // param: Type
                if (match(TokenType::opColon, true))
                {
                    paramType = parseType();
                }

                parameters.emplace_back(std::move(paramName), std::move(paramType));

            } while (match(TokenType::comma, true));
        }
        consume(TokenType::rightParen);

        NodePtr<TypeSpecifier> returnType = nullptr;
        if (match(TokenType::opArrow, true)) // '->'
        {
            returnType = parseType();
        }
        
        NodePtr<Expression> whenCond = nullptr;
        NodePtr<Expression> whenFallback = nullptr;

        if (match(TokenType::kwWhen, true))
        {
            whenCond = parseExpression();
            
            if (match(TokenType::kwElse, true))
                whenFallback = parseExpression();
        }

        NodePtr<Statement> body = match(TokenType::semicolon, true) ? nullptr : parseBlockStatement();

        return makeNodePtr<FunctionDeclaration>(
            std::move(attributes),
            std::move(name),
            std::move(parameters),
            std::move(returnType),
            std::move(whenCond),
            std::move(whenFallback),
            std::move(body),
            startTok.loc
        );
    }

    NodePtr<Statement> Parser::parseInterfaceDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwInterface);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));

        consume(TokenType::leftBrace);
        std::vector<NodePtr<FunctionDeclaration>> methods;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> methodAttrs;
            while (peek().type == TokenType::atSign)
                methodAttrs.push_back(parseAttributeStatement());

            auto method = parseFunctionDeclaration(std::move(methodAttrs), false);
            
            if (method->body != nullptr) {
                utError("Interface methods cannot have a body. Use ';' instead of '{...}'.", method->location());
            }

            methods.push_back(std::move(method));
        }
        consume(TokenType::rightBrace);
        
        return makeNodePtr<InterfaceDeclaration>(std::move(attributes), std::move(name), std::move(methods), startTok.loc);
    }

    NodePtr<Statement> Parser::parseComponentDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwComponent);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));

        consume(TokenType::leftBrace);
        std::vector<ComponentMember> members;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> memberAttrs;
            while (peek().type == TokenType::atSign)
                memberAttrs.push_back(parseAttributeStatement());

            AccessModifier access = AccessModifier::None; 
            if (match(TokenType::kwPublic, true)) access = AccessModifier::Public;
            else if (match(TokenType::kwPrivate, true)) access = AccessModifier::Private;
            else if (match(TokenType::kwProtected, true)) access = AccessModifier::Protected;

            if (match(TokenType::kwFn) ||
                match(TokenType::identifier, "OnConstruct", false) ||
                match(TokenType::identifier, "OnDestruct", false))
            {
                bool isLifecycle = !match(TokenType::kwFn);
                auto method = parseFunctionDeclaration(std::move(memberAttrs), isLifecycle);
                
                members.push_back(ComponentMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(method)
                });
            }
            else
            {
                NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consume(TokenType::identifier));
                
                NodePtr<TypeSpecifier> memberType = nullptr;
                if (match(TokenType::opColon, true)) memberType = parseType();
                
                NodePtr<Expression> init = nullptr;
                if (match(TokenType::opAssign, true)) init = parseExpression();

                if (!memberType && !init) {
                    utError("Component members must have an explicit type or an initializer.", peek(-1).loc);
                }

                match(TokenType::comma, true);
                match(TokenType::semicolon, true); 

                auto varDecl =
                    makeNodePtr<VariableDeclaration>(
                        std::move(memberAttrs),
                        Mutability::Mutable,
                        std::move(memberName),
                        std::move(memberType),
                        std::move(init),
                        peek(-1).loc
                    );

                members.push_back(ComponentMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(varDecl)
                });
            }
        }
        consume(TokenType::rightBrace);
        return makeNodePtr<ComponentDeclaration>(std::move(attributes), std::move(name), std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseObjectDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwObject);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));

        consume(TokenType::leftBrace);
        std::vector<ObjectMember> members;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> memberAttrs;
            while (peek().type == TokenType::atSign)
                memberAttrs.push_back(parseAttributeStatement());

            AccessModifier access = AccessModifier::None; 
            if (match(TokenType::kwPublic, true)) access = AccessModifier::Public;
            else if (match(TokenType::kwPrivate, true)) access = AccessModifier::Private;
            else if (match(TokenType::kwProtected, true)) access = AccessModifier::Protected;

            if (match(TokenType::kwFn) ||
                match(TokenType::identifier, "OnConstruct", false) ||
                match(TokenType::identifier, "OnDestruct", false))
            {
                bool isLifecycle = !match(TokenType::kwFn);
                auto method = parseFunctionDeclaration(std::move(memberAttrs), isLifecycle);
                
                members.push_back(ObjectMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(method)
                });
            }
            else
            {
                NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consume(TokenType::identifier));
                
                NodePtr<TypeSpecifier> memberType = nullptr;
                if (match(TokenType::opColon, true)) memberType = parseType();
                
                NodePtr<Expression> init = nullptr;
                if (match(TokenType::opAssign, true)) init = parseExpression();

                if (!memberType && !init) {
                    utError("Object members must have an explicit type or an initializer.", peek(-1).loc);
                }

                match(TokenType::comma, true);
                match(TokenType::semicolon, true);

                auto varDecl =
                    makeNodePtr<VariableDeclaration>(
                        std::move(memberAttrs),
                        Mutability::Mutable,
                        std::move(memberName),
                        std::move(memberType),
                        std::move(init),
                        peek(-1).loc
                    );

                members.push_back(ObjectMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(varDecl)
                });
            }
        }
        consume(TokenType::rightBrace);
        return makeNodePtr<ObjectDeclaration>(std::move(attributes), std::move(name), std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseFlagDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwFlag);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));
        consume(TokenType::semicolon); // flag IsDead;
        
        return makeNodePtr<FlagDeclaration>(std::move(attributes), std::move(name), startTok.loc);
    }

    NodePtr<Statement> Parser::parseEnumDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwEnum);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));
        
        consume(TokenType::leftBrace);
        std::vector<EnumMember> members;
        
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consume(TokenType::identifier));
            NodePtr<Expression> value = nullptr;
            
            if (match(TokenType::opAssign, true))
                value = parseExpression();
            
            members.emplace_back(std::move(memberName), std::move(value));
            match(TokenType::comma, true);
        }
        consume(TokenType::rightBrace);
        
        return makeNodePtr<EnumDeclaration>(std::move(attributes), std::move(name), std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseFlagsetDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwFlagset);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));
        
        consume(TokenType::leftBrace);
        std::vector<EnumMember> members;
        
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consume(TokenType::identifier));
            NodePtr<Expression> value = nullptr;
            
            if (match(TokenType::opAssign, true))
                value = parseExpression();
            
            members.emplace_back(std::move(memberName), std::move(value));
            match(TokenType::comma, true);
        }
        consume(TokenType::rightBrace);
        
        return makeNodePtr<FlagsetDeclaration>(std::move(attributes), std::move(name), std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseIfStatement()
    {
        Token startTok = consume(TokenType::kwIf);
        Location startLoc = startTok.loc;
        
        bool hasParen = match(TokenType::leftParen, true);

        NodePtr<Expression> condition = parseExpression(0, true);

        Token matchVar = Token::invalid();
        if (match(TokenType::kwFit, true))
            matchVar = consume(TokenType::identifier);

        if (hasParen)
            consume(TokenType::rightParen);

        NodePtr<Statement> thenBranch = match(TokenType::leftBrace) ? parseBlockStatement() : parseStatement();
        NodePtr<Statement> elseBranch = nullptr;

        if (match(TokenType::kwElse, true))
        {
            if (match(TokenType::kwIf, true))
            {
                elseBranch = parseIfStatement();
            }
            else
            {
                elseBranch = parseBlockStatement();
            }
        }

        return makeNodePtr<IfStatement>(std::move(condition), std::move(thenBranch), std::move(elseBranch), std::move(matchVar), startLoc);
    }

    NodePtr<Statement> Parser::parseWhileStatement()
    {
        Token startTok = consume(TokenType::kwWhile);
        Location startLoc = startTok.loc;

        NodePtr<Expression> condition = parseExpression();
        NodePtr<Statement> body = match(TokenType::leftBrace) ? parseBlockStatement() : parseStatement();

        return makeNodePtr<WhileStatement>(std::move(condition), std::move(body), startLoc);
    }

    NodePtr<Statement> Parser::parseBreakStatement()
    {
        Token startTok = consume(TokenType::kwBreak);
        consume(TokenType::semicolon);
        return makeNodePtr<BreakStatement>(startTok.loc);
    }

    NodePtr<Statement> Parser::parseContinueStatement()
    {
        Token startTok = consume(TokenType::kwContinue);
        consume(TokenType::semicolon);
        return makeNodePtr<ContinueStatement>(startTok.loc);
    }

    NodePtr<Statement> Parser::parseReturnStatement()
    {
        Token startTok = consume(TokenType::kwReturn);
        Location startLoc = startTok.loc;

        NodePtr<Expression> value = nullptr;

        if (!match(TokenType::semicolon, true))
        {
            value = parseExpression();
            consume(TokenType::semicolon);
        }

        return makeNodePtr<ReturnStatement>(std::move(value), startLoc);
    }

    // TODO: Refactor
    NodePtr<Statement> Parser::parseUseStatement()
    {
        Token startTok = consume(TokenType::kwUse);
        Location startLoc = startTok.loc;

        std::vector<std::string> moduleParts;
        bool isStdLib = false;
        
        while (matchOneOf({TokenType::kwSuper, TokenType::kwSelf, TokenType::identifier}))
        {
            Token tok = advance();
            bool skipScope = false;
            if (tok.type == TokenType::kwSuper)
            {
                moduleParts.emplace_back("..");
            }
            else if (tok.type == TokenType::kwSelf)
            {
                skipScope = true;
            }
            else
            {
                if (tok.value == "std" && moduleParts.empty())
                {
                    skipScope = true;
                    isStdLib = true;
                }
                else
                {
                    moduleParts.push_back(tok.value);
                }
            }

            if (match(TokenType::opScope, true) && !skipScope)
            {
                moduleParts.emplace_back("/");
            }
        }

        if (moduleParts.empty())
            utError("Use statement must include a module path.", startLoc);

        if (moduleParts.back() == ".." || moduleParts.back() == "/")
            utError("Unfinished use statement. Use statements should finish with a module name.", peek(-1).loc);

        std::string moduleName = moduleParts.back();
        std::string modulePath;
        std::string aliasName;

        std::ranges::for_each(moduleParts, [&modulePath](const std::string& part)
        {
            modulePath.append(part);
        });

        if (match(TokenType::kwAs, true))
        {
            aliasName = consume(TokenType::identifier).value;
        }

        consume(TokenType::semicolon);
        
        return makeNodePtr<UseStatement>(std::move(moduleName), std::move(modulePath), std::move(aliasName), isStdLib, startLoc);
    }

    NodePtr<Statement> Parser::parseRealmDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        if (!attributes.empty())
            utError("Attributes are not supported on realm declarations yet.", attributes.front()->location());

        Token startTok = consume(TokenType::kwRealm);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consume(TokenType::identifier));

        consume(TokenType::leftBrace);

        std::vector<NodePtr<Statement>> statements;
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            if (NodePtr<Statement> statement = parseStatement(); statement)
                statements.emplace_back(std::move(statement));
        }

        consume(TokenType::rightBrace);
        return makeNodePtr<RealmDeclaration>(std::move(name), std::move(statements), startTok.loc);
    }

    // todo: improve
    int Parser::getPrecedence(TokenType type)
    {
        // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
        switch (type)
        {
        // ---------------------------------
        // fit
        // ---------------------------------
        case TokenType::kwFit:
            return 13;
            
        // ---------------------------------
        // Postfix / access / call
        // ---------------------------------
        case TokenType::opDot:
        case TokenType::opScope:
        case TokenType::leftParen:    // call
        case TokenType::leftBracket:  // index
            return 12;
    
        // ---------------------------------
        // Prefix (unary)
        // ---------------------------------
        case TokenType::kwRef:
        case TokenType::kwNot:        // not
        case TokenType::opLogicalNot: // !
        case TokenType::opBitNot:     // ~
            return 11;
    
        // ---------------------------------
        // Multiplicative
        // ---------------------------------
        case TokenType::opStar:
        case TokenType::opSlash:
        case TokenType::opPercent:
            return 10;
    
        // ---------------------------------
        // Additive
        // ---------------------------------
        case TokenType::opPlus:
        case TokenType::opMinus:
            return 9;
    
        // ---------------------------------
        // Shift
        // ---------------------------------
        case TokenType::opShiftLeft:
        case TokenType::opShiftRight:
            return 8;
            
        // ---------------------------------
        // Range
        // ---------------------------------
        case TokenType::opRangeInclusive:
        case TokenType::opRangeExclusive:
            return 7;
    
        // ---------------------------------
        // Relational
        // ---------------------------------
        case TokenType::opLess:
        case TokenType::opLessEqual:
        case TokenType::opGreater:
        case TokenType::opGreaterEqual:
        case TokenType::kwIn:
            return 6;
    
        // ---------------------------------
        // Equality
        // ---------------------------------
        case TokenType::opEqual:
        case TokenType::opNotEqual:
        case TokenType::kwIs:
            return 5;
    
        // ---------------------------------
        // Bitwise
        // ---------------------------------
        case TokenType::opBitAnd:
        case TokenType::opBitXor:
        case TokenType::opBitOr:
            return 4;

        // ---------------------------------
        // Logical AND
        // ---------------------------------
        case TokenType::opLogicalAnd:
        case TokenType::kwAnd:
            return 3;

        // ---------------------------------
        // Logical OR
        // ---------------------------------
        case TokenType::opLogicalOr:
        case TokenType::kwOr:
            return 2;    
    
        // ---------------------------------
        // Flow / pipe
        // ---------------------------------
        case TokenType::opFlowRight: // |>
        case TokenType::opFlowLeft:  // <|
            return 1;
    
        // ---------------------------------
        // Assignment (lowest, right-assoc)
        // ---------------------------------
        case TokenType::opAssign:
        case TokenType::opPlusAssign:
        case TokenType::opMinusAssign:
        case TokenType::opStarAssign:
        case TokenType::opSlashAssign:
        case TokenType::opPercentAssign:
        case TokenType::opShiftLeftAssign:
        case TokenType::opShiftRightAssign:
        case TokenType::opBitAndAssign:
        case TokenType::opBitOrAssign:
        case TokenType::opBitXorAssign:
        case TokenType::opBitNotAssign:
            return 0;
    
        // ---------------------------------
        // Expression boundaries
        // ---------------------------------
        case TokenType::semicolon:
        case TokenType::leftBrace:
        case TokenType::rightBrace:
        default:
            return -1;
        }
    }

    void Parser::utError(const std::string& message, Location location)
    {
        WIO_LOG_ADD_ERROR(location, message);
        throw UnexpectedTokenError(message.c_str(), location);
    }

    void Parser::ucError(Location location)
    {
        WIO_LOG_ADD_ERROR(location, "Constants must be initialized.");
        throw UninitializedConstantError("Constants must be initialized.", location);
    }
}
