#include "wio/ast/ast.h"
#include "wio/sema/type.h"
#include "wio/sema/symbol.h"

namespace wio
{
    ASTNode::ASTNode() = default;
    
    ASTNode::ASTNode(common::Location loc)
        : loc(loc)
    {
    }

    ASTNode::~ASTNode() = default;

    Expression::Expression() = default;

    Expression::~Expression() = default;

    Statement::Statement() = default;
    
    Statement::~Statement() = default;

    Program::Program(std::vector<NodePtr<Statement>> _statements)
        : ASTNode(common::Location::invalid()), statements(std::move(_statements))
    {
    }

    Program::~Program() = default;

    TypeSpecifier::TypeSpecifier(Token _name, std::vector<NodePtrUnchecked<TypeSpecifier>> _generics, size_t size, bool _isMut, bool _isRef, common::Location _loc)
        : ASTNode(_loc), name(std::move(_name)), generics(std::move(_generics)), size(size), isMut(_isMut), isRef(_isRef)
    {
    }

    TypeSpecifier::~TypeSpecifier() = default;

    BinaryExpression::BinaryExpression(NodePtr<Expression> _left, Token _op, NodePtr<Expression> _right, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _op.loc), left(std::move(_left)), op(std::move(_op)), right(std::move(_right))
    {
    }

    BinaryExpression::~BinaryExpression() = default;

    UnaryExpression::UnaryExpression(Token _op, NodePtr<Expression> _operand, UnaryOperatorType _opType, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _op.loc), op(std::move(_op)), operand(std::move(_operand)), opType(_opType)
    {
    }

    UnaryExpression::~UnaryExpression() = default;

    AssignmentExpression::AssignmentExpression(NodePtr<Expression> _left, Token _op, NodePtr<Expression> _right, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _op.loc), left(std::move(_left)), op(std::move(_op)), right(std::move(_right))
    {
    }

    AssignmentExpression::~AssignmentExpression() = default;

    IntegerLiteral::IntegerLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    IntegerLiteral::~IntegerLiteral() = default;

    FloatLiteral::FloatLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    FloatLiteral::~FloatLiteral() = default;

    StringLiteral::StringLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    StringLiteral::~StringLiteral() = default;

    InterpolatedStringLiteral::InterpolatedStringLiteral(std::vector<NodePtr<Expression>> _parts, common::Location _loc)
        : Expression(_loc), parts(std::move(_parts))
    {
    }

    InterpolatedStringLiteral::~InterpolatedStringLiteral() = default;

    BoolLiteral::BoolLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    BoolLiteral::~BoolLiteral() = default;

    CharLiteral::CharLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    CharLiteral::~CharLiteral() = default;

    ByteLiteral::ByteLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    ByteLiteral::~ByteLiteral() = default;

