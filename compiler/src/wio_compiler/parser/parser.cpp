#include "wio/parser/parser.h"

#include "wio/common/exception.h"
#include "wio/common/operator_overload.h"
#include "wio/common/utility.h"
#include "wio/common/logger.h"

#include "general/traits/integer_traits.h"
#include "general/traits/float_traits.h"

#include <algorithm>
#include <cstddef>
#include <functional>

namespace wio
{
    using namespace common;

    namespace
    {
        bool canStartAttributeTypeArgument(const Token& token)
        {
            return token.isIdentifier() ||
                   token.isType() ||
                   token.type == TokenType::integerLiteral ||
                   token.type == TokenType::stringLiteral ||
                   token.type == TokenType::kwRef ||
                   token.type == TokenType::kwView ||
                   token.type == TokenType::kwFn ||
                   token.type == TokenType::leftBracket;
        }

        bool isSignFoldableNumericLiteral(const Token& token)
        {
            return token.type == TokenType::integerLiteral || token.type == TokenType::floatLiteral;
        }

        NodePtr<Identifier> makeSyntheticIdentifier(std::string value, const Location location)
        {
            return makeNodePtr<Identifier>(Token{
                .type = TokenType::identifier,
                .value = std::move(value),
                .loc = location
            });
        }

        std::optional<Attribute> resolveAttributeName(std::string_view name)
        {
            if (auto legacy = frenum::cast<Attribute>(name); legacy.has_value())
                return legacy;

            if (name == "readonly") return Attribute::ReadOnly;
            if (name == "default") return Attribute::Default;
            if (name == "no_default_ctor") return Attribute::NoDefaultCtor;
            if (name == "generate_ctors") return Attribute::GenerateCtors;
            if (name == "from" || name == "conversion::from") return Attribute::From;
            if (name == "trust") return Attribute::Trust;
            if (name == "final") return Attribute::Final;
            if (name == "type" || name == "abi::type") return Attribute::Type;
            if (name == "native") return Attribute::Native;
            if (name == "cpp::header") return Attribute::CppHeader;
            if (name == "cpp::name") return Attribute::CppName;
            if (name == "instantiate") return Attribute::Instantiate;
            if (name == "specialize") return Attribute::Specialize;
            if (name == "apply") return Attribute::Apply;
            if (name == "export" || name == "export::c") return Attribute::Export;
            if (name == "command") return Attribute::Command;
            if (name == "event") return Attribute::Event;
            if (name == "module::api_version") return Attribute::ModuleApiVersion;
            if (name == "module::load") return Attribute::ModuleLoad;
            if (name == "module::update") return Attribute::ModuleUpdate;
            if (name == "module::unload") return Attribute::ModuleUnload;
            if (name == "module::save_state") return Attribute::ModuleSaveState;
            if (name == "module::restore_state") return Attribute::ModuleRestoreState;
            return std::nullopt;
        }
    }
    
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

        if (requiresAsyncModule_)
        {
            const bool alreadyImported = std::ranges::any_of(
                statements,
                [](const NodePtr<Statement>& statement)
                {
                    const auto* use = statement ? statement->as<UseStatement>() : nullptr;
                    return use && use->isStdLib && use->modulePath == "async";
                });
            if (!alreadyImported)
            {
                statements.insert(
                    statements.begin(),
                    makeNodePtr<UseStatement>(
                        "async", "async", "", true, false, false, Location::invalid()));
            }
        }

