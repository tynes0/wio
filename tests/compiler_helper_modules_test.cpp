#include "wio/ast/attribute_queries.h"
#include "wio/ast/declaration_queries.h"
#include "wio/codegen/cpp_identifier.h"
#include "wio/sema/constant_evaluator.h"
#include "wio/sema/generic_support.h"
#include "wio/sema/type_queries.h"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }

    wio::Token integerToken(const std::string& value)
    {
        return wio::Token{.type = wio::TokenType::integerLiteral, .value = value};
    }
}

int main()
{
    using namespace wio;
    using namespace wio::common;
    using namespace wio::sema;

    bool ok = true;

    using namespace wio::codegen::cpp_identifier;
    ok &= expect(isCppReservedIdentifier("class"), "C++ keyword must be reserved");
    ok &= expect(isCppReservedIdentifier("reflexpr"), "reserved C++ reflection spelling must be rejected");
    ok &= expect(isValidCppIdentifier("player_score"), "ordinary C++ identifier must be accepted");
    ok &= expect(!isValidCppIdentifier("9lives"), "identifier cannot start with a digit");
    ok &= expect(isValidCppSymbolPath("engine::player_score", true), "qualified symbol must be accepted");
    ok &= expect(!isValidCppSymbolPath("engine::class", true), "reserved segment must reject a symbol path");
    ok &= expect(sanitizeCppIdentifier("class") == "_wio_class", "reserved identifier must be sanitized");

    std::string replacement = "T std::vector<T> Type TT";
    replaceCppIdentifier(replacement, "T", "i32");
    ok &= expect(
        replacement == "i32 std::vector<i32> Type TT",
        "identifier replacement must respect token boundaries");

    const Ref<Type> i32 = MakeRef<PrimitiveType>("i32");
    const Ref<Type> alias = MakeRef<AliasType>("Score", MakeRef<AliasType>("Value", i32));
    ok &= expect(type_queries::unwrapAliasType(alias).Get() == i32.Get(), "nested aliases must unwrap completely");
    ok &= expect(type_queries::isPrimitiveNamed(alias, "i32"), "primitive query must see through aliases");

    const Ref<Type> nestedReference = MakeRef<ReferenceType>(MakeRef<ReferenceType>(alias, false), true);
    ok &= expect(type_queries::shouldAutoReadReferenceType(nestedReference), "value references must auto-read");
    ok &= expect(
        type_queries::getAutoReadableReferenceDepth(nestedReference) == 2,
        "auto-read depth must include every reference layer");
    ok &= expect(
        type_queries::getAutoReadableType(nestedReference).Get() == i32.Get(),
        "auto-readable type must resolve references and aliases");

    using namespace wio::sema::generic_support;
    const auto absolute = tryParsePackElementBindingName("Args[3]");
    ok &= expect(
        absolute && absolute->packName == "Args" && absolute->value == 3 &&
            absolute->kind == PackElementBindingKind::Absolute,
        "absolute pack binding must parse");

    const auto fromEnd = tryParsePackElementBindingName("Args[last-2]");
    ok &= expect(
        fromEnd && fromEnd->value == 3 && fromEnd->kind == PackElementBindingKind::FromEnd,
        "tail-relative pack binding must parse");
    ok &= expect(
        fromEnd && tryResolveConcretePackElementIndex(*fromEnd, 6) == 3,
        "tail-relative pack binding must resolve against pack size");
    ok &= expect(!tryParsePackElementBindingName("Args[last-x]"), "invalid pack binding must be rejected");

    const Ref<Type> stringType = MakeRef<PrimitiveType>("string");
    const GenericBindingSet bindings = buildExtendedGenericBindings(
        {"Head", "Tail"}, true, {i32, stringType, alias});
    ok &= expect(bindings.directBindings.at("Head").Get() == i32.Get(), "fixed generic binding must be retained");
    ok &= expect(bindings.packBindings.at("Tail").size() == 2, "generic pack must retain every trailing type");
    ok &= expect(bindings.directBindings.contains("Tail[1]"), "generic pack elements must receive stable names");

    const auto one = makeNodePtr<IntegerLiteral>(integerToken("1"));
    const auto seven = makeNodePtr<IntegerLiteral>(integerToken("7"));
    const auto sum = makeNodePtr<BinaryExpression>(
        one,
        Token{.type = TokenType::opPlus, .value = "+"},
        seven);
    const auto product = makeNodePtr<BinaryExpression>(
        sum,
        Token{.type = TokenType::opStar, .value = "*"},
        makeNodePtr<IntegerLiteral>(integerToken("5")));
    ok &= expect(
        ConstExpressionEvaluator({}).evaluateInteger(product) == 40,
        "shared constant evaluator must preserve integer expression semantics");

    std::vector<NodePtr<AttributeStatement>> attributes{
        makeNodePtr<AttributeStatement>(Attribute::From, std::vector<Token>{Token{.value = "BaseA"}}),
        makeNodePtr<AttributeStatement>(Attribute::From, std::vector<Token>{Token{.value = "BaseB"}})
    };
    const auto attributeArguments = attribute_queries::getAllAttributeArgs(attributes, Attribute::From);
    ok &= expect(attribute_queries::hasAttribute(attributes, Attribute::From), "attribute lookup must find built-ins");
    ok &= expect(
        attributeArguments.size() == 2 && attributeArguments[0].value == "BaseA" &&
            attributeArguments[1].value == "BaseB",
        "repeatable attribute arguments must retain source order");
    ok &= expect(
        attribute_queries::getFirstAttributeArgs(attributes, Attribute::From).size() == 1,
        "first-attribute query must preserve code-generation lookup semantics");

    std::vector<Parameter> parameters;
    parameters.emplace_back();
    parameters.emplace_back(nullptr, nullptr, makeNodePtr<IntegerLiteral>(integerToken("0")));
    parameters.emplace_back(nullptr, nullptr, nullptr, true);
    const auto declaration = makeNodePtr<FunctionDeclaration>(
        std::vector<NodePtr<AttributeStatement>>{},
        nullptr,
        std::vector<NodePtr<Identifier>>{},
        false,
        std::move(parameters),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        common::Location{.file = "compiler_helper_modules_test", .line = 1, .column = 1});
    ok &= expect(declaration_queries::getFixedParameterCount(*declaration) == 2, "parameter pack must not be fixed");
    ok &= expect(declaration_queries::getRequiredParameterCount(*declaration) == 1, "defaults must reduce required arity");
    ok &= expect(declaration_queries::hasDefaultParameters(*declaration), "default parameter query must be shared");

    return ok ? 0 : 1;
}