    DurationLiteral::DurationLiteral(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    DurationLiteral::~DurationLiteral() = default;

    ArrayLiteral::ArrayLiteral(std::vector<NodePtr<Expression>> _elements, common::Location _loc)
        : Expression(_loc), elements(std::move(_elements))
    {
    }

    ArrayLiteral::~ArrayLiteral() = default;

    DictionaryLiteral::DictionaryLiteral(std::vector<std::pair<NodePtr<Expression>, NodePtr<Expression>>> _pairs, common::Location _loc)
        : Expression(_loc), pairs(std::move(_pairs))
    {
    }

    DictionaryLiteral::~DictionaryLiteral() = default;

    Identifier::Identifier(Token _token, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _token.loc), token(std::move(_token))
    {
    }

    Identifier::~Identifier() = default;

    NullExpression::NullExpression(common::Location _loc)
        : Expression(_loc)
    {
    }

    NullExpression::~NullExpression() = default;

    ArrayAccessExpression::ArrayAccessExpression(NodePtr<Expression> _object, NodePtr<Expression> _index, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _object->location()), object(std::move(_object)), index(std::move(_index))
    {
    }

    ArrayAccessExpression::~ArrayAccessExpression() = default;

    MemberAccessExpression::MemberAccessExpression(NodePtr<Expression> _object, NodePtr<Identifier> _member, TokenType _opType, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _object->location()), object(std::move(_object)), member(std::move(_member)), opType(_opType)
    {
    }

    MemberAccessExpression::~MemberAccessExpression() = default;

    FunctionCallExpression::FunctionCallExpression(NodePtr<Expression> _callee, std::vector<NodePtr<Expression>> _args, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _callee->location()), callee(std::move(_callee)), arguments(std::move(_args))
    {
    }

    FunctionCallExpression::~FunctionCallExpression() = default;

    RefExpression::RefExpression(bool _isMut, NodePtr<Expression> _operand, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _operand->location()), isMut(_isMut), operand(std::move(_operand))
    {
    }

    RefExpression::~RefExpression() = default;

    FitExpression::FitExpression(NodePtr<Expression> _operand, NodePtrUnchecked<TypeSpecifier> _targetType, common::Location _loc)
        : Expression(_loc.isValid() ? _loc : _operand->location()), operand(std::move(_operand)), targetType(std::move(_targetType))
    {
    }

    FitExpression::~FitExpression() = default;
    
    ExpressionStatement::ExpressionStatement(NodePtr<Expression> _expression, common::Location _loc)
        : Statement(_loc.isValid() ? _loc : _expression->location()), expression(std::move(_expression))
    {
    }

    ExpressionStatement::~ExpressionStatement() = default;

    VariableDeclaration::VariableDeclaration(Mutability _mutability, NodePtr<Identifier> _name,
        NodePtr<TypeSpecifier> _type, NodePtr<Expression> _init, common::Location _loc)
        : Statement(_loc), mutability(_mutability), name(std::move(_name)),
        type(std::move(_type)), initializer(std::move(_init))
    {
    }

    VariableDeclaration::~VariableDeclaration() = default;

    FunctionDeclaration::FunctionDeclaration(NodePtr<Identifier> _name, std::vector<Parameter> _params,
        NodePtr<TypeSpecifier> _retType, std::vector<NodePtr<Expression>> _guards, NodePtr<Statement> _body,
        common::Location _loc)
        : Statement(_loc), name(std::move(_name)), parameters(std::move(_params)),
        returnType(std::move(_retType)), guards(std::move(_guards)),  body(std::move(_body))
    {
    }

    FunctionDeclaration::~FunctionDeclaration() = default;

    BlockStatement::BlockStatement(std::vector<NodePtr<Statement>> _statements, common::Location _loc)
        : Statement(_loc), statements(std::move(_statements))
    {
    }

    BlockStatement::~BlockStatement() = default;

    IfStatement::IfStatement(NodePtr<Expression> _cond, NodePtr<Statement> _then, NodePtr<Statement> _else, common::Location _loc)
        : Statement(_loc), condition(std::move(_cond)), thenBranch(std::move(_then)), elseBranch(std::move(_else))
    {
    }

    IfStatement::~IfStatement() = default;

    WhileStatement::WhileStatement(NodePtr<Expression> _cond, NodePtr<Statement> _body, common::Location _loc)
        : Statement(_loc), condition(std::move(_cond)), body(std::move(_body))
    {
    }

    WhileStatement::~WhileStatement() = default;

    ReturnStatement::ReturnStatement(NodePtr<Expression> _value, common::Location _loc)
        : Statement(_loc), value(std::move(_value))
    {
    }

    ReturnStatement::~ReturnStatement() = default;

    UseStatement::UseStatement(std::string _moduleName, std::string _modulePath, bool _isStdLib, common::Location _loc)
        : Statement(_loc), moduleName(std::move(_moduleName)), modulePath(std::move(_modulePath)), isStdLib(_isStdLib)
    {
    }

    UseStatement::~UseStatement() = default;

    AttributeStatement::AttributeStatement(Attribute _attribute, common::Location _loc)
        : Statement(_loc), attribute(_attribute)
    {
    }

    AttributeStatement::~AttributeStatement() = default;
}