        return makeNodePtr<Program>(std::move(statements));
    }

    Token Parser::peek(int offset) const
    {
        using SignedIndex = std::ptrdiff_t;

        const SignedIndex baseIndex = static_cast<SignedIndex>(currentTokenIndex_);
        const SignedIndex candidateIndex = baseIndex + static_cast<SignedIndex>(offset);
        if (candidateIndex >= 0 &&
            candidateIndex < static_cast<SignedIndex>(tokens_.size()))
        {
            return tokens_[static_cast<size_t>(candidateIndex)];
        }

        return Token::invalid();
    }

    Token Parser::previous() const
    {
        return peek(-1);
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

    void Parser::consumeGenericClose()
    {
        if (match(TokenType::opGreater, true))
            return;

        const Token combined = peek();
        auto makeRemainder = [&](TokenType type, std::string value, uint64_t columnOffset)
        {
            Token token = combined;
            token.type = type;
            token.value = std::move(value);
            if (token.loc.column != 0)
                token.loc.column += columnOffset;
            return token;
        };

        if (combined.type == TokenType::opShiftRight)
        {
            advance();
            tokens_.insert(tokens_.begin() + static_cast<std::ptrdiff_t>(currentTokenIndex_),
                           makeRemainder(TokenType::opGreater, ">", 1));
            return;
        }

        if (combined.type == TokenType::opGreaterEqual)
        {
            advance();
            tokens_.insert(tokens_.begin() + static_cast<std::ptrdiff_t>(currentTokenIndex_),
                           makeRemainder(TokenType::opAssign, "=", 1));
            return;
        }

        if (combined.type == TokenType::opShiftRightAssign)
        {
            advance();
            const auto insertAt = tokens_.begin() + static_cast<std::ptrdiff_t>(currentTokenIndex_);
            tokens_.insert(insertAt, makeRemainder(TokenType::opGreater, ">", 1));
            tokens_.insert(tokens_.begin() + static_cast<std::ptrdiff_t>(currentTokenIndex_ + 1),
                           makeRemainder(TokenType::opAssign, "=", 2));
            return;
        }

        consume(TokenType::opGreater);
    }

    bool Parser::matchIdentifier(bool consume)
    {
        if (!peek().isIdentifier() && !peek().isKeyword())
            return false;

        if (consume)
            advance();
        return true;
    }

    Token Parser::consumeIdentifier()
    {
        if (!matchIdentifier())
            return consume(TokenType::identifier);

        Token token = advance();
        token.type = TokenType::identifier;
        return token;
    }

    Location Parser::previousLocation() const
    {
        return previous().loc;
    }

    Location Parser::currentOrPreviousLocation() const
    {
        const Token current = peek();
        if (current.loc.isValid())
            return current.loc;

        return previousLocation();
    }

    void Parser::expectElementAfterComma(TokenType closingType, std::string_view elementDescription)
    {
        if (!match(closingType))
            return;

        utError(formatString("Expected {} after ','.", elementDescription), currentOrPreviousLocation());
    }

    void Parser::synchronize()
    {
        advance();

        while (peek().isValid())
        {
            if (previous().type == TokenType::semicolon) return;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (peek().type)
            {
            case TokenType::kwFn:
            case TokenType::kwLet:
            case TokenType::kwMut:
            case TokenType::kwConst:
            case TokenType::kwType:
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
            case TokenType::kwForeach:
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
            
            if (op.type == TokenType::kwSpawn)
            {
                if (asyncScopeNames_.empty())
                    utError("'spawn' requires an enclosing 'async scope'.", op.loc);

                std::string executor;
                if (peek().type == TokenType::identifier &&
                    (peek().value == "worker" || peek().value == "blocking"))
                {
                    executor = advance().value;
                }

                const int precedence = getPrecedence(op.type);
                NodePtr<Expression> operand = parseExpression(precedence + 1, stopAtFit);
                std::string spawnMethod = "Spawn";
                if (!executor.empty())
                {
                    auto lambdaBody = makeNodePtr<ExpressionStatement>(std::move(operand), op.loc);
                    operand = makeNodePtr<LambdaExpression>(
                        std::vector<Parameter>{}, nullptr, std::move(lambdaBody), op.loc);
                    spawnMethod = executor == "worker" ? "SpawnWorker" : "SpawnBlocking";
                }
                auto scopeAccess = makeNodePtr<MemberAccessExpression>(
                    makeSyntheticIdentifier(asyncScopeNames_.back(), op.loc),
                    makeSyntheticIdentifier(std::move(spawnMethod), op.loc),
                    TokenType::opDot,
                    op.loc);
                std::vector<NodePtr<Expression>> arguments;
                arguments.push_back(std::move(operand));
                left = makeNodePtr<FunctionCallExpression>(
                    std::move(scopeAccess),
                    std::vector<NodePtr<TypeSpecifier>>{},
                    std::move(arguments),
                    false,
                    false,
                    op.loc);
            }
            else if (op.type == TokenType::kwDetach)
            {
                const int precedence = getPrecedence(op.type);
                NodePtr<Expression> operand = parseExpression(precedence + 1, stopAtFit);
                auto detachAccess = makeNodePtr<MemberAccessExpression>(
                    std::move(operand),
                    makeSyntheticIdentifier("Detach", op.loc),
                    TokenType::opDot,
                    op.loc);
                left = makeNodePtr<FunctionCallExpression>(
                    std::move(detachAccess),
                    std::vector<NodePtr<TypeSpecifier>>{},
                    std::vector<NodePtr<Expression>>{},
                    false,
                    false,
                    op.loc);
            }
            else if (op.type == TokenType::kwRef)
            {
                NodePtr<Expression> operand = parseExpression(getPrecedence(TokenType::kwRef) + 1, true);
                left = makeNodePtr<RefExpression>(false, std::move(operand), op.loc);
            }
            else if (op.type == TokenType::opMinus && isSignFoldableNumericLiteral(peek()))
            {
                Token literal = advance();
                literal.value = "-" + literal.value;

                if (literal.type == TokenType::integerLiteral)
                    left = makeNodePtr<IntegerLiteral>(std::move(literal));
                else
                    left = makeNodePtr<FloatLiteral>(std::move(literal));
            }
            else
            {
                int precedence = getPrecedence(op.type);
                NodePtr<Expression> operand = parseExpression(precedence + 1, stopAtFit);
                bool isMainExecutorAwait = false;
                if (op.type == TokenType::kwAwait)
                {
                    auto identifier = operand ? operand->as<Identifier>() : nullptr;
                    if (identifier && identifier->token.value == "main")
                    {
                        requiresAsyncModule_ = true;
                        isMainExecutorAwait = true;
                    }
                }
                auto unary = makeNodePtr<UnaryExpression>(std::move(op), std::move(operand));
                unary->isMainExecutorAwait = isMainExecutorAwait;
                left = std::move(unary);
            }
        }
        else
        {
            left = parsePrimary();
        }

        auto parseCallArguments = [&]() -> std::vector<NodePtr<Expression>>
        {
            consume(TokenType::leftParen);
            std::vector<NodePtr<Expression>> args;

            if (!match(TokenType::rightParen))
            {
                args.push_back(parseExpression());
                while (match(TokenType::comma, true))
                {
                    expectElementAfterComma(TokenType::rightParen, "function call argument");
                    args.push_back(parseExpression());
                }
            }

            consume(TokenType::rightParen);
            return args;
        };

        while (true)
        {
            if (stopAtFit && peek().type == TokenType::kwFit)
                break;

            if (peek().type == TokenType::opRangeInclusive &&
                (peek(1).type == TokenType::comma ||
                 peek(1).type == TokenType::rightParen ||
                 peek(1).type == TokenType::rightBracket ||
                 peek(1).type == TokenType::semicolon ||
                 peek(1).type == TokenType::rightBrace))
            {
                const Token ellipsisToken = advance();
                left = makeNodePtr<PackExpansionExpression>(std::move(left), ellipsisToken.loc);
                continue;
            }

            if (match(TokenType::opQuestion) && peek(1).type == TokenType::leftParen)
            {
                advance();
                auto args = parseCallArguments();
                left = makeNodePtr<FunctionCallExpression>(
                    std::move(left),
                    std::vector<NodePtr<TypeSpecifier>>{},
                    std::move(args),
                    false,
                    true
                );
                continue;
            }

            if (match(TokenType::opQuestion) && peek(1).type != TokenType::leftParen)
            {
                constexpr int conditionalPrecedence = 1;
                if (conditionalPrecedence < minPrecedence)
                    break;

                const Token question = advance();
                NodePtr<Expression> whenTrue = parseExpression();
                consume(TokenType::opColon);
                NodePtr<Expression> whenFalse = parseExpression(conditionalPrecedence);
                left = makeNodePtr<ConditionalExpression>(
                    std::move(left),
                    std::move(whenTrue),
                    std::move(whenFalse),
                    question.loc
                );
                continue;
            }

            // An explicit generic call is postfix syntax even though it begins
            // with '<'. Resolve it before binary precedence so prefix forms
            // such as `await Load<T>()` and `spawn Load<T>()` keep the call
            // attached to their operand.
            if (peek().type == TokenType::opLess &&
                (left->is<Identifier>() || left->is<MemberAccessExpression>()) &&
                canParseExplicitTypeArgumentCall())
            {
                std::vector<NodePtr<TypeSpecifier>> explicitTypeArguments = parseExplicitTypeArgumentList();
                const bool unwrapResult = match(TokenType::opLogicalNot, true);
                const bool propagateResult = !unwrapResult && match(TokenType::opQuestion, true);
                auto args = parseCallArguments();
                left = makeNodePtr<FunctionCallExpression>(
                    std::move(left),
                    std::move(explicitTypeArguments),
                    std::move(args),
                    unwrapResult,
                    propagateResult
                );
                continue;
            }

            int precedence = getPrecedence(peek().type);
            if (precedence < minPrecedence)
                break;

            if (peek().type == TokenType::opGreater && peek(1).type == TokenType::rightBrace)
                break;

            if (match(TokenType::kwIs))
            {
                Token op = advance();
                auto targetType = parseType();
                auto right = makeNodePtr<TypeExpression>(std::move(targetType), op.loc);
                left = makeNodePtr<BinaryExpression>(std::move(left), std::move(op), std::move(right), op.loc);
                continue;
            }

            if (match(TokenType::opLogicalNot) && peek(1).type == TokenType::leftParen)
            {
                advance();
                auto args = parseCallArguments();
                left = makeNodePtr<FunctionCallExpression>(
                    std::move(left),
                    std::vector<NodePtr<TypeSpecifier>>{},
                    std::move(args),
                    true,
                    false
                );
                continue;
            }

            if (match(TokenType::leftParen))
            {
                auto args = parseCallArguments();
                left = makeNodePtr<FunctionCallExpression>(
                    std::move(left),
                    std::vector<NodePtr<TypeSpecifier>>{},
                    std::move(args),
                    false,
                    false
                );
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
                NodePtr<Identifier> member = makeNodePtr<Identifier>(consumeIdentifier());
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
            if (match(TokenType::kwFit))
            {
                const Token fitToken = advance();
                auto targetType = parseType(); 
            
                left = makeNodePtr<FitExpression>(std::move(left), std::move(targetType), fitToken.loc);
                continue;
            }
            
            Token op = advance();
            // Right-flow is left associative (`x |> f |> g`), while left-flow
            // follows ordinary functional application and is right
            // associative (`f <| g <| x` == `f(g(x))`).
            const int rightMinPrecedence = op.type == TokenType::opFlowLeft
                ? precedence
                : precedence + 1;
            NodePtr<Expression> right = parseExpression(rightMinPrecedence, stopAtFit);

            if (op.type == TokenType::opRangeInclusive || op.type == TokenType::opRangeExclusive)
            {
                bool isInclusive = (op.type == TokenType::opRangeInclusive);
                left = makeNodePtr<RangeExpression>(std::move(left), std::move(right), isInclusive, op.loc);
            }
            else if (op.type == TokenType::opFlowRight)
            {
                if (auto call = right.As<FunctionCallExpression>())
                {
                    call->arguments.insert(call->arguments.begin(), std::move(left));
                    call->isPipelineCall = true;
                    left = std::move(right);
                }
                else
                {
                    std::vector<NodePtr<Expression>> arguments;
                    arguments.push_back(std::move(left));
                    left = makeNodePtr<FunctionCallExpression>(
                        std::move(right), std::vector<NodePtr<TypeSpecifier>>{},
                        std::move(arguments), false, false, op.loc);
                    left.AsFast<FunctionCallExpression>()->isPipelineCall = true;
                }
            }
            else if (op.type == TokenType::opFlowLeft)
            {
                if (auto call = left.As<FunctionCallExpression>())
                {
                    call->arguments.push_back(std::move(right));
                    call->isPipelineCall = true;
                }
                else
                {
                    std::vector<NodePtr<Expression>> arguments;
                    arguments.push_back(std::move(right));
                    left = makeNodePtr<FunctionCallExpression>(
                        std::move(left), std::vector<NodePtr<TypeSpecifier>>{},
                        std::move(arguments), false, false, op.loc);
                    left.AsFast<FunctionCallExpression>()->isPipelineCall = true;
                }
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
        if (match(TokenType::integerLiteral))
            return makeNodePtr<IntegerLiteral>(advance());

        if (match(TokenType::floatLiteral))
            return makeNodePtr<FloatLiteral>(advance());

        if (match(TokenType::stringLiteral))
            return parseStringLiteral();

        if (match(TokenType::identifier) || peek().isType())
            return makeNodePtr<Identifier>(advance());

        if (match(TokenType::charLiteral))
            return makeNodePtr<CharLiteral>(advance());

        if (matchOneOf({ TokenType::kwTrue , TokenType::kwFalse }))
            return makeNodePtr<BoolLiteral>(advance());
        
        if (match(TokenType::kwNull))
            return makeNodePtr<NullExpression>(advance().loc);

        if (match(TokenType::kwSelf))
            return makeNodePtr<SelfExpression>(advance().loc);
            
        if (match(TokenType::kwSuper))
            return makeNodePtr<SuperExpression>(advance().loc);

        if (match(TokenType::durationLiteral))
            return makeNodePtr<DurationLiteral>(advance());
        
        if (match(TokenType::byteLiteral))
            return makeNodePtr<ByteLiteral>(advance());

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
            const Token refToken = advance();

            bool isMut = match(TokenType::kwMut, true);
            
            NodePtr<Expression> operand = parseExpression(getPrecedence(TokenType::kwRef) + 1, true); 
            
            return makeNodePtr<RefExpression>(isMut, std::move(operand), refToken.loc);
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

        // At this point all grammatical keyword forms have already had their
        // chance. Treat a remaining keyword token as an API identifier so
        // keyword-shaped functions and realms remain callable.
        if (peek().isKeyword())
            return makeNodePtr<Identifier>(consumeIdentifier());

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

        return makeNodePtr<InterpolatedStringLiteral>(
            std::move(parts), startTok.isUnicodeString, startTok.loc);
    }

    std::vector<NodePtr<TypeSpecifier>> Parser::parseExplicitTypeArgumentList()
    {
        consume(TokenType::opLess);

        std::vector<NodePtr<TypeSpecifier>> typeArguments;
        if (!match(TokenType::opGreater))
        {
            typeArguments.push_back(parseGenericArgument());
            while (match(TokenType::comma, true))
            {
                expectElementAfterComma(TokenType::opGreater, "explicit type argument");
                typeArguments.push_back(parseGenericArgument());
            }
        }

        consumeGenericClose();
        return typeArguments;
    }

    NodePtr<TypeSpecifier> Parser::parseGenericArgument()
    {
        if (match(TokenType::integerLiteral) || match(TokenType::stringLiteral))
        {
            Token value = advance();
            const auto location = value.loc;
            return makeNodePtr<TypeSpecifier>(
                std::move(value), std::vector<NodePtr<TypeSpecifier>>{}, nullptr,
                0, false, false, false, location);
        }

        return parseType();
    }

    NodePtr<Expression> Parser::parseArrayLiteral()
    {
        Location startLoc = peek().loc;
        consume(TokenType::leftBracket); // [

        std::vector<NodePtr<Expression>> elements;
    
        if (!match(TokenType::rightBracket))
        {
            elements.push_back(parseExpression());
            while (match(TokenType::comma, true))
            {
                expectElementAfterComma(TokenType::rightBracket, "array element");
                elements.push_back(parseExpression());
            }
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
            while (true)
            {
                if (isOrdered && peek().type == TokenType::opGreater) break;
                if (!isOrdered && peek().type == TokenType::rightBrace) break;
                
                NodePtr<Expression> key = parseExpression();
                consume(TokenType::opColon);
                NodePtr<Expression> value = parseExpression();

                pairs.emplace_back(std::move(key), std::move(value));
                if (!match(TokenType::comma, true))
                    break;

                if ((isOrdered && match(TokenType::opGreater)) || (!isOrdered && match(TokenType::rightBrace)))
                    utError("Expected dictionary entry after ','.", currentOrPreviousLocation());
            }
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
            parameters.emplace_back(
                makeNodePtr<Identifier>(consumeIdentifier()),
                nullptr
            );
            if (match(TokenType::opColon, true))
            {
                parameters.back().type = parseType();
            }

            while (match(TokenType::comma, true))
            {
                expectElementAfterComma(TokenType::rightParen, "lambda parameter");

                NodePtr<Identifier> paramName = makeNodePtr<Identifier>(consumeIdentifier());
                NodePtr<TypeSpecifier> paramType = nullptr;

                if (match(TokenType::opColon, true))
                    paramType = parseType();

                parameters.emplace_back(std::move(paramName), std::move(paramType));
            }
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
            body = makeNodePtr<ExpressionStatement>(std::move(expr));
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
            std::string variantName;
            std::vector<NodePtr<Identifier>> bindings;

            if (match(TokenType::kwAssumed, true))
            {
            }
            else if (peek().type == TokenType::identifier &&
                     peek(1).type == TokenType::leftParen &&
                     (peek().value == "Some" || peek().value == "None" ||
                      peek().value == "Ok" || peek().value == "Err"))
            {
                variantName = advance().value;
                consume(TokenType::leftParen);
                if (!match(TokenType::rightParen))
                {
                    bindings.push_back(makeNodePtr<Identifier>(consumeIdentifier()));
                    while (match(TokenType::comma, true))
                        bindings.push_back(makeNodePtr<Identifier>(consumeIdentifier()));
                }
                consume(TokenType::rightParen);
            }
            else if (match(TokenType::leftBracket, true))
            {
                variantName = "__array";
                if (!match(TokenType::rightBracket))
                {
                    bindings.push_back(makeNodePtr<Identifier>(consumeIdentifier()));
                    while (match(TokenType::comma, true))
                        bindings.push_back(makeNodePtr<Identifier>(consumeIdentifier()));
                }
                consume(TokenType::rightBracket);
            }
            else
            {
                do
                {
                    matchValues.push_back(parseExpression(3));
                }
                while (match(TokenType::comma, true) || match(TokenType::kwOr, true));
            }

            NodePtr<Expression> guard = nullptr;
            if (match(TokenType::kwIf, true))
                guard = parseExpression();

            consume(TokenType::opColon);
            
            NodePtr<Statement> body = parseStatement(); 
            
            cases.push_back(MatchCase{
                .matchValues = std::move(matchValues),
                .body = std::move(body),
                .variantName = std::move(variantName),
                .bindings = std::move(bindings),
                .guard = std::move(guard)
            });
        }
        consume(TokenType::rightBrace);

        return makeNodePtr<MatchExpression>(std::move(value), std::move(cases), startTok.loc);
    }

    Token Parser::parseAttributeArgumentToken()
    {
        Token arg = advance();

        if (!arg.isValid())
            utError("Expected attribute argument.", currentOrPreviousLocation());

        if (arg.type != TokenType::identifier || !match(TokenType::opScope))
            return arg;

        while (match(TokenType::opScope, true))
        {
            Token nextSegment = consumeIdentifier();
            arg.value += "::" + nextSegment.value;
            arg.type = TokenType::identifier;
        }

        return arg;
    }

    NodePtr<TypeSpecifier> Parser::parseType()
    {
        auto finishType = [&](NodePtr<TypeSpecifier> type) -> NodePtr<TypeSpecifier>
        {
            while (true)
            {
                if (match(TokenType::opQuestion, true))
                {
                    if (type->isNullable)
                        utError("A type can only have one nullable suffix '?'.", previous().loc);
                    type->isNullable = true;
                    continue;
                }

                if (match(TokenType::leftBracket, true))
                {
                    const Token leftBracketToken = previous();
                    consume(TokenType::rightBracket);

                    Token arrayToken {
                        .type = TokenType::DynamicArray,
                        .value = "",
                        .loc = leftBracketToken.loc
                    };
                    std::vector<NodePtr<TypeSpecifier>> args;
                    args.push_back(std::move(type));
                    type = makeNodePtr<TypeSpecifier>(
                        std::move(arrayToken),
                        std::move(args),
                        nullptr,
                        0,
                        false,
                        false,
                        false,
                        leftBracketToken.loc
                    );
                    continue;
                }

                if (match(TokenType::opRangeInclusive, true))
                {
                    type->isPackExpansion = true;
                    continue;
                }

                break;
            }
            return type;
        };

        if (match(TokenType::leftParen, true))
        {
            auto groupedType = parseType();
            consume(TokenType::rightParen);
            return finishType(std::move(groupedType));
        }

        if (match(TokenType::kwRef))
        {
            const Token refToken = advance();
            auto innerType = parseType();
            
            Token token { .type = TokenType::kwRef, .value = "ref", .loc = refToken.loc };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(innerType));
            
            auto result = makeNodePtr<TypeSpecifier>(std::move(token), std::move(generics), nullptr, 0, true, true, false, refToken.loc);
            return finishType(std::move(result));
        }
        if (match(TokenType::kwView))
        {
            const Token viewToken = advance();
            auto innerType = parseType();
            
            Token token { .type = TokenType::kwView, .value = "view", .loc = viewToken.loc };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(innerType));
            
            auto result = makeNodePtr<TypeSpecifier>(std::move(token), std::move(generics), nullptr, 0, true, false, false, viewToken.loc);
            return finishType(std::move(result));
        }

        if (match(TokenType::leftBracket))
        {
            const Token leftBracketToken = advance();
            
            auto innerType = parseType();

            size_t size = 0;
            NodePtr<TypeSpecifier> arrayExtent = nullptr;
            bool hasInferredArrayExtent = false;
            if (match(TokenType::semicolon, true))
            {
                if (match(TokenType::rightBracket))
                {
                    utError("Static array types must declare a size after ';'.", currentOrPreviousLocation());
                }
                else if (match(TokenType::integerLiteral))
                {
                    const Token sizeToken = advance();
                    size = traits::IntegerTraits<size_t>::IntegerResultCastedAs(getInteger(sizeToken.value));
                }
                else if (match(TokenType::identifier))
                {
                    if (peek().value == "_")
                    {
                        advance();
                        hasInferredArrayExtent = true;
                    }
                    else
                    {
                        arrayExtent = parseGenericArgument();
                    }
                }
                else
                {
                    utError("Static array extents must be a non-negative integer literal or const generic parameter.", currentOrPreviousLocation());
                }
            }
            
            consume(TokenType::rightBracket);

            Token arrayToken {
                .type = TokenType::StaticArray,
                .value ="",
                .loc = leftBracketToken.loc
            };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(innerType));

            auto arrayType = makeNodePtr<TypeSpecifier>(arrayToken, std::move(generics), nullptr, size, false, false, false, leftBracketToken.loc);
            arrayType->arrayExtent = std::move(arrayExtent);
            arrayType->hasInferredArrayExtent = hasInferredArrayExtent;
            return finishType(std::move(arrayType));
        }

        if (match(TokenType::kwFn))
        {
            const Token fnToken = advance();
            consume(TokenType::leftParen);
            
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.emplace_back(nullptr);

            if (!match(TokenType::rightParen))
            {
                generics.push_back(parseType());
                while (match(TokenType::comma, true))
                {
                    expectElementAfterComma(TokenType::rightParen, "function type parameter");
                    generics.push_back(parseType());
                }
            }
            const Token rightParenToken = consume(TokenType::rightParen);

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
                    .loc = rightParenToken.loc
                };
                retType = makeNodePtr<TypeSpecifier>(std::move(voidTok), std::vector<NodePtr<TypeSpecifier>>{}, nullptr, 0, false, false, false, rightParenToken.loc);
            }
            
            generics[0] = std::move(retType);

            Token fnTok {
                .type = TokenType::kwFn,
                .value = "fn",
                .loc = fnToken.loc
            };
            return finishType(makeNodePtr<TypeSpecifier>(std::move(fnTok), std::move(generics), nullptr, 0, false, false, false, fnToken.loc));
        }

        match(TokenType::kwConst, true);

        Token typeName = Token::invalid();

        if (peek().isType() || peek().isIdentifier())
            typeName = advance();
        else
            utError("Expected type name.", peek().loc);

        while (typeName.type == TokenType::identifier && match(TokenType::opScope, true))
        {
            Token nextSegment = consumeIdentifier();
            typeName.value += "::" + nextSegment.value;
        }
        
        Location startLoc = typeName.loc;
        std::vector<NodePtr<TypeSpecifier>> generics;

        if (match(TokenType::opLess, true))
        {
            generics.push_back(parseGenericArgument());
            while (match(TokenType::comma, true))
            {
                expectElementAfterComma(TokenType::opGreater, "generic type argument");
                generics.push_back(parseGenericArgument());
            }

            consumeGenericClose();
        }

        NodePtr<Expression> packIndex = nullptr;
        if (match(TokenType::leftBracket, false) && peek(1).type != TokenType::rightBracket)
        {
            consume(TokenType::leftBracket);
            packIndex = parseExpression();
            consume(TokenType::rightBracket);
        }

        auto result = makeNodePtr<TypeSpecifier>(std::move(typeName), std::move(generics), std::move(packIndex), 0, false, false, false, startLoc);

        return finishType(std::move(result));
    }

    NodePtr<Statement> Parser::parseStatement()
    {
        if (matchOneOf({ TokenType::semicolon, TokenType::endOfFile }, true))
            return nullptr;

        std::vector<NodePtr<AttributeStatement>> attributes;
        parseLeadingAttributes(attributes, bracketAttributeListPrecedesDeclaration());
        
        if (peek().isKeyword())
        {
            if (matchOneOf({ TokenType::kwLet, TokenType::kwMut, TokenType::kwConst }))
                return parseVariableDeclaration(std::move(attributes));
            if (match(TokenType::kwType))
                return parseTypeAliasDeclaration(std::move(attributes));
            if (match(TokenType::kwAttribute))
                return parseAttributeDeclaration(std::move(attributes));
            if (match(TokenType::kwApplication))
            {
                if (!attributes.empty())
                    utError("Application attributes must use the postfix 'with' clause.", attributes.front()->location());
                return parseApplicationDeclaration();
            }
            if (match(TokenType::kwSystem))
            {
                if (!attributes.empty())
                    utError("System attributes must use the postfix 'with' clause.", attributes.front()->location());
                return parseSystemDeclaration();
            }
            if (match(TokenType::kwFn))
                return parseFunctionDeclaration(std::move(attributes));
            if (match(TokenType::kwAsync))
            {
                if (peek(1).type == TokenType::identifier && peek(1).value == "scope")
                {
                    if (!attributes.empty())
                        utError("Async scopes cannot currently carry attributes.", attributes.front()->location());
                    return parseAsyncScopeStatement();
                }
                if (peek(1).type != TokenType::kwFn)
                    utError("Expected 'fn' or 'scope' after 'async'.", peek().loc);
                return parseFunctionDeclaration(std::move(attributes), false, false, true);
            }
            if (match(TokenType::kwInterface))
                return parseInterfaceDeclaration(std::move(attributes));
            if (match(TokenType::kwComponent))
                return parseComponentDeclaration(std::move(attributes));
            if (match(TokenType::kwExtension))
                return parseExtensionDeclaration(std::move(attributes));
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
            if (matchOneOf({ TokenType::kwFor, TokenType::kwForeach }))
                return parseForInStatement();
            if (match(TokenType::kwBreak))
                return parseBreakStatement();
            if (match(TokenType::kwContinue))
                return parseContinueStatement();
            if (match(TokenType::kwReturn))
                return parseReturnStatement();
            if (match(TokenType::kwUse))
                return parseUseStatement();
            if (match(TokenType::kwUsing))
                return parseUsingStatement();
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

    NodePtr<Statement> Parser::parseAsyncScopeStatement()
    {
        const Token asyncToken = consume(TokenType::kwAsync);
        requiresAsyncModule_ = true;
        const Token scopeToken = consumeIdentifier();
        if (scopeToken.value != "scope")
            utError("Expected 'scope' after 'async'.", scopeToken.loc);

        NodePtr<Expression> deadline;
        if (match(TokenType::kwWith, true))
        {
            const Token deadlineToken = consumeIdentifier();
            if (deadlineToken.value != "deadline")
                utError("Expected 'deadline(...)' after 'async scope with'.", deadlineToken.loc);
            consume(TokenType::leftParen);
            deadline = parseExpression();
            consume(TokenType::rightParen);
        }

        const std::string scopeName = "__wio_async_scope_" + std::to_string(asyncScopeCounter_++);
        asyncScopeNames_.push_back(scopeName);

        NodePtr<Statement> bodyStatement;
        try
        {
            bodyStatement = parseBlockStatement();
        }
        catch (...)
        {
            asyncScopeNames_.pop_back();
            throw;
        }
        asyncScopeNames_.pop_back();

        auto body = bodyStatement.As<BlockStatement>();
        if (!body)
            utError("Expected a block after 'async scope'.", asyncToken.loc);

        auto makeAwaitJoinStatement = [&](const Location location) -> NodePtr<Statement>
        {
            auto joinAccess = makeNodePtr<MemberAccessExpression>(
                makeSyntheticIdentifier(scopeName, location),
                makeSyntheticIdentifier("Join", location),
                TokenType::opDot,
                location);
            auto joinCall = makeNodePtr<FunctionCallExpression>(
                std::move(joinAccess),
                std::vector<NodePtr<TypeSpecifier>>{},
                std::vector<NodePtr<Expression>>{},
                false,
                false,
                location);
            Token awaitToken{
                .type = TokenType::kwAwait,
                .value = "await",
                .loc = location
            };
            auto awaitJoin = makeNodePtr<UnaryExpression>(
                std::move(awaitToken), std::move(joinCall), UnaryExpression::UnaryOperatorType::Prefix, location);
            return makeNodePtr<ExpressionStatement>(std::move(awaitJoin), location);
        };

        size_t returnCounter = 0;
        std::function<void(NodePtr<Statement>&)> rewriteReturns = [&](NodePtr<Statement>& statement)
        {
            if (!statement)
                return;
            if (auto returnStatement = statement.As<ReturnStatement>())
            {
                const Location returnLocation = returnStatement->location();
                std::vector<NodePtr<Statement>> replacement;
                NodePtr<Expression> returnValue;
                if (returnStatement->value)
                {
                    const std::string returnName = scopeName + "_return_" + std::to_string(returnCounter++);
                    replacement.push_back(makeNodePtr<VariableDeclaration>(
                        std::vector<NodePtr<AttributeStatement>>{},
                        Mutability::Immutable,
                        makeSyntheticIdentifier(returnName, returnLocation),
                        nullptr,
                        std::move(returnStatement->value),
                        false,
                        returnLocation));
                    returnValue = makeSyntheticIdentifier(returnName, returnLocation);
                }
                replacement.push_back(makeAwaitJoinStatement(returnLocation));
                replacement.push_back(makeNodePtr<ReturnStatement>(std::move(returnValue), returnLocation));
                statement = makeNodePtr<BlockStatement>(std::move(replacement), returnLocation);
                return;
            }
            if (auto block = statement.As<BlockStatement>())
            {
                for (auto& child : block->statements)
                    rewriteReturns(child);
                return;
            }
            if (auto conditional = statement.As<IfStatement>())
            {
                rewriteReturns(conditional->thenBranch);
                rewriteReturns(conditional->elseBranch);
                return;
            }
            if (auto loop = statement.As<WhileStatement>())
            {
                rewriteReturns(loop->body);
                return;
            }
            if (auto loop = statement.As<ForInStatement>())
            {
                rewriteReturns(loop->body);
                return;
            }
            if (auto loop = statement.As<CForStatement>())
                rewriteReturns(loop->body);
        };
        for (auto& statement : body->statements)
            rewriteReturns(statement);

        auto stdNamespace = makeSyntheticIdentifier("std", asyncToken.loc);
        auto asyncNamespace = makeNodePtr<MemberAccessExpression>(
            std::move(stdNamespace), makeSyntheticIdentifier("async", asyncToken.loc), TokenType::opScope, asyncToken.loc);
        auto scopeConstructor = makeNodePtr<MemberAccessExpression>(
            std::move(asyncNamespace), makeSyntheticIdentifier("Scope", asyncToken.loc), TokenType::opScope, asyncToken.loc);
        std::vector<NodePtr<Expression>> scopeArguments;
        if (deadline)
            scopeArguments.push_back(std::move(deadline));
        auto constructScope = makeNodePtr<FunctionCallExpression>(
            std::move(scopeConstructor),
            std::vector<NodePtr<TypeSpecifier>>{},
            std::move(scopeArguments),
            false,
            false,
            asyncToken.loc);
        auto scopeDeclaration = makeNodePtr<VariableDeclaration>(
            std::vector<NodePtr<AttributeStatement>>{},
            Mutability::Immutable,
            makeSyntheticIdentifier(scopeName, asyncToken.loc),
            nullptr,
            std::move(constructScope),
            false,
            asyncToken.loc);
        body->statements.insert(body->statements.begin(), std::move(scopeDeclaration));

        body->statements.push_back(makeAwaitJoinStatement(asyncToken.loc));
        return bodyStatement;
    }

    NodePtr<AttributeStatement> Parser::parseAttributeStatement(bool legacyAtSyntax)
    {
        Token startTok = legacyAtSyntax ? consume(TokenType::atSign) : peek();
        Location startLoc = startTok.loc;

        Token id = consumeIdentifier();
        std::string qualifiedName = id.value;
        while (match(TokenType::opScope, true))
        {
            qualifiedName += "::";
            qualifiedName += consumeIdentifier().value;
        }

        std::vector<Token> args;
        std::vector<NodePtr<TypeSpecifier>> typeArgs;
        std::vector<std::string> argumentNames;
        if (match(TokenType::leftParen, true))
        {
            if (!match(TokenType::rightParen))
            {
                bool sawNamedArgument = false;
                while (true)
                {
                    std::string argumentName;
                    if (matchIdentifier() && peek(1).type == TokenType::opColon)
                    {
                        argumentName = advance().value;
                        consume(TokenType::opColon);
                        sawNamedArgument = true;
                    }
                    else if (sawNamedArgument)
                    {
                        utError("Positional attribute arguments cannot follow named arguments.", peek().loc);
                    }

                    const bool standaloneTargetKeyword = peek().type == TokenType::kwFn &&
                        peek(1).type != TokenType::leftParen;
                    if (canStartAttributeTypeArgument(peek()) && !standaloneTargetKeyword)
                    {
                        const size_t typeStartIndex = currentTokenIndex_;
                        auto parsedType = parseGenericArgument();
                        const size_t typeEndIndex = currentTokenIndex_;

                        std::string rawArgument;
                        for (size_t tokenIndex = typeStartIndex; tokenIndex < typeEndIndex; ++tokenIndex)
                            rawArgument += tokens_[tokenIndex].value;

                        Token rawToken = Token::invalid();
                        if (typeStartIndex < tokens_.size())
                        {
                            rawToken = tokens_[typeStartIndex];
                            rawToken.value = std::move(rawArgument);
                        }

                        args.push_back(std::move(rawToken));
                        typeArgs.push_back(std::move(parsedType));
                    }
                    else
                    {
                        args.push_back(parseAttributeArgumentToken());
                        typeArgs.emplace_back(nullptr);
                    }
                    argumentNames.push_back(std::move(argumentName));

                    if (!match(TokenType::comma, true))
                        break;

                    expectElementAfterComma(TokenType::rightParen, "attribute argument");
                }
            }
            consume(TokenType::rightParen);
        }

        if (std::optional<Attribute> attribute = resolveAttributeName(qualifiedName); attribute.has_value())
        {
            if (std::ranges::any_of(argumentNames, [](const std::string& name) { return !name.empty(); }))
                utError("Named arguments are currently supported only for user-defined attributes.", startLoc);
            return makeNodePtr<AttributeStatement>(attribute.value(), args, typeArgs, startLoc, qualifiedName, argumentNames);
        }
        return makeNodePtr<AttributeStatement>(Attribute::Unknown, args, typeArgs, startLoc, qualifiedName, argumentNames);
    }

    void Parser::parseBracketAttributeList(std::vector<NodePtr<AttributeStatement>>& attributes)
    {
        const Token open = consume(TokenType::leftBracket);
        if (match(TokenType::rightBracket))
            utError("An attribute list cannot be empty.", open.loc);

        while (true)
        {
            if (!matchIdentifier())
                utError("Expected an attribute name in attribute list.", peek().loc);

            attributes.push_back(parseAttributeStatement(false));
            if (!match(TokenType::comma, true))
                break;
            if (match(TokenType::rightBracket))
                utError("Expected an attribute after ','.", peek().loc);
        }

        consume(TokenType::rightBracket);
    }

    void Parser::parseLeadingAttributes(std::vector<NodePtr<AttributeStatement>>& attributes,
                                        bool acceptBracketSyntax)
    {
        while (true)
        {
            if (match(TokenType::atSign))
            {
                attributes.push_back(parseAttributeStatement());
                continue;
            }
            if (acceptBracketSyntax && match(TokenType::leftBracket))
            {
                parseBracketAttributeList(attributes);
                continue;
            }
            break;
        }
    }

    bool Parser::bracketAttributeListPrecedesDeclaration() const
    {
        if (peek().type != TokenType::leftBracket)
            return false;

        size_t offset = 0;
        do
        {
            int squareDepth = 0;
            int parenDepth = 0;
            int braceDepth = 0;
            bool closed = false;
            for (; currentTokenIndex_ + offset < tokens_.size(); ++offset)
            {
                const TokenType type = peek(static_cast<int>(offset)).type;
                if (type == TokenType::leftBracket)
                    ++squareDepth;
                else if (type == TokenType::rightBracket)
                {
                    --squareDepth;
                    if (squareDepth == 0 && parenDepth == 0 && braceDepth == 0)
                    {
                        ++offset;
                        closed = true;
                        break;
                    }
                }
                else if (type == TokenType::leftParen)
                    ++parenDepth;
                else if (type == TokenType::rightParen)
                    --parenDepth;
                else if (type == TokenType::leftBrace)
                    ++braceDepth;
                else if (type == TokenType::rightBrace)
                    --braceDepth;
                else if (type == TokenType::endOfFile)
                    return false;
            }
            if (!closed)
                return false;
        }
        while (peek(static_cast<int>(offset)).type == TokenType::leftBracket);

        switch (peek(static_cast<int>(offset)).type)
        {
        case TokenType::kwLet:
        case TokenType::kwMut:
        case TokenType::kwConst:
        case TokenType::kwType:
        case TokenType::kwAttribute:
        case TokenType::kwApplication:
        case TokenType::kwSystem:
        case TokenType::kwFn:
        case TokenType::kwAsync:
        case TokenType::kwInterface:
        case TokenType::kwComponent:
        case TokenType::kwExtension:
        case TokenType::kwObject:
        case TokenType::kwEnum:
        case TokenType::kwFlagset:
        case TokenType::kwFlag:
        case TokenType::kwRealm:
            return true;
        default:
            return false;
        }
    }

    void Parser::parseWithAttributeClause(std::vector<NodePtr<AttributeStatement>>& attributes)
    {
        if (!match(TokenType::kwWith, true))
            return;

        if (!matchIdentifier())
            utError("Expected an attribute name after 'with'.", peek().loc);

        attributes.push_back(parseAttributeStatement(false));
        while (match(TokenType::comma, true))
        {
            if (!matchIdentifier())
                utError("Expected an attribute name after ',' in a 'with' clause.", peek().loc);
            attributes.push_back(parseAttributeStatement(false));
        }

        if (match(TokenType::kwWith))
            utError("A declaration may contain only one postfix 'with' clause.", peek().loc);
    }

    NodePtr<AttributeDeclaration> Parser::parseAttributeDeclaration(
        std::vector<NodePtr<AttributeStatement>> metaAttributes)
    {
        Token startTok = consume(TokenType::kwAttribute);
        auto name = makeNodePtr<Identifier>(consumeIdentifier());

        auto consumePolicyWord = [&]() -> Token
        {
            Token token = peek();
            if (!token.isIdentifier() && !token.isKeyword())
                utError("Expected an attribute policy name.", token.loc);
            advance();
            return token;
        };

        auto hasCompactTargetList = [&]()
        {
            if (peek().type != TokenType::leftParen)
                return false;

            size_t offset = 1;
            bool expectTarget = true;
            bool sawTarget = false;
            while (true)
            {
                const Token token = peek(static_cast<int>(offset));
                if (!token.isValid())
                    return false;
                if (token.type == TokenType::rightParen)
                    return sawTarget && !expectTarget &&
                           peek(static_cast<int>(offset + 1)).type == TokenType::leftParen;
                if (expectTarget)
                {
                    if (!token.isIdentifier() && !token.isKeyword())
                        return false;
                    sawTarget = true;
                    expectTarget = false;
                }
                else
                {
                    if (token.type != TokenType::opBitOr)
                        return false;
                    expectTarget = true;
                }
                ++offset;
            }
        };

        std::vector<std::string> targets;
        const bool compactDeclaration = hasCompactTargetList();
        if (compactDeclaration)
        {
            consume(TokenType::leftParen);
            targets.push_back(consumePolicyWord().value);
            while (match(TokenType::opBitOr, true))
                targets.push_back(consumePolicyWord().value);
            consume(TokenType::rightParen);
        }

        consume(TokenType::leftParen);
        std::vector<Parameter> parameters;
        if (!match(TokenType::rightParen))
        {
            while (true)
            {
                auto parameterName = makeNodePtr<Identifier>(consumeIdentifier());
                consume(TokenType::opColon);
                auto parameterType = parseType();
                NodePtr<Expression> defaultValue = nullptr;
                if (match(TokenType::opAssign, true))
                    defaultValue = parseExpression();
                parameters.emplace_back(
                    std::move(parameterName), std::move(parameterType),
                    std::move(defaultValue), false);

                if (!match(TokenType::comma, true))
                    break;
                expectElementAfterComma(TokenType::rightParen, "attribute parameter");
            }
        }
        consume(TokenType::rightParen);

        std::vector<std::string> retention;
        std::vector<std::string> conflictGroups;
        std::vector<std::string> requiredAttributes;
        std::vector<std::string> requiredAnyAttributes;
        std::vector<std::string> conflictingAttributes;
        std::vector<std::string> onlyWithAttributes;
        std::vector<std::string> beforeAttributes;
        std::vector<std::string> afterAttributes;
        std::vector<std::string> impliedAttributes;
        std::vector<std::string> processorTypes;
        size_t cardinalityMin = 0;
        size_t cardinalityMax = 1;
        bool hasExplicitCardinality = false;
        bool repeatable = false;
        bool inherited = false;
        bool scoped = false;

        auto policyTail = [](const std::string& qualifiedName)
        {
            const size_t separator = qualifiedName.rfind("::");
            return separator == std::string::npos
                ? qualifiedName
                : qualifiedName.substr(separator + 2);
        };
        auto appendPolicyNames = [&](const NodePtr<AttributeStatement>& policy,
                                     std::vector<std::string>& destination)
        {
            if (policy->args.empty())
                utError("Attribute policy '" + policy->qualifiedName + "' requires at least one argument.", policy->location());
            for (const Token& argument : policy->args)
                destination.push_back(argument.value);
        };

        for (const auto& policy : metaAttributes)
        {
            const std::string policyName = policyTail(policy->qualifiedName);
            if (policyName == "Targets") appendPolicyNames(policy, targets);
            else if (policyName == "Source") retention.push_back("source");
            else if (policyName == "Compile") retention.push_back("compile");
            else if (policyName == "Runtime") retention.push_back("runtime");
            else if (policyName == "Repeatable") repeatable = true;
            else if (policyName == "Inherited") inherited = true;
            else if (policyName == "Scoped") scoped = true;
            else if (policyName == "Requires") appendPolicyNames(policy, requiredAttributes);
            else if (policyName == "RequiresAny") appendPolicyNames(policy, requiredAnyAttributes);
            else if (policyName == "Conflicts") appendPolicyNames(policy, conflictingAttributes);
            else if (policyName == "OnlyWith") appendPolicyNames(policy, onlyWithAttributes);
            else if (policyName == "Before") appendPolicyNames(policy, beforeAttributes);
            else if (policyName == "After") appendPolicyNames(policy, afterAttributes);
            else if (policyName == "Implies") appendPolicyNames(policy, impliedAttributes);
            else if (policyName == "Processor") appendPolicyNames(policy, processorTypes);
            else if (policyName == "Exclusive")
            {
                if (policy->args.size() != 1)
                    utError("Attribute policy 'Exclusive' expects exactly one group.", policy->location());
                conflictGroups.push_back(policy->args.front().value);
            }
            else if (policyName == "Cardinality")
            {
                if (policy->args.empty() || policy->args.size() > 2)
                    utError("Attribute policy 'Cardinality' expects one or two integer arguments.", policy->location());
                for (const Token& argument : policy->args)
                    if (argument.type != TokenType::integerLiteral)
                        utError("Attribute policy 'Cardinality' accepts only integer literals.", argument.loc);
                cardinalityMin = traits::IntegerTraits<size_t>::IntegerResultCastedAs(getInteger(policy->args.front().value));
                cardinalityMax = policy->args.size() == 1
                    ? cardinalityMin
                    : traits::IntegerTraits<size_t>::IntegerResultCastedAs(getInteger(policy->args[1].value));
                if (cardinalityMin > cardinalityMax)
                    utError("Attribute cardinality minimum cannot exceed its maximum.", policy->location());
                hasExplicitCardinality = true;
                repeatable = cardinalityMax > 1;
            }
        }

        if (!compactDeclaration && match(TokenType::kwFor, true))
        {
            targets.push_back(consumePolicyWord().value);
            while (match(TokenType::opBitOr, true))
                targets.push_back(consumePolicyWord().value);
        }
        else if (!compactDeclaration && targets.empty())
        {
            utError("Attribute declarations require a 'for' target clause or [attribute::Targets(...)].", startTok.loc);
        }

        std::vector<NodePtr<AttributeStatement>> composedAttributes;
        while (!match(TokenType::semicolon))
        {
            if (match(TokenType::identifier, "compose", true))
            {
                if (!composedAttributes.empty())
                    utError("An attribute declaration may contain only one compose clause.", previous().loc);
                if (!match(TokenType::leftBracket))
                    utError("Expected an attribute list after 'compose'.", peek().loc);
                parseBracketAttributeList(composedAttributes);
                continue;
            }
            if (match(TokenType::kwWith))
            {
                std::vector<NodePtr<AttributeStatement>> policies;
                parseWithAttributeClause(policies);
                for (const auto& policy : policies)
                {
                    const std::string& policyName = policy->qualifiedName;
                    auto requireNoArguments = [&]()
                    {
                        if (!policy->args.empty())
                            utError("Attribute declaration policy '" + policyName + "' does not accept arguments.", policy->location());
                    };

                    if (policyName == "attribute::source" ||
                        policyName == "attribute::compile" ||
                        policyName == "attribute::runtime")
                    {
                        requireNoArguments();
                        retention.push_back(policyName.substr(std::string("attribute::").size()));
                    }
                    else if (policyName == "attribute::repeatable")
                    {
                        requireNoArguments();
                        repeatable = true;
                    }
                    else if (policyName == "attribute::inherited")
                    {
                        requireNoArguments();
                        inherited = true;
                    }
                    else if (policyName == "attribute::scoped")
                    {
                        requireNoArguments();
                        scoped = true;
                    }
                    else if (policyName == "attribute::conflict")
                    {
                        if (policy->args.size() != 1 ||
                            (policy->args.front().type != TokenType::stringLiteral &&
                             policy->args.front().type != TokenType::identifier))
                        {
                            utError("Attribute declaration policy 'attribute::conflict' expects one string or identifier argument.", policy->location());
                        }
                        conflictGroups.push_back(policy->args.front().value);
                    }
                    else
                    {
                        utError("Unknown attribute declaration policy '" + policyName + "'.", policy->location());
                    }
                }
                continue;
            }
            if (match(TokenType::identifier, "retain", true))
            {
                retention.push_back(consumePolicyWord().value);
                while (match(TokenType::opBitOr, true))
                    retention.push_back(consumePolicyWord().value);
                continue;
            }
            if (match(TokenType::identifier, "repeatable", true))
            {
                repeatable = true;
                continue;
            }
            if (match(TokenType::identifier, "conflicts", true))
            {
                conflictGroups.push_back(consumePolicyWord().value);
                while (match(TokenType::opBitOr, true))
                    conflictGroups.push_back(consumePolicyWord().value);
                continue;
            }
            if (match(TokenType::identifier, "inherited", true))
            {
                inherited = true;
                continue;
            }
            if (match(TokenType::identifier, "scoped", true))
            {
                scoped = true;
                continue;
            }

            utError("Unknown attribute declaration policy '" + peek().value + "'.", peek().loc);
        }
        consume(TokenType::semicolon);

        if (retention.empty())
            retention.push_back("compile");

        auto declaration = makeNodePtr<AttributeDeclaration>(
            std::move(name), std::move(parameters), std::move(targets),
            std::move(retention), std::move(conflictGroups),
            repeatable, inherited, scoped, startTok.loc);
        declaration->metaAttributes = std::move(metaAttributes);
        declaration->composedAttributes = std::move(composedAttributes);
        declaration->requiredAttributes = std::move(requiredAttributes);
        declaration->requiredAnyAttributes = std::move(requiredAnyAttributes);
        declaration->conflictingAttributes = std::move(conflictingAttributes);
        declaration->onlyWithAttributes = std::move(onlyWithAttributes);
        declaration->beforeAttributes = std::move(beforeAttributes);
        declaration->afterAttributes = std::move(afterAttributes);
        declaration->impliedAttributes = std::move(impliedAttributes);
        declaration->processorTypes = std::move(processorTypes);
        declaration->cardinalityMin = cardinalityMin;
        declaration->cardinalityMax = cardinalityMax;
        declaration->hasExplicitCardinality = hasExplicitCardinality;
        return declaration;
    }

    NodePtr<Statement> Parser::parseApplicationDeclaration()
    {
        const Token startToken = consume(TokenType::kwApplication);
        requiresAsyncModule_ = true;
        auto applicationName = makeNodePtr<Identifier>(consumeIdentifier());
        const Token applicationNameToken = applicationName->token;

        std::vector<NodePtr<AttributeStatement>> applicationAttributes;
        parseWithAttributeClause(applicationAttributes);
        consume(TokenType::leftBrace);

        auto makeType = [&](TokenType type, std::string value)
        {
            Token token{ .type = type, .value = std::move(value), .loc = startToken.loc };
            return makeNodePtr<TypeSpecifier>(std::move(token), std::vector<NodePtr<TypeSpecifier>>{},
                nullptr, 0, false, false, false, startToken.loc);
        };
        auto makeIdentifier = [&](std::string value)
        {
            return makeNodePtr<Identifier>(Token{
                .type = TokenType::identifier, .value = std::move(value), .loc = startToken.loc });
        };
        auto makeMember = [&](NodePtr<Expression> object, std::string name)
        {
            return makeNodePtr<MemberAccessExpression>(
                std::move(object), makeIdentifier(std::move(name)), TokenType::opDot, startToken.loc);
        };
        auto makeAppIdentifier = [&]() -> NodePtr<Expression>
        {
            return makeIdentifier("__application");
        };
        auto makeAppCall = [&](std::string method)
        {
            return makeNodePtr<FunctionCallExpression>(
                makeMember(makeAppIdentifier(), std::move(method)),
                std::vector<NodePtr<TypeSpecifier>>{}, std::vector<NodePtr<Expression>>{},
                false, false, startToken.loc);
        };
        auto makeAsyncRuntimeCall = [&](std::string method)
        {
            auto stdNamespace = makeNodePtr<MemberAccessExpression>(
                makeIdentifier("std"), makeIdentifier("async"), TokenType::opScope, startToken.loc);
            auto function = makeNodePtr<MemberAccessExpression>(
                std::move(stdNamespace), makeIdentifier(std::move(method)), TokenType::opScope, startToken.loc);
            return makeNodePtr<FunctionCallExpression>(
                std::move(function),
                std::vector<NodePtr<TypeSpecifier>>{},
                std::vector<NodePtr<Expression>>{},
                false, false, startToken.loc);
        };

        std::vector<ComponentMember> fields;
        std::unordered_map<std::string, NodePtr<FunctionDeclaration>> handlers;
        std::vector<std::string> ownedSystems;
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            if (peek().type == TokenType::identifier && peek().value == "on")
            {
                advance();
                const Token lifecycle = consumeIdentifier();
                if (lifecycle.value != "start" && lifecycle.value != "update" && lifecycle.value != "close")
                    utError("Application handlers must be 'on start', 'on update', or 'on close'.", lifecycle.loc);
                if (handlers.contains(lifecycle.value))
                    utError("Application handler '" + lifecycle.value + "' is declared more than once.", lifecycle.loc);

                if (match(TokenType::leftParen, true))
                {
                    if (!match(TokenType::rightParen))
                        utError("The first application runner supports parameterless lifecycle handlers.", peek().loc);
                    consume(TokenType::rightParen);
                }
                auto body = parseBlockStatement();
                std::string methodName = lifecycle.value == "start" ? "Start" :
                    (lifecycle.value == "update" ? "Update" : "Close");
                handlers.emplace(lifecycle.value, makeNodePtr<FunctionDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier(methodName),
                    std::vector<NodePtr<Identifier>>{}, false, std::vector<Parameter>{}, nullptr,
                    nullptr, nullptr, std::move(body), lifecycle.loc));
                continue;
            }

            Mutability mutability = Mutability::Mutable;
            bool applicationOwned = false;
            const bool systemOwned = peek().type == TokenType::kwSystem;
            if ((peek().type == TokenType::identifier && peek().value == "resource") ||
                peek().type == TokenType::kwSystem)
            {
                advance();
                applicationOwned = true;
            }
            else
            {
                const Token qualifier = advance();
                if (qualifier.type == TokenType::kwLet) mutability = Mutability::Immutable;
                else if (qualifier.type == TokenType::kwConst) mutability = Mutability::Const;
                else if (qualifier.type != TokenType::kwMut)
                    utError("Application fields require 'mut', 'let', 'const', 'resource', or 'system'.", qualifier.loc);
            }

            auto name = makeNodePtr<Identifier>(consumeIdentifier());
            if (systemOwned) ownedSystems.push_back(name->token.value);
            consume(TokenType::opColon);
            auto type = parseType();
            NodePtr<Expression> initializer = nullptr;
            if (match(TokenType::opAssign, true)) initializer = parseExpression();
            consume(TokenType::semicolon);
            fields.push_back(ComponentMember{
                .attributes = {}, .access = AccessModifier::Public,
                .declaration = makeNodePtr<VariableDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, mutability, std::move(name),
                    std::move(type), std::move(initializer), false, startToken.loc)
            });
            WIO_UNUSED(applicationOwned);
        }
        consume(TokenType::rightBrace);

        if (!handlers.contains("update"))
            utError("An application must declare exactly one 'on update' handler.", startToken.loc);

        auto addControlField = [&](std::string name, NodePtr<TypeSpecifier> type, NodePtr<Expression> initializer)
        {
            fields.push_back(ComponentMember{
                .attributes = {}, .access = AccessModifier::Public,
                .declaration = makeNodePtr<VariableDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, Mutability::Mutable,
                    makeIdentifier(std::move(name)), std::move(type), std::move(initializer), false, startToken.loc)
            });
        };
        addControlField("__exitRequested", makeType(TokenType::kwBool, "bool"),
            makeNodePtr<BoolLiteral>(Token{ .type = TokenType::kwFalse, .value = "false", .loc = startToken.loc }));
        addControlField("__exitCode", makeType(TokenType::kwI32, "i32"),
            makeNodePtr<IntegerLiteral>(Token{ .type = TokenType::integerLiteral, .value = "0", .loc = startToken.loc }));

        auto component = makeNodePtr<ComponentDeclaration>(
            std::move(applicationAttributes), std::move(applicationName),
            std::vector<NodePtr<Identifier>>{}, false, std::move(fields), startToken.loc);

        auto addReceiver = [&](NodePtr<FunctionDeclaration>& method)
        {
            auto receiverInner = makeNodePtr<TypeSpecifier>(applicationNameToken,
                std::vector<NodePtr<TypeSpecifier>>{}, nullptr, 0, false, false, false, startToken.loc);
            Token refToken{ .type = TokenType::kwRef, .value = "ref", .loc = startToken.loc };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(receiverInner));
            auto receiverType = makeNodePtr<TypeSpecifier>(std::move(refToken), std::move(generics),
                nullptr, 0, true, true, false, startToken.loc);
            method->parameters.insert(method->parameters.begin(),
                Parameter(makeIdentifier("_wio_self"), std::move(receiverType), nullptr, false));
            method->isExtensionMethod = true;
            method->extensionMutableReceiver = true;
            method->extensionMemberName = method->name->token.value;
        };

        std::vector<ExtensionMember> extensionMembers;
        std::vector<NodePtr<Statement>> firstExitStatements;
        firstExitStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeNodePtr<AssignmentExpression>(
                makeMember(makeNodePtr<SelfExpression>(startToken.loc), "__exitRequested"),
                Token{ .type = TokenType::opAssign, .value = "=", .loc = startToken.loc },
                makeNodePtr<BoolLiteral>(Token{ .type = TokenType::kwTrue, .value = "true", .loc = startToken.loc })),
            startToken.loc));
        firstExitStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeNodePtr<AssignmentExpression>(
                makeMember(makeNodePtr<SelfExpression>(startToken.loc), "__exitCode"),
                Token{ .type = TokenType::opAssign, .value = "=", .loc = startToken.loc },
                makeIdentifier("code")), startToken.loc));
        std::vector<Parameter> exitParameters;
        exitParameters.emplace_back(makeIdentifier("code"), makeType(TokenType::kwI32, "i32"), nullptr, false);
        auto firstExitCondition = makeNodePtr<UnaryExpression>(
            Token{ .type = TokenType::opLogicalNot, .value = "!", .loc = startToken.loc },
            makeMember(makeNodePtr<SelfExpression>(startToken.loc), "__exitRequested"),
            UnaryExpression::UnaryOperatorType::Prefix, startToken.loc);
        std::vector<NodePtr<Statement>> exitStatements;
        exitStatements.push_back(makeNodePtr<IfStatement>(
            std::move(firstExitCondition),
            makeNodePtr<BlockStatement>(std::move(firstExitStatements), startToken.loc),
            nullptr, Token::invalid(), startToken.loc));
        auto exitMethod = makeNodePtr<FunctionDeclaration>(
            std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier("Exit"),
            std::vector<NodePtr<Identifier>>{}, false, std::move(exitParameters), nullptr,
            nullptr, nullptr, makeNodePtr<BlockStatement>(std::move(exitStatements), startToken.loc), startToken.loc);
        addReceiver(exitMethod);
        extensionMembers.push_back(ExtensionMember{ .access = AccessModifier::Public, .mutableReceiver = true, .method = std::move(exitMethod) });

        for (const std::string lifecycle : { "start", "update", "close" })
        {
            NodePtr<FunctionDeclaration> method;
            if (auto iterator = handlers.find(lifecycle); iterator != handlers.end())
                method = std::move(iterator->second);
            else
            {
                const std::string methodName = lifecycle == "start" ? "Start" : "Close";
                method = makeNodePtr<FunctionDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier(methodName),
                    std::vector<NodePtr<Identifier>>{}, false, std::vector<Parameter>{}, nullptr,
                    nullptr, nullptr, makeNodePtr<BlockStatement>(std::vector<NodePtr<Statement>>{}, startToken.loc), startToken.loc);
            }

            auto makeSystemCallStatement = [&](const std::string& systemName, const std::string& methodName)
            {
                auto systemAccess = makeNodePtr<MemberAccessExpression>(
                    makeNodePtr<SelfExpression>(startToken.loc), makeIdentifier(systemName),
                    TokenType::opDot, startToken.loc);
                auto methodAccess = makeNodePtr<MemberAccessExpression>(
                    std::move(systemAccess), makeIdentifier(methodName), TokenType::opDot, startToken.loc);
                auto call = makeNodePtr<FunctionCallExpression>(
                    std::move(methodAccess), std::vector<NodePtr<TypeSpecifier>>{},
                    std::vector<NodePtr<Expression>>{}, false, false, startToken.loc);
                return makeNodePtr<ExpressionStatement>(std::move(call), startToken.loc);
            };
            if (auto block = method->body.As<BlockStatement>(); block && !ownedSystems.empty())
            {
                const std::string methodName = lifecycle == "start" ? "Start" :
                    (lifecycle == "update" ? "Update" : "Close");
                if (lifecycle == "start")
                {
                    for (const auto& systemName : ownedSystems)
                        block->statements.push_back(makeSystemCallStatement(systemName, methodName));
                }
                else
                {
                    std::vector<NodePtr<Statement>> calls;
                    if (lifecycle == "close")
                    {
                        for (auto iterator = ownedSystems.rbegin(); iterator != ownedSystems.rend(); ++iterator)
                            calls.push_back(makeSystemCallStatement(*iterator, methodName));
                    }
                    else
                    {
                        for (const auto& systemName : ownedSystems)
                            calls.push_back(makeSystemCallStatement(systemName, methodName));
                    }
                    calls.insert(calls.end(), block->statements.begin(), block->statements.end());
                    block->statements = std::move(calls);
                }
            }
            addReceiver(method);
            extensionMembers.push_back(ExtensionMember{ .access = AccessModifier::Public, .mutableReceiver = true, .method = std::move(method) });
        }

        auto targetType = makeNodePtr<TypeSpecifier>(applicationNameToken,
            std::vector<NodePtr<TypeSpecifier>>{}, nullptr, 0, false, false, false, startToken.loc);
        auto extension = makeNodePtr<ExtensionDeclaration>(
            std::vector<NodePtr<AttributeStatement>>{},
            makeIdentifier(applicationNameToken.value + "Lifecycle"),
            std::move(targetType), std::move(extensionMembers), startToken.loc);

        std::vector<NodePtr<Statement>> entryStatements;
        entryStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeAsyncRuntimeCall("BindMain"), startToken.loc));
        std::vector<NodePtr<Expression>> noArguments;
        auto constructorCall = makeNodePtr<FunctionCallExpression>(
            makeIdentifier(applicationNameToken.value), std::vector<NodePtr<TypeSpecifier>>{},
            std::move(noArguments), false, false, startToken.loc);
        entryStatements.push_back(makeNodePtr<VariableDeclaration>(
            std::vector<NodePtr<AttributeStatement>>{}, Mutability::Mutable,
            makeIdentifier("__application"), nullptr, std::move(constructorCall), false, startToken.loc));
        entryStatements.push_back(makeNodePtr<ExpressionStatement>(makeAppCall("Start"), startToken.loc));
        entryStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeAsyncRuntimeCall("DrainMain"), startToken.loc));
        auto condition = makeNodePtr<UnaryExpression>(
            Token{ .type = TokenType::opLogicalNot, .value = "!", .loc = startToken.loc },
            makeMember(makeAppIdentifier(), "__exitRequested"), UnaryExpression::UnaryOperatorType::Prefix, startToken.loc);
        std::vector<NodePtr<Statement>> updateStatements;
        updateStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeAsyncRuntimeCall("DrainMain"), startToken.loc));
        updateStatements.push_back(makeNodePtr<ExpressionStatement>(makeAppCall("Update"), startToken.loc));
        updateStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeAsyncRuntimeCall("DrainMain"), startToken.loc));
        auto updateBody = makeNodePtr<BlockStatement>(std::move(updateStatements), startToken.loc);
        entryStatements.push_back(makeNodePtr<WhileStatement>(std::move(condition), std::move(updateBody), startToken.loc));
        entryStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeAsyncRuntimeCall("DrainMain"), startToken.loc));
        entryStatements.push_back(makeNodePtr<ExpressionStatement>(makeAppCall("Close"), startToken.loc));
        entryStatements.push_back(makeNodePtr<ExpressionStatement>(
            makeAsyncRuntimeCall("DrainMain"), startToken.loc));
        entryStatements.push_back(makeNodePtr<ReturnStatement>(
            makeMember(makeAppIdentifier(), "__exitCode"), startToken.loc));
        auto entry = makeNodePtr<FunctionDeclaration>(
            std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier("Entry"),
            std::vector<NodePtr<Identifier>>{}, false, std::vector<Parameter>{},
            makeType(TokenType::kwI32, "i32"), nullptr, nullptr,
            makeNodePtr<BlockStatement>(std::move(entryStatements), startToken.loc), startToken.loc);
        entry->isApplicationEntry = true;
        entry->applicationName = applicationNameToken.value;

        std::vector<NodePtr<Statement>> declarations;
        declarations.push_back(std::move(component));
        declarations.push_back(std::move(extension));
        declarations.push_back(std::move(entry));
        return makeNodePtr<DeclarationGroup>(std::move(declarations), startToken.loc);
    }

    NodePtr<Statement> Parser::parseSystemDeclaration()
    {
        const Token startToken = consume(TokenType::kwSystem);
        auto systemName = makeNodePtr<Identifier>(consumeIdentifier());
        const Token systemNameToken = systemName->token;
        std::vector<NodePtr<AttributeStatement>> attributes;
        parseWithAttributeClause(attributes);
        consume(TokenType::leftBrace);

        auto makeIdentifier = [&](std::string value)
        {
            return makeNodePtr<Identifier>(Token{
                .type = TokenType::identifier, .value = std::move(value), .loc = startToken.loc });
        };
        std::vector<ComponentMember> fields;
        std::unordered_map<std::string, NodePtr<FunctionDeclaration>> handlers;
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            if (peek().type == TokenType::identifier && peek().value == "on")
            {
                advance();
                const Token lifecycle = consumeIdentifier();
                if (lifecycle.value != "start" && lifecycle.value != "update" && lifecycle.value != "close")
                    utError("System handlers must be 'on start', 'on update', or 'on close'.", lifecycle.loc);
                if (handlers.contains(lifecycle.value))
                    utError("System handler '" + lifecycle.value + "' is declared more than once.", lifecycle.loc);
                if (match(TokenType::leftParen, true))
                {
                    if (!match(TokenType::rightParen))
                        utError("The first system scheduler supports parameterless lifecycle handlers.", peek().loc);
                    consume(TokenType::rightParen);
                }
                auto body = parseBlockStatement();
                const std::string methodName = lifecycle.value == "start" ? "Start" :
                    (lifecycle.value == "update" ? "Update" : "Close");
                handlers.emplace(lifecycle.value, makeNodePtr<FunctionDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier(methodName),
                    std::vector<NodePtr<Identifier>>{}, false, std::vector<Parameter>{}, nullptr,
                    nullptr, nullptr, std::move(body), lifecycle.loc));
                continue;
            }

            const Token qualifier = advance();
            Mutability mutability = Mutability::Mutable;
            if (qualifier.type == TokenType::kwLet) mutability = Mutability::Immutable;
            else if (qualifier.type == TokenType::kwConst) mutability = Mutability::Const;
            else if (qualifier.type != TokenType::kwMut)
                utError("System fields require 'mut', 'let', or 'const'.", qualifier.loc);
            auto name = makeNodePtr<Identifier>(consumeIdentifier());
            consume(TokenType::opColon);
            auto type = parseType();
            NodePtr<Expression> initializer = nullptr;
            if (match(TokenType::opAssign, true)) initializer = parseExpression();
            consume(TokenType::semicolon);
            fields.push_back(ComponentMember{
                .attributes = {}, .access = AccessModifier::Public,
                .declaration = makeNodePtr<VariableDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, mutability, std::move(name),
                    std::move(type), std::move(initializer), false, startToken.loc)
            });
        }
        consume(TokenType::rightBrace);

        auto component = makeNodePtr<ComponentDeclaration>(
            std::move(attributes), std::move(systemName),
            std::vector<NodePtr<Identifier>>{}, false, std::move(fields), startToken.loc);

        auto addReceiver = [&](NodePtr<FunctionDeclaration>& method)
        {
            auto receiverInner = makeNodePtr<TypeSpecifier>(systemNameToken,
                std::vector<NodePtr<TypeSpecifier>>{}, nullptr, 0, false, false, false, startToken.loc);
            Token refToken{ .type = TokenType::kwRef, .value = "ref", .loc = startToken.loc };
            std::vector<NodePtr<TypeSpecifier>> generics;
            generics.push_back(std::move(receiverInner));
            auto receiverType = makeNodePtr<TypeSpecifier>(std::move(refToken), std::move(generics),
                nullptr, 0, true, true, false, startToken.loc);
            method->parameters.insert(method->parameters.begin(),
                Parameter(makeIdentifier("_wio_self"), std::move(receiverType), nullptr, false));
            method->isExtensionMethod = true;
            method->extensionMutableReceiver = true;
            method->extensionMemberName = method->name->token.value;
        };

        std::vector<ExtensionMember> members;
        for (const std::string lifecycle : { "start", "update", "close" })
        {
            NodePtr<FunctionDeclaration> method;
            if (auto iterator = handlers.find(lifecycle); iterator != handlers.end())
                method = std::move(iterator->second);
            else
            {
                const std::string methodName = lifecycle == "start" ? "Start" :
                    (lifecycle == "update" ? "Update" : "Close");
                method = makeNodePtr<FunctionDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier(methodName),
                    std::vector<NodePtr<Identifier>>{}, false, std::vector<Parameter>{}, nullptr,
                    nullptr, nullptr, makeNodePtr<BlockStatement>(std::vector<NodePtr<Statement>>{}, startToken.loc), startToken.loc);
            }
            addReceiver(method);
            members.push_back(ExtensionMember{ .access = AccessModifier::Public, .mutableReceiver = true, .method = std::move(method) });
        }
        auto targetType = makeNodePtr<TypeSpecifier>(systemNameToken,
            std::vector<NodePtr<TypeSpecifier>>{}, nullptr, 0, false, false, false, startToken.loc);
        auto extension = makeNodePtr<ExtensionDeclaration>(
            std::vector<NodePtr<AttributeStatement>>{}, makeIdentifier(systemNameToken.value + "Lifecycle"),
            std::move(targetType), std::move(members), startToken.loc);
        std::vector<NodePtr<Statement>> declarations;
        declarations.push_back(std::move(component));
        declarations.push_back(std::move(extension));
        return makeNodePtr<DeclarationGroup>(std::move(declarations), startToken.loc);
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

        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());

        NodePtr<TypeSpecifier> specifier;
        if (match(TokenType::opColon, true))
            specifier = parseType();

        parseWithAttributeClause(attributes);

        NodePtr<Expression> initializer = nullptr;

        if (match(TokenType::opAssign, true))
        {
            initializer = parseExpression();
        }

        validateOrdinaryVariableDeclaration(
            mutability,
            specifier != nullptr,
            initializer != nullptr,
            startTok.loc
        );
        
        consume(TokenType::semicolon);

        return makeNodePtr<VariableDeclaration>(
            std::move(attributes),
            mutability,
            std::move(name), 
            std::move(specifier),
            std::move(initializer),
            false,
            startTok.loc
        );
    }

    Parser::GenericParameterList Parser::parseGenericParameterList()
    {
        GenericParameterList result;
        if (!match(TokenType::opLess, true))
            return result;

        bool sawDefault = false;
        while (true)
        {
            const bool isConstParameter = match(TokenType::kwConst, true);
            auto parameter = makeNodePtr<Identifier>(consume(TokenType::identifier));
            parameter->isConstGenericParameter = isConstParameter;
            if (isConstParameter)
            {
                consume(TokenType::opColon);
                parameter->genericValueType = parseType();
            }
            const bool isPack = match(TokenType::opRangeInclusive, true);
            if (isPack)
            {
                if (isConstParameter)
                    utError("Const generic parameter packs are not supported.", parameter->location());
                result.hasParameterPack = true;
                if (match(TokenType::opAssign, false))
                    utError("Generic parameter packs cannot have default arguments.", peek().loc);
            }
            else if (match(TokenType::opAssign, true))
            {
                parameter->genericDefaultType = isConstParameter ? parseGenericArgument() : parseType();
                sawDefault = true;
            }
            else if (sawDefault)
            {
                utError("Generic parameters without defaults cannot follow a defaulted parameter.", parameter->location());
            }

            result.parameters.push_back(std::move(parameter));
            if (!match(TokenType::comma, true))
                break;

            expectElementAfterComma(TokenType::opGreater, "generic parameter");
            if (result.hasParameterPack)
                utError("Generic parameter packs must be trailing.", currentOrPreviousLocation());
        }

        consumeGenericClose();
        return result;
    }

    NodePtr<AttributeStatement> Parser::parseWhereClause(
        const std::vector<NodePtr<Identifier>>& genericParameters,
        const bool hasGenericParameterPack)
    {
        if (!match(TokenType::kwWhere, true))
            return nullptr;

        const Location whereLocation = previous().loc;
        if (genericParameters.empty())
            utError("A where clause requires a generic declaration.", whereLocation);

        std::vector<std::vector<Token>> argumentGroups(genericParameters.size());
        std::vector<std::vector<NodePtr<TypeSpecifier>>> typeArgumentGroups(genericParameters.size());
        std::vector<bool> constrainedParameters(genericParameters.size(), false);
        for (size_t parameterIndex = 0; parameterIndex < genericParameters.size(); ++parameterIndex)
        {
            const auto& parameter = genericParameters[parameterIndex];
            argumentGroups[parameterIndex].push_back(
                Token{TokenType::kwTrue, "true", parameter ? parameter->location() : whereLocation});
            typeArgumentGroups[parameterIndex].push_back(nullptr);
        }

        while (true)
        {
            Token parameterToken = consume(TokenType::identifier);
            auto parameterIt = std::ranges::find_if(genericParameters, [&](const NodePtr<Identifier>& parameter)
            {
                return parameter && parameter->token.value == parameterToken.value;
            });
            if (parameterIt == genericParameters.end())
                utError("Where clause references an unknown generic parameter '" + parameterToken.value + "'.", parameterToken.loc);

            const size_t parameterIndex = static_cast<size_t>(std::distance(genericParameters.begin(), parameterIt));
            if (constrainedParameters[parameterIndex])
                utError("Where clause repeats generic parameter '" + parameterToken.value + "'.", parameterToken.loc);

            consume(TokenType::opColon);
            const bool isPack = hasGenericParameterPack && parameterIndex + 1 == genericParameters.size();
            argumentGroups[parameterIndex].clear();
            typeArgumentGroups[parameterIndex].clear();
            constrainedParameters[parameterIndex] = true;

            while (true)
            {
                NodePtr<TypeSpecifier> constraint = parseType();
                if (!constraint->generics.empty())
                    utError("Where-clause constraint names must omit operands; write 'T: Trait' rather than 'T: Trait<T>'.", constraint->location());

                auto operand = makeNodePtr<TypeSpecifier>(
                    parameterToken,
                    std::vector<NodePtr<TypeSpecifier>>{},
                    nullptr,
                    0,
                    false,
                    false,
                    isPack,
                    parameterToken.loc);
                constraint->generics.push_back(std::move(operand));

                Token rawConstraint = constraint->name;
                rawConstraint.value += "<" + parameterToken.value + (isPack ? "...>" : ">");
                argumentGroups[parameterIndex].push_back(std::move(rawConstraint));
                typeArgumentGroups[parameterIndex].push_back(std::move(constraint));

                if (!match(TokenType::opPlus, true))
                    break;
            }

            if (!match(TokenType::comma, true))
                break;
        }

        std::vector<Token> arguments;
        std::vector<NodePtr<TypeSpecifier>> typeArguments;
        std::vector<size_t> groupOffsets;
        groupOffsets.reserve(genericParameters.size() + 1);
        for (size_t parameterIndex = 0; parameterIndex < genericParameters.size(); ++parameterIndex)
        {
            groupOffsets.push_back(arguments.size());
            for (auto& argument : argumentGroups[parameterIndex])
                arguments.push_back(std::move(argument));
            for (auto& typeArgument : typeArgumentGroups[parameterIndex])
                typeArguments.push_back(std::move(typeArgument));
        }
        groupOffsets.push_back(arguments.size());

        auto result = makeNodePtr<AttributeStatement>(
            Attribute::Apply, std::move(arguments), std::move(typeArguments), whereLocation);
        result->constraintGroupOffsets = std::move(groupOffsets);
        result->conjunctiveConstraintGroups = true;
        return result;
    }

    NodePtr<TypeAliasDeclaration> Parser::parseTypeAliasDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        for (const auto& attribute : attributes)
        {
            if (!attribute || attribute->attribute == Attribute::Apply)
                continue;

            utError("Only @Apply is currently supported on type aliases.", attribute->location());
        }

        Token startTok = consume(TokenType::kwType);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());

        auto genericParameterList = parseGenericParameterList();

        if (auto whereClause = parseWhereClause(genericParameterList.parameters, genericParameterList.hasParameterPack))
            attributes.push_back(std::move(whereClause));

        consume(TokenType::opAssign);
        NodePtr<TypeSpecifier> aliasedType = parseType();
        parseWithAttributeClause(attributes);
        consume(TokenType::semicolon);

        return makeNodePtr<TypeAliasDeclaration>(
            std::move(attributes),
            std::move(name),
            std::move(genericParameterList.parameters),
            genericParameterList.hasParameterPack,
            std::move(aliasedType),
            startTok.loc
        );
    }

    NodePtr<FunctionDeclaration> Parser::parseFunctionDeclaration(std::vector<NodePtr<AttributeStatement>> attributes, bool isLifecycle, bool isStructMethod, bool isAsync)
    {
        Token startTok = peek();
        if (!isLifecycle)
        {
            if (isAsync)
                consume(TokenType::kwAsync);
            consume(TokenType::kwFn);
        }

        auto consumeOperatorToken = [&]() -> Token
        {
            if (match(TokenType::leftParen, false))
            {
                if (peek(1).type != TokenType::rightParen)
                    utError("Expected '()' for the call operator overload.", peek().loc);
                Token callOperatorToken = advance();
                consume(TokenType::rightParen);
                return callOperatorToken;
            }

            Token consumed = advance();
            if (consumed.type == TokenType::leftBracket)
                consume(TokenType::rightBracket);
            return consumed;
        };

        std::optional<Token> operatorToken;
        NodePtr<Identifier> name = nullptr;
        if (!isLifecycle && match(TokenType::identifier, "operator", false))
        {
            advance();
            operatorToken = consumeOperatorToken();
            if (!common::isOverloadableOperatorToken(operatorToken->type))
            {
                utError("Expected an overloadable operator after 'operator'.", operatorToken->loc);
            }

            Token syntheticNameToken = *operatorToken;
            syntheticNameToken.type = TokenType::identifier;
            syntheticNameToken.value = "__op_pending";
            name = makeNodePtr<Identifier>(std::move(syntheticNameToken));
        }
        else if (!isLifecycle &&
                 ((peek().type != TokenType::leftParen && common::isOverloadableOperatorToken(peek().type)) ||
                  (peek().type == TokenType::leftParen && peek(1).type == TokenType::rightParen)))
        {
            operatorToken = consumeOperatorToken();

            Token syntheticNameToken = *operatorToken;
            syntheticNameToken.type = TokenType::identifier;
            syntheticNameToken.value = "__op_pending";
            name = makeNodePtr<Identifier>(std::move(syntheticNameToken));
        }
        else
        {
            name = makeNodePtr<Identifier>(consumeIdentifier());
        }

        auto genericParameterList = parseGenericParameterList();

        consume(TokenType::leftParen);
        
        std::vector<Parameter> parameters;
        if (!match(TokenType::rightParen))
        {
            NodePtr<Identifier> paramName = makeNodePtr<Identifier>(consumeIdentifier());
            NodePtr<TypeSpecifier> paramType = nullptr;
            NodePtr<Expression> defaultValue = nullptr;
            bool isParameterPack = false;

            if (match(TokenType::opColon, true))
            {
                paramType = parseType();
                isParameterPack = paramType && paramType->isPackExpansion;
            }

            if (match(TokenType::opAssign, true))
                defaultValue = parseExpression();

            parameters.emplace_back(std::move(paramName), std::move(paramType), std::move(defaultValue), isParameterPack);

            while (match(TokenType::comma, true))
            {
                expectElementAfterComma(TokenType::rightParen, "function parameter");

                NodePtr<Identifier> nextParamName = makeNodePtr<Identifier>(consumeIdentifier());
                NodePtr<TypeSpecifier> nextParamType = nullptr;
                NodePtr<Expression> nextDefaultValue = nullptr;
                bool nextIsParameterPack = false;

                if (match(TokenType::opColon, true))
                {
                    nextParamType = parseType();
                    nextIsParameterPack = nextParamType && nextParamType->isPackExpansion;
                }

                if (match(TokenType::opAssign, true))
                    nextDefaultValue = parseExpression();

                parameters.emplace_back(std::move(nextParamName), std::move(nextParamType), std::move(nextDefaultValue), nextIsParameterPack);
            }
        }
        consume(TokenType::rightParen);

        if (operatorToken.has_value())
        {
            auto overloadName = isStructMethod
                ? common::getMemberOperatorOverloadName(operatorToken->type, parameters.size())
                : common::getFreeOperatorOverloadName(operatorToken->type, parameters.size());
            if (!overloadName.has_value())
            {
                utError(
                    isStructMethod
                        ? "Invalid member operator overload arity for the selected operator."
                        : "Invalid free operator overload arity for the selected operator.",
                    operatorToken->loc
                );
            }

            name->token.value = std::string(*overloadName);
        }

        NodePtr<TypeSpecifier> returnType = nullptr;
        if (match(TokenType::opArrow, true)) // '->'
        {
            returnType = parseType();
        }

        if (auto whereClause = parseWhereClause(genericParameterList.parameters, genericParameterList.hasParameterPack))
            attributes.push_back(std::move(whereClause));

        parseWithAttributeClause(attributes);
        
        NodePtr<Expression> whenCond = nullptr;
        NodePtr<Expression> whenFallback = nullptr;

        if (match(TokenType::kwWhen, true))
        {
            whenCond = parseExpression();
            
            if (match(TokenType::kwElse, true))
                whenFallback = parseExpression();
        }

        NodePtr<Statement> body = match(TokenType::semicolon, true) ? nullptr : parseBlockStatement();

        auto declaration = makeNodePtr<FunctionDeclaration>(
            std::move(attributes),
            std::move(name),
            std::move(genericParameterList.parameters),
            genericParameterList.hasParameterPack,
            std::move(parameters),
            std::move(returnType),
            std::move(whenCond),
            std::move(whenFallback),
            std::move(body),
            startTok.loc
        );
        declaration->isAsync = isAsync;
        return declaration;
    }

    NodePtr<Statement> Parser::parseInterfaceDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwInterface);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        auto genericParameterList = parseGenericParameterList();

        if (auto whereClause = parseWhereClause(genericParameterList.parameters, genericParameterList.hasParameterPack))
            attributes.push_back(std::move(whereClause));

        parseWithAttributeClause(attributes);

        consume(TokenType::leftBrace);
        std::vector<NodePtr<FunctionDeclaration>> methods;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> methodAttrs;
            parseLeadingAttributes(methodAttrs);

            const bool isAsync = match(TokenType::kwAsync);
            auto method = parseFunctionDeclaration(std::move(methodAttrs), false, true, isAsync);
            
            if (method->body != nullptr) {
                utError("Interface methods cannot have a body. Use ';' instead of '{...}'.", method->location());
            }

            methods.push_back(std::move(method));
        }
        consume(TokenType::rightBrace);
        
        return makeNodePtr<InterfaceDeclaration>(std::move(attributes), std::move(name), std::move(genericParameterList.parameters), genericParameterList.hasParameterPack, std::move(methods), startTok.loc);
    }

    NodePtr<Statement> Parser::parseComponentDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwComponent);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        auto genericParameterList = parseGenericParameterList();

        if (auto whereClause = parseWhereClause(genericParameterList.parameters, genericParameterList.hasParameterPack))
            attributes.push_back(std::move(whereClause));

        parseWithAttributeClause(attributes);

        consume(TokenType::leftBrace);
        std::vector<ComponentMember> members;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> memberAttrs;
            parseLeadingAttributes(memberAttrs);

            AccessModifier access = AccessModifier::None; 
            if (match(TokenType::kwPublic, true)) access = AccessModifier::Public;
            else if (match(TokenType::kwPrivate, true)) access = AccessModifier::Private;
            else if (match(TokenType::kwProtected, true)) access = AccessModifier::Protected;

            if (match(TokenType::kwAsync) || match(TokenType::kwFn) ||
                match(TokenType::identifier, "OnConstruct", false) ||
                match(TokenType::identifier, "OnDestruct", false))
            {
                const bool isAsync = match(TokenType::kwAsync);
                bool isLifecycle = !isAsync && !match(TokenType::kwFn);
                auto method = parseFunctionDeclaration(std::move(memberAttrs), isLifecycle, true, isAsync);
                
                members.push_back(ComponentMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(method)
                });
            }
            else
            {
                bool isPackField = false;
                if (match(TokenType::identifier, "pack", false))
                {
                    advance();
                    isPackField = true;
                }

                Mutability memberMutability = Mutability::Mutable;
                if (match(TokenType::kwConst, true))
                    memberMutability = Mutability::Const;

                NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consumeIdentifier());
                
                NodePtr<TypeSpecifier> memberType = nullptr;
                if (match(TokenType::opColon, true)) memberType = parseType();

                parseWithAttributeClause(memberAttrs);
                
                NodePtr<Expression> init = nullptr;
                if (match(TokenType::opAssign, true)) init = parseExpression();

                if (!memberType && !init) {
                    utError("Component members must have an explicit type or an initializer.", memberName->location());
                }

                match(TokenType::comma, true);
                match(TokenType::semicolon, true); 

                auto varDecl =
                    makeNodePtr<VariableDeclaration>(
                        std::move(memberAttrs),
                        memberMutability,
                        std::move(memberName),
                        std::move(memberType),
                        std::move(init),
                        isPackField,
                        memberName->location()
                    );

                members.push_back(ComponentMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(varDecl)
                });
            }
        }
        consume(TokenType::rightBrace);
        return makeNodePtr<ComponentDeclaration>(std::move(attributes), std::move(name), std::move(genericParameterList.parameters), genericParameterList.hasParameterPack, std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseExtensionDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwExtension);

        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        consume(TokenType::kwFor);
        NodePtr<TypeSpecifier> targetType = parseType();
        parseWithAttributeClause(attributes);
        consume(TokenType::leftBrace);

        std::vector<ExtensionMember> members;
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> methodAttrs;
            parseLeadingAttributes(methodAttrs);

            AccessModifier access = AccessModifier::None;
            if (match(TokenType::kwPublic, true)) access = AccessModifier::Public;
            else if (match(TokenType::kwPrivate, true)) access = AccessModifier::Private;
            else if (match(TokenType::kwProtected, true)) access = AccessModifier::Protected;

            bool mutableReceiver = false;
            if (match(TokenType::kwRef, true))
                mutableReceiver = true;
            else
                consume(TokenType::kwView);

            const bool isAsync = match(TokenType::kwAsync);
            auto method = parseFunctionDeclaration(std::move(methodAttrs), false, false, isAsync);
            if (!targetType->generics.empty())
                utError("Generic extension targets are not supported yet.", targetType->location());

            Token receiverInnerToken = targetType->name;
            auto receiverInner = makeNodePtr<TypeSpecifier>(
                std::move(receiverInnerToken), std::vector<NodePtr<TypeSpecifier>>{}, nullptr,
                0, false, false, false, targetType->location());
            Token receiverToken{
                .type = mutableReceiver ? TokenType::kwRef : TokenType::kwView,
                .value = mutableReceiver ? "ref" : "view",
                .loc = method->location()
            };
            std::vector<NodePtr<TypeSpecifier>> receiverGenerics;
            receiverGenerics.push_back(std::move(receiverInner));
            auto receiverType = makeNodePtr<TypeSpecifier>(
                std::move(receiverToken), std::move(receiverGenerics), nullptr,
                0, true, mutableReceiver, false, method->location());
            Token receiverNameToken{
                .type = TokenType::identifier,
                .value = "_wio_self",
                .loc = method->location()
            };
            method->parameters.insert(
                method->parameters.begin(),
                Parameter(makeNodePtr<Identifier>(std::move(receiverNameToken)), std::move(receiverType), nullptr, false));
            method->isExtensionMethod = true;
            method->extensionMutableReceiver = mutableReceiver;
            method->extensionMemberName = method->name->token.value;
            members.push_back(ExtensionMember{
                .access = access,
                .mutableReceiver = mutableReceiver,
                .method = std::move(method)
            });
        }
        consume(TokenType::rightBrace);
        return makeNodePtr<ExtensionDeclaration>(
            std::move(attributes), std::move(name), std::move(targetType), std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseObjectDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwObject);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        auto genericParameterList = parseGenericParameterList();

        if (auto whereClause = parseWhereClause(genericParameterList.parameters, genericParameterList.hasParameterPack))
            attributes.push_back(std::move(whereClause));

        parseWithAttributeClause(attributes);

        consume(TokenType::leftBrace);
        std::vector<ObjectMember> members;

        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            std::vector<NodePtr<AttributeStatement>> memberAttrs;
            parseLeadingAttributes(memberAttrs);

            AccessModifier access = AccessModifier::None; 
            if (match(TokenType::kwPublic, true)) access = AccessModifier::Public;
            else if (match(TokenType::kwPrivate, true)) access = AccessModifier::Private;
            else if (match(TokenType::kwProtected, true)) access = AccessModifier::Protected;

            if (match(TokenType::kwAsync) || match(TokenType::kwFn) ||
                match(TokenType::identifier, "OnConstruct", false) ||
                match(TokenType::identifier, "OnDestruct", false))
            {
                const bool isAsync = match(TokenType::kwAsync);
                bool isLifecycle = !isAsync && !match(TokenType::kwFn);
                auto method = parseFunctionDeclaration(std::move(memberAttrs), isLifecycle, true, isAsync);
                
                members.push_back(ObjectMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(method)
                });
            }
            else
            {
                bool isPackField = false;
                if (match(TokenType::identifier, "pack", false))
                {
                    advance();
                    isPackField = true;
                }

                Mutability memberMutability = Mutability::Mutable;
                if (match(TokenType::kwConst, true))
                    memberMutability = Mutability::Const;

                NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consumeIdentifier());
                
                NodePtr<TypeSpecifier> memberType = nullptr;
                if (match(TokenType::opColon, true)) memberType = parseType();

                parseWithAttributeClause(memberAttrs);
                
                NodePtr<Expression> init = nullptr;
                if (match(TokenType::opAssign, true)) init = parseExpression();

                if (!memberType && !init) {
                    utError("Object members must have an explicit type or an initializer.", memberName->location());
                }

                match(TokenType::comma, true);
                match(TokenType::semicolon, true);

                auto varDecl =
                    makeNodePtr<VariableDeclaration>(
                        std::move(memberAttrs),
                        memberMutability,
                        std::move(memberName),
                        std::move(memberType),
                        std::move(init),
                        isPackField,
                        memberName->location()
                    );

                members.push_back(ObjectMember{
                    .attributes = std::vector<NodePtr<AttributeStatement>>{},
                    .access = access,
                    .declaration = std::move(varDecl)
                });
            }
        }
        consume(TokenType::rightBrace);
        return makeNodePtr<ObjectDeclaration>(std::move(attributes), std::move(name), std::move(genericParameterList.parameters), genericParameterList.hasParameterPack, std::move(members), startTok.loc);
    }

    NodePtr<Statement> Parser::parseFlagDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwFlag);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        parseWithAttributeClause(attributes);
        consume(TokenType::semicolon); // flag IsDead;
        
        return makeNodePtr<FlagDeclaration>(std::move(attributes), std::move(name), startTok.loc);
    }

    NodePtr<Statement> Parser::parseEnumDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        Token startTok = consume(TokenType::kwEnum);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        parseWithAttributeClause(attributes);
        
        consume(TokenType::leftBrace);
        std::vector<EnumMember> members;
        
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consumeIdentifier());
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
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());
        parseWithAttributeClause(attributes);
        
        consume(TokenType::leftBrace);
        std::vector<EnumMember> members;
        
        while (peek().isValid() && !match(TokenType::rightBrace))
        {
            NodePtr<Identifier> memberName = makeNodePtr<Identifier>(consumeIdentifier());
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
            matchVar = consumeIdentifier();

        if (hasParen)
            consume(TokenType::rightParen);

        NodePtr<Statement> thenBranch = match(TokenType::leftBrace) ? parseBlockStatement() : parseStatement();
        NodePtr<Statement> elseBranch = nullptr;

        if (match(TokenType::kwElse, true))
        {
            if (match(TokenType::kwIf))
            {
                elseBranch = parseIfStatement();
            }
            else
            {
                elseBranch = match(TokenType::leftBrace) ? parseBlockStatement() : parseStatement();
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

    NodePtr<Statement> Parser::parseForInStatement()
    {
        Token startTok = advance();
        if (startTok.type != TokenType::kwFor && startTok.type != TokenType::kwForeach)
            utError("Expected 'for' or 'foreach'.", startTok.loc);

        Location startLoc = startTok.loc;

        bool hasParen = match(TokenType::leftParen, true);

        auto headerLooksLikeCStyleFor = [&]() -> bool
        {
            if (startTok.type != TokenType::kwFor || !hasParen)
                return false;

            int parenDepth = 0;
            int braceDepth = 0;
            int bracketDepth = 0;

            for (int offset = 0; ; ++offset)
            {
                Token token = peek(offset);
                if (!token.isValid())
                    break;

                if (token.type == TokenType::leftParen) ++parenDepth;
                else if (token.type == TokenType::rightParen)
                {
                    if (parenDepth == 0 && braceDepth == 0 && bracketDepth == 0)
                        break;
                    --parenDepth;
                }
                else if (token.type == TokenType::leftBrace) ++braceDepth;
                else if (token.type == TokenType::rightBrace && braceDepth > 0) --braceDepth;
                else if (token.type == TokenType::leftBracket) ++bracketDepth;
                else if (token.type == TokenType::rightBracket && bracketDepth > 0) --bracketDepth;
                else if (token.type == TokenType::semicolon && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0)
                    return true;
            }

            return false;
        };

        if (headerLooksLikeCStyleFor())
            return parseCForStatement(startLoc);

        std::vector<NodePtr<Identifier>> bindings;
        std::vector<ForBindingMode> bindingModes;

        auto parseBindingMode = [&]() -> ForBindingMode
        {
            if (match(TokenType::kwMut, true))
                return ForBindingMode::ValueMutable;
            if (match(TokenType::kwLet, true))
                return ForBindingMode::ValueImmutable;
            if (match(TokenType::kwRef, true))
                return ForBindingMode::ReferenceMutable;
            if (match(TokenType::kwView, true))
                return ForBindingMode::ReferenceView;
            if (match(TokenType::kwConst))
                utError("'const' is not supported in loop bindings. Use 'let', 'mut', 'ref', or 'view'.", advance().loc);

            return ForBindingMode::ValueImmutable;
        };

        bindingModes.push_back(parseBindingMode());
        bindings.push_back(makeNodePtr<Identifier>(consume(TokenType::identifier)));
        while (match(TokenType::opBitOr, true))
        {
            bindingModes.push_back(parseBindingMode());
            bindings.push_back(makeNodePtr<Identifier>(consume(TokenType::identifier)));
        }

        consume(TokenType::kwIn);

        NodePtr<Expression> iterable = parseExpression();
        NodePtr<Expression> step = nullptr;

        if (match(TokenType::kwStep, true))
            step = parseExpression();

        if (hasParen)
            consume(TokenType::rightParen);

        NodePtr<Statement> body = match(TokenType::leftBrace) ? parseBlockStatement() : parseStatement();

        return makeNodePtr<ForInStatement>(std::move(bindings), std::move(bindingModes), std::move(iterable), std::move(step), std::move(body), startLoc);
    }

    NodePtr<Statement> Parser::parseCForStatement(common::Location startLoc)
    {
        NodePtr<Statement> initializer = nullptr;
        if (!match(TokenType::semicolon, true))
        {
            if (matchOneOf({ TokenType::kwLet, TokenType::kwMut, TokenType::kwConst }))
            {
                Token startTok = advance();
                Mutability mutability;

                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (startTok.type)
                {
                case TokenType::kwLet:   mutability = Mutability::Immutable; break;
                case TokenType::kwMut:   mutability = Mutability::Mutable; break;
                case TokenType::kwConst: mutability = Mutability::Const; break;
                default:
                    utError("Unexpected for-loop initializer qualifier.", startTok.loc);
                }

                NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());

                NodePtr<TypeSpecifier> specifier = nullptr;
                if (match(TokenType::opColon, true))
                    specifier = parseType();

                NodePtr<Expression> value = nullptr;
                if (match(TokenType::opAssign, true))
                    value = parseExpression();

                validateOrdinaryVariableDeclaration(
                    mutability,
                    specifier != nullptr,
                    value != nullptr,
                    startTok.loc
                );

                initializer = makeNodePtr<VariableDeclaration>(
                    std::vector<NodePtr<AttributeStatement>>{},
                    mutability,
                    std::move(name),
                    std::move(specifier),
                    std::move(value),
                    false,
                    startTok.loc
                );
            }
            else
            {
                NodePtr<Expression> initExpr = parseExpression();
                initializer = makeNodePtr<ExpressionStatement>(std::move(initExpr), startLoc);
            }

            consume(TokenType::semicolon);
        }

        NodePtr<Expression> condition = nullptr;
        if (!match(TokenType::semicolon, true))
        {
            condition = parseExpression();
            consume(TokenType::semicolon);
        }

        NodePtr<Expression> increment = nullptr;
        if (!match(TokenType::rightParen, true))
        {
            increment = parseExpression();
            consume(TokenType::rightParen);
        }

        NodePtr<Statement> body = match(TokenType::leftBrace) ? parseBlockStatement() : parseStatement();
        return makeNodePtr<CForStatement>(std::move(initializer), std::move(condition), std::move(increment), std::move(body), startLoc);
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

    NodePtr<Statement> Parser::parseUseStatement()
    {
        Token startTok = consume(TokenType::kwUse);
        Location startLoc = startTok.loc;

        bool isStdLib = false;

        if (match(TokenType::atSign))
        {
            NodePtr<AttributeStatement> stmt = parseAttributeStatement();
            if (stmt->attribute != Attribute::CppHeader)
                utError("Use statement only accepts @CppHeader attribute.", startLoc);

            if (stmt->args.size() != 1 || stmt->args.front().type != TokenType::stringLiteral)
                utError("@CppHeader attribute needs 1 filepath parameter.", startLoc);

            consume(TokenType::semicolon);

            return makeNodePtr<UseStatement>("", stmt->args.front().value, "", false, true, false, startLoc);
        }
        
        std::vector<std::string> moduleParts;
        Location modulePathEndLoc = startLoc;

        bool isFirstPart = true;
        bool expectsPart = true;
        bool sawPathToken = false;
        while (expectsPart)
        {
            if (!matchIdentifier())
            {
                if (!sawPathToken)
                    utError("Use statement must include a module path.", startLoc);

                if (match(TokenType::semicolon))
                    utError("Unfinished use statement. Use statements should finish with a module name.", modulePathEndLoc);

                utError("Expected a module name after '::'.", currentOrPreviousLocation());
            }

            Token tok = consumeIdentifier();
            sawPathToken = true;
            modulePathEndLoc = tok.loc;

            if (tok.value == "super")
            {
                moduleParts.emplace_back("..");
            }
            else if (tok.value == "self")
            {
                if (!isFirstPart)
                    utError("'self' may appear only at the beginning of a use path.", tok.loc);
            }
            else if (tok.value == "std" && isFirstPart)
            {
                isStdLib = true;
            }
            else
            {
                moduleParts.push_back(tok.value);
            }

            isFirstPart = false;
            if (!match(TokenType::opScope))
            {
                expectsPart = false;
                break;
            }

            if (peek(1).type == TokenType::opStar)
                break;

            advance();
        }

        if (moduleParts.empty())
            utError("Use statement must include a module path.", startLoc);

        if (moduleParts.back() == "..")
            utError("Unfinished use statement. Use statements should finish with a module name.", modulePathEndLoc);

        std::string moduleName = moduleParts.back();
        std::string modulePath;
        std::string aliasName;
        bool importAllIntoScope = false;

        for (size_t i = 0; i < moduleParts.size(); ++i)
        {
            if (i > 0)
                modulePath.push_back('/');
            modulePath.append(moduleParts[i]);
        }

        if (match(TokenType::opScope, true))
        {
            consume(TokenType::opStar);
            importAllIntoScope = true;
        }

        if (match(TokenType::kwAs, true))
        {
            aliasName = consumeIdentifier().value;
        }

        consume(TokenType::semicolon);
        
        return makeNodePtr<UseStatement>(std::move(moduleName), std::move(modulePath), std::move(aliasName), isStdLib, false, importAllIntoScope, startLoc);
    }

    NodePtr<Statement> Parser::parseUsingStatement()
    {
        Token startTok = consume(TokenType::kwUsing);
        NodePtr<AttributeStatement> attribute = parseAttributeStatement(false);
        if (attribute->attribute == Attribute::CppHeader)
        {
            if (attribute->args.size() != 1 || attribute->args.front().type != TokenType::stringLiteral)
                utError("cpp::header expects exactly one string literal path.", startTok.loc);
            if (match(TokenType::leftBrace))
                utError("Bounded cpp::header scopes are not supported; place 'using cpp::header(...)' in the containing realm.", startTok.loc);
            consume(TokenType::semicolon);
            return makeNodePtr<UseStatement>("", attribute->args.front().value, "", false, true, false, startTok.loc);
        }

        NodePtr<DeclarationGroup> body = nullptr;
        if (match(TokenType::leftBrace))
        {
            consume(TokenType::leftBrace);
            std::vector<NodePtr<Statement>> declarations;
            while (peek().isValid() && !match(TokenType::rightBrace))
            {
                if (auto declaration = parseStatement(); declaration)
                    declarations.push_back(std::move(declaration));
            }
            consume(TokenType::rightBrace);
            body = makeNodePtr<DeclarationGroup>(std::move(declarations), startTok.loc);
        }
        else
        {
            consume(TokenType::semicolon);
        }
        return makeNodePtr<UsingAttributeStatement>(std::move(attribute), std::move(body), startTok.loc);
    }

    NodePtr<Statement> Parser::parseRealmDeclaration(std::vector<NodePtr<AttributeStatement>> attributes)
    {
        if (!attributes.empty())
            utError("Attributes are not supported on realm declarations yet.", attributes.front()->location());

        Token startTok = consume(TokenType::kwRealm);
        NodePtr<Identifier> name = makeNodePtr<Identifier>(consumeIdentifier());

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

    int Parser::getPrecedence(TokenType type)
    {
        // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
        switch (type)
        {
        // ---------------------------------
        // fit
        // ---------------------------------
        case TokenType::kwFit:
            return 15;
            
        // ---------------------------------
        // Postfix / access / call
        // ---------------------------------
        case TokenType::opDot:
        case TokenType::opScope:
        case TokenType::leftParen:    // call
        case TokenType::leftBracket:  // index
            return 14;
    
        // ---------------------------------
        // Prefix (unary)
        // ---------------------------------
        case TokenType::kwRef:
        case TokenType::kwAwait:
        case TokenType::kwSpawn:
        case TokenType::kwDetach:
        case TokenType::kwDeref:
        case TokenType::kwNot:        // not
        case TokenType::opLogicalNot: // !
        case TokenType::opBitNot:     // ~
            return 13;
    
        // ---------------------------------
        // Multiplicative
        // ---------------------------------
        case TokenType::opStar:
        case TokenType::opSlash:
        case TokenType::opPercent:
            return 12;
    
        // ---------------------------------
        // Additive
        // ---------------------------------
        case TokenType::opPlus:
        case TokenType::opMinus:
            return 11;
    
        // ---------------------------------
        // Shift
        // ---------------------------------
        case TokenType::opShiftLeft:
        case TokenType::opShiftRight:
            return 10;
            
        // ---------------------------------
        // Range
        // ---------------------------------
        case TokenType::opRangeInclusive:
        case TokenType::opRangeExclusive:
            return 9;
    
        // ---------------------------------
        // Relational
        // ---------------------------------
        case TokenType::opLess:
        case TokenType::opLessEqual:
        case TokenType::opGreater:
        case TokenType::opGreaterEqual:
        case TokenType::kwIn:
            return 8;
    
        // ---------------------------------
        // Equality
        // ---------------------------------
        case TokenType::opEqual:
        case TokenType::opNotEqual:
        case TokenType::kwIs:
            return 7;
    
        // ---------------------------------
        // Bitwise
        // ---------------------------------
        case TokenType::opBitAnd:
            return 6;
        case TokenType::opBitXor:
            return 5;
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

    void Parser::validateOrdinaryVariableDeclaration(Mutability mutability,
                                                     bool hasExplicitType,
                                                     bool hasInitializer,
                                                     Location location)
    {
        if (hasInitializer)
            return;

        if (mutability == Mutability::Const)
            ucError(location);

        if (!hasExplicitType)
            utError("A variable without an initializer must have an explicit type.", location);
    }

    void Parser::ucError(Location location)
    {
        WIO_LOG_ADD_ERROR(location, "Constants must be initialized.");
        throw UninitializedConstantError("Constants must be initialized.", location);
    }

    bool Parser::canParseExplicitTypeArgumentCall() const
    {
        if (peek().type != TokenType::opLess)
            return false;

        int angleDepth = 0;
        bool sawInnerToken = false;

        for (size_t index = currentTokenIndex_; index < tokens_.size(); ++index)
        {
            TokenType type = tokens_[index].type;

            if (type == TokenType::opLess)
            {
                ++angleDepth;
                continue;
            }

            const int closeCount = type == TokenType::opShiftRight ? 2 :
                                   type == TokenType::opGreater ? 1 : 0;
            if (closeCount != 0)
            {
                if (angleDepth == 0)
                    return false;

                angleDepth -= closeCount;
                if (angleDepth < 0)
                    return false;
                if (angleDepth == 0)
                {
                    if (!sawInnerToken || index + 1 >= tokens_.size())
                        return false;

                    const TokenType nextType = tokens_[index + 1].type;
                    if (nextType == TokenType::leftParen)
                        return true;

                    return ((nextType == TokenType::opLogicalNot) ||
                            (nextType == TokenType::opQuestion)) &&
                           index + 2 < tokens_.size() &&
                           tokens_[index + 2].type == TokenType::leftParen;
                }

                continue;
            }

            if (angleDepth == 0)
                return false;

            sawInnerToken = true;

            if (type == TokenType::semicolon ||
                type == TokenType::leftBrace ||
                type == TokenType::rightBrace)
            {
                return false;
            }
        }

        return false;
    }
}
