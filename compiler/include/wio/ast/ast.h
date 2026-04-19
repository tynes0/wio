#pragma once

/*
 * Note: AST uses the x macro and the ast_nodes.def file instead of using Frenum.
 * This is because all AST nodes can be easily defined in a single file. Additionally,
 * the `#include "ast_nodes.def"` keyword cannot be used within Frenum macros.
 */

#include "wio/lexer/token.h"
#include "wio/common/location.h"
#include "wio/common/smart_ptr.h"
#include "ast_visitor.h"

#include <concepts>
#include <frenum.h>

#define WIO_NODE_BODY_1(K)                                      \
    static constexpr NodeKind KIND = NodeKind::K;               \
    NodeKind kind() const override { return KIND; }             \
    void accept(ASTVisitor& v) override { v.visit(*this); }

#define WIO_AST_NODE_BODY(K)         \
    protected:                       \
        using ASTNode::ASTNode;      \
    public:                          \
        WIO_NODE_BODY_1(K)

#define WIO_EXP_NODE_BODY(K)         \
    protected:                       \
        using Expression::Expression;\
    public:                          \
        WIO_NODE_BODY_1(K)


#define WIO_STMT_NODE_BODY(K)       \
    protected:                      \
        using Statement::Statement; \
    public:                         \
        WIO_NODE_BODY_1(K)

namespace wio
{
    namespace sema
    {
        struct Type;
        struct Symbol;
    }
    
    enum class NodeKind : uint8_t
    {
        Unknown,
#define X(item) item,
#include "ast_nodes.def"
#undef X
    };
    
    inline std::string_view getKindName(NodeKind kind)
    {
        switch (kind)
        {
#define X(item) case NodeKind::item: return #item;
#include "ast_nodes.def"
#undef X
        case NodeKind::Unknown:
            return "";
        }
        return "";
    }
    inline std::string getKindNameStr(NodeKind kind)
    {
        return std::string(getKindName(kind));
    }

    FrenumClassInNamespace(wio, Mutability, uint8_t,
        Immutable, // let
        Mutable,   // mut
        Const      // const
    );

    FrenumClassInNamespace(wio, Attribute ,uint8_t,
        Unknown,
        ReadOnly,
        Default,
        NoDefaultCtor,
        GenerateCtors,
        From,
        Trust,
        Final,
        Type,
        Native,
        CppHeader,
        CppName,
        Instantiate,
        Export,
        Command,
        Event,
        ModuleApiVersion,
        ModuleLoad,
        ModuleUpdate,
        ModuleUnload,
        ModuleSaveState,
        ModuleRestoreState
    );

    FrenumClassInNamespace(wio, AccessModifier, uint8_t,
        Public,
        Private,
        Protected,
        None
    );

    FrenumClassInNamespace(wio, ForBindingMode, uint8_t,
        ValueImmutable,
        ValueMutable,
        ReferenceMutable,
        ReferenceView
    );

    FrenumClassInNamespace(wio, IntrinsicMember, uint8_t,
        None,
        ArrayCount,
        ArrayEmpty,
        ArrayCapacity,
        ArrayContains,
        ArrayIndexOf,
        ArrayLastIndexOf,
        ArrayFirst,
        ArrayLast,
        ArrayGetOr,
        ArrayClone,
        ArraySlice,
        ArrayTake,
        ArraySkip,
        ArrayConcat,
        ArrayReversed,
        ArrayJoin,
        ArrayPush,
        ArrayPushFront,
        ArrayPop,
        ArrayPopFront,
        ArrayInsert,
        ArrayClear,
        ArrayRemoveAt,
        ArrayRemove,
        ArrayExtend,
        ArrayReserve,
        ArrayShrinkToFit,
        ArrayFill,
        ArrayReverse,
        ArraySort,
        ArraySorted,
        DictCount,
        DictEmpty,
        DictContainsKey,
        DictContainsValue,
        DictGet,
        DictGetOr,
        DictTryGet,
        DictSet,
        DictGetOrAdd,
        DictKeys,
        DictValues,
        DictClone,
        DictMerge,
        DictExtend,
        DictClear,
        DictRemove,
        TreeFirstKey,
        TreeFirstValue,
        TreeLastKey,
        TreeLastValue,
        TreeFloorKeyOr,
        TreeCeilKeyOr,
        StringCount,
        StringEmpty,
        StringContains,
        StringContainsChar,
        StringStartsWith,
        StringEndsWith,
        StringIndexOf,
        StringLastIndexOf,
        StringIndexOfChar,
        StringLastIndexOfChar,
        StringFirst,
        StringLast,
        StringGetOr,
        StringSlice,
        StringSliceFrom,
        StringTake,
        StringSkip,
        StringLeft,
        StringRight,
        StringTrim,
        StringTrimStart,
        StringTrimEnd,
        StringToLower,
        StringToUpper,
        StringReplace,
        StringReplaceFirst,
        StringRepeat,
        StringSplit,
        StringLines,
        StringPadLeft,
        StringPadRight,
        StringReversed,
        StringAppend,
        StringPush,
        StringInsert,
        StringErase,
        StringClear,
        StringReverse,
        StringReplaceInPlace,
        StringTrimInPlace,
        StringToLowerInPlace,
        StringToUpperInPlace
    );

    struct ASTNode;
    struct Expression;
    struct Statement;

    template <typename T, typename = void>
    struct Has_KIND : std::false_type {};

    template <typename T>
    struct Has_KIND<T, std::void_t<decltype(T::KIND)>>
        : std::bool_constant<std::is_same_v<std::remove_cv_t<decltype(T::KIND)>, NodeKind>> {};
    
    template <typename T>
    concept ASTType = (std::derived_from<T, ASTNode> &&
        (Has_KIND<T>::value || std::same_as<T, Expression> || std::same_as<T, Statement>));

    template <typename T>
    requires ASTType<T>
    using NodePtr = Ref<T>;
    
    template <typename T>
    using NodePtrUnchecked = Ref<T>;

    template <typename T, typename... Args>
    NodePtr<T> makeNodePtr(Args ...args)
    {
        return Ref<T>::Create(std::forward<Args>(args)...);
    }
    
    struct ASTNode : RefCountedObject
    {
    protected:
        ASTNode();
        explicit ASTNode(common::Location loc);
        common::Location loc;
        
    public:
        WeakRef<sema::Type> refType = nullptr;
        
        ASTNode(const ASTNode&) = delete;
        ASTNode& operator=(const ASTNode&) = delete;
        ASTNode(ASTNode&&) = default;
        ASTNode& operator=(ASTNode&&) = default;

        ~ASTNode() override;
        virtual void accept(ASTVisitor& v) = 0;
        virtual NodeKind kind() const = 0;
        
        const common::Location& location() const { return loc; }
        std::string_view kindName() const { return getKindName(kind()); }
        std::string kindNameStr() const { return getKindNameStr(kind()); }

        bool is(NodeKind k) const { return kind() == k; }

        template <ASTType T>
        bool is() const { return kind() == T::KIND; }

        template <ASTType T>
        T* as() { return (is<T>()) ? static_cast<T*>(this) : nullptr; }

        template <ASTType T>
        const T* as() const { return (is<T>()) ? static_cast<const T*>(this) : nullptr; }
    };

    // NOLINTBEGIN(cppcoreguidelines-special-member-functions)
    
    struct Expression : ASTNode
    {
    protected:
        using ASTNode::ASTNode;
    public:
        Expression();
        ~Expression() override;
        
        WeakRef<sema::Symbol> referencedSymbol = nullptr;
    };

    struct Statement : ASTNode
    {
    protected:
        using ASTNode::ASTNode;
    public:
        Statement();
        ~Statement() override;
    };

    struct Program : ASTNode
    {
        WIO_AST_NODE_BODY(Program)

        explicit Program(std::vector<NodePtr<Statement>> _statements);
        ~Program() override;
        
        std::vector<NodePtr<Statement>> statements;
    };

    struct TypeSpecifier : ASTNode
    {
        WIO_AST_NODE_BODY(TypeSpecifier)

        Token name; // Type name (Ex: Result)
        std::vector<NodePtrUnchecked<TypeSpecifier>> generics; // Generic parameters (Ex: <Texture>)
        size_t size = 0;

        bool isMut = false;
        bool isRef = false;

        TypeSpecifier(Token _name, std::vector<NodePtrUnchecked<TypeSpecifier>> _generics, size_t size, bool _isMut, bool _isRef, common::Location _loc);
        ~TypeSpecifier() override;
    };

    struct BinaryExpression : Expression
    {
        WIO_EXP_NODE_BODY(BinaryExpression)
        
        NodePtr<Expression> left;
        Token op;
        NodePtr<Expression> right;
        
        BinaryExpression(NodePtr<Expression> _left, Token _op, NodePtr<Expression> _right, common::Location _loc = common::Location::invalid());
        ~BinaryExpression() override;
    };

    struct UnaryExpression : Expression
    {
        WIO_EXP_NODE_BODY(UnaryExpression)
        
        enum class UnaryOperatorType : uint8_t { Prefix, Postfix };

        Token op;
        NodePtr<Expression> operand;
        UnaryOperatorType opType;

        UnaryExpression(Token _op, NodePtr<Expression> _operand, UnaryOperatorType _opType = UnaryOperatorType::Prefix, common::Location _loc = common::Location::invalid());
        ~UnaryExpression() override;
    };

    struct AssignmentExpression : Expression
    {
        WIO_EXP_NODE_BODY(AssignmentExpression)
        
        NodePtr<Expression> left;
        Token op;
        NodePtr<Expression> right;
        
        AssignmentExpression(NodePtr<Expression> _left, Token _op, NodePtr<Expression> _right, common::Location _loc = common::Location::invalid());
        ~AssignmentExpression() override;
    };

    struct IntegerLiteral : Expression
    {
        WIO_EXP_NODE_BODY(IntegerLiteral)
        
        Token token;

        explicit IntegerLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~IntegerLiteral() override;
    };

    struct FloatLiteral : Expression
    {
        WIO_EXP_NODE_BODY(FloatLiteral)
        
        Token token;
        
        FloatLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~FloatLiteral() override;
    };

    struct StringLiteral : Expression
    {
        WIO_EXP_NODE_BODY(StringLiteral)

        Token token;
        
        explicit StringLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~StringLiteral() override;
    };

    struct InterpolatedStringLiteral : Expression
    {
        WIO_EXP_NODE_BODY(InterpolatedStringLiteral)

        std::vector<NodePtr<Expression>> parts;
        
        explicit InterpolatedStringLiteral(std::vector<NodePtr<Expression>> _parts, common::Location _loc = common::Location::invalid());
        ~InterpolatedStringLiteral() override;
    };

    struct BoolLiteral : Expression
    {
        WIO_EXP_NODE_BODY(BoolLiteral)
        
        Token token;
        
        explicit BoolLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~BoolLiteral() override;
    };

    struct CharLiteral : Expression
    {
        WIO_EXP_NODE_BODY(CharLiteral)
        
        Token token;
        
        explicit CharLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~CharLiteral() override;
    };

    struct ByteLiteral : Expression
    {
        WIO_EXP_NODE_BODY(ByteLiteral)
        
        Token token;
        
        explicit ByteLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~ByteLiteral() override;
    };

    struct DurationLiteral : Expression
    {
        WIO_EXP_NODE_BODY(DurationLiteral)
        
        Token token;
        
        explicit DurationLiteral(Token _token, common::Location _loc = common::Location::invalid());
        ~DurationLiteral() override;
        
    };

    struct ArrayLiteral : Expression
    {
        WIO_EXP_NODE_BODY(ArrayLiteral)
        
        std::vector<NodePtr<Expression>> elements;
        
        explicit ArrayLiteral(std::vector<NodePtr<Expression>> _elements, common::Location _loc = common::Location::invalid());
        ~ArrayLiteral() override;
    };

    struct DictionaryLiteral : Expression
    {
        WIO_EXP_NODE_BODY(DictionaryLiteral)
        
        std::vector<std::pair<NodePtr<Expression>, NodePtr<Expression>>> pairs;
        bool isOrdered;

        explicit DictionaryLiteral(std::vector<std::pair<NodePtr<Expression>, NodePtr<Expression>>> _pairs,
            bool _isOrdered, common::Location _loc = common::Location::invalid());
        ~DictionaryLiteral() override;
    };

    struct Identifier : Expression
    {
        WIO_EXP_NODE_BODY(Identifier)
        
        Token token;

        explicit Identifier(Token _token, common::Location _loc = common::Location::invalid());
        ~Identifier() override;
    };

    struct NullExpression : Expression
    {
        WIO_EXP_NODE_BODY(NullExpression)
        
        explicit NullExpression(common::Location _loc = common::Location::invalid());
        ~NullExpression() override;
    };

    struct ArrayAccessExpression : Expression
    {
        WIO_EXP_NODE_BODY(ArrayAccessExpression)

        NodePtr<Expression> object;
        NodePtr<Expression> index;

        ArrayAccessExpression(NodePtr<Expression> _object, NodePtr<Expression> _index, common::Location _loc = common::Location::invalid());
        ~ArrayAccessExpression() override;
    };

    struct MemberAccessExpression : Expression
    {
        WIO_EXP_NODE_BODY(MemberAccessExpression)

        NodePtr<Expression> object;
        NodePtr<Identifier> member;
        TokenType opType;
        IntrinsicMember intrinsicMember = IntrinsicMember::None;

        MemberAccessExpression(NodePtr<Expression> _object, NodePtr<Identifier> _member, TokenType _opType, common::Location _loc = common::Location::invalid());
        ~MemberAccessExpression() override;
    };

    struct FunctionCallExpression : Expression
    {
        WIO_EXP_NODE_BODY(FunctionCallExpression)

        NodePtr<Expression> callee;
        std::vector<NodePtr<TypeSpecifier>> explicitTypeArguments;
        std::vector<NodePtr<Expression>> arguments;

        FunctionCallExpression(NodePtr<Expression> _callee, std::vector<NodePtr<TypeSpecifier>> _explicitTypeArguments, std::vector<NodePtr<Expression>> _args, common::Location _loc = common::Location::invalid());
        ~FunctionCallExpression() override;
    };

    struct Parameter
    {
        NodePtr<Identifier> name;
        NodePtr<TypeSpecifier> type;
    };

    struct LambdaExpression : Expression
    {
        WIO_EXP_NODE_BODY(LambdaExpression)

        std::vector<Parameter> parameters;
        NodePtr<TypeSpecifier> returnType;
        NodePtr<Statement> body;

        LambdaExpression(std::vector<Parameter> _params, NodePtr<TypeSpecifier> _retType, NodePtr<Statement> _body,
            common::Location _loc = common::Location::invalid());
        ~LambdaExpression() override;
    };

    struct RefExpression : Expression
    {
        WIO_EXP_NODE_BODY(RefExpression)

        bool isMut; // ref mut x -> true, ref x -> false
        NodePtr<Expression> operand;

        RefExpression(bool _isMut, NodePtr<Expression> _operand, common::Location _loc);
        ~RefExpression() override;
    };

    struct FitExpression : Expression
    {
        WIO_EXP_NODE_BODY(FitExpression)

        NodePtr<Expression> operand;
        NodePtrUnchecked<TypeSpecifier> targetType;

        FitExpression(NodePtr<Expression> _operand, NodePtrUnchecked<TypeSpecifier> _targetType, common::Location _loc);
        ~FitExpression() override;
    };

    struct SelfExpression : Expression
    {
        WIO_EXP_NODE_BODY(SelfExpression)
        
        explicit SelfExpression(common::Location _loc = common::Location::invalid());
        ~SelfExpression() override;
    };

    struct SuperExpression : Expression
    {
        WIO_EXP_NODE_BODY(SuperExpression)
        
        explicit SuperExpression(common::Location _loc = common::Location::invalid());
        ~SuperExpression() override;
    };

    struct RangeExpression : Expression
    {
        WIO_EXP_NODE_BODY(RangeExpression)

        NodePtr<Expression> start;
        NodePtr<Expression> end;
        bool isInclusive; // true '...', false '..<'

        RangeExpression(NodePtr<Expression> _start, NodePtr<Expression> _end, bool _isInclusive, common::Location _loc = common::Location::invalid());
        ~RangeExpression() override;
    };

    struct MatchCase
    {
        std::vector<NodePtr<Expression>> matchValues;
        NodePtr<Statement> body;
    };

    struct MatchExpression : Expression
    {
        WIO_EXP_NODE_BODY(MatchExpression)

        NodePtr<Expression> value;
        std::vector<MatchCase> cases;

        MatchExpression(NodePtr<Expression> _value, std::vector<MatchCase> _cases, common::Location _loc = common::Location::invalid());
        ~MatchExpression() override;
    };

    struct ExpressionStatement : Statement
    {
        WIO_STMT_NODE_BODY(ExpressionStatement)

        NodePtr<Expression> expression;
        
        explicit ExpressionStatement(NodePtr<Expression> _expression, common::Location _loc = common::Location::invalid());
        ~ExpressionStatement() override;
    };

    struct AttributeStatement : Statement
    {
        WIO_STMT_NODE_BODY(AttributeStatement)

        Attribute attribute;
        std::vector<Token> args;
        std::vector<NodePtr<TypeSpecifier>> typeArgs;

        AttributeStatement(Attribute _attribute, std::vector<Token> _args, std::vector<NodePtr<TypeSpecifier>> _typeArgs = {}, common::Location _loc = common::Location::invalid());
        ~AttributeStatement() override;
    };

    struct VariableDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(VariableDeclaration)

        std::vector<NodePtr<AttributeStatement>> attributes;
        Mutability mutability; // let, mut, const
        NodePtr<Identifier> name;
        NodePtr<TypeSpecifier> type;
        NodePtr<Expression> initializer;
        
        VariableDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, Mutability _mutability,
            NodePtr<Identifier> _name, NodePtr<TypeSpecifier> _type, NodePtr<Expression> _init, common::Location _loc);
        ~VariableDeclaration() override;
    };

    struct TypeAliasDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(TypeAliasDeclaration)

        NodePtr<Identifier> name;
        std::vector<NodePtr<Identifier>> genericParameters;
        NodePtr<TypeSpecifier> aliasedType;

        TypeAliasDeclaration(NodePtr<Identifier> _name,
            std::vector<NodePtr<Identifier>> _genericParameters,
            NodePtr<TypeSpecifier> _aliasedType,
            common::Location _loc);
        ~TypeAliasDeclaration() override;
    };
    
    struct FunctionDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(FunctionDeclaration)

        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;
        std::vector<NodePtr<Identifier>> genericParameters;
        std::vector<Parameter> parameters;
        NodePtr<TypeSpecifier> returnType;
        NodePtr<Expression> whenCondition; 
        NodePtr<Expression> whenFallback;
        NodePtr<Statement> body;

        FunctionDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name,
            std::vector<NodePtr<Identifier>> _genericParameters, std::vector<Parameter> _params, NodePtr<TypeSpecifier> _retType, NodePtr<Expression> _whenCondition,
            NodePtr<Expression> _whenFallback, NodePtr<Statement> _body, common::Location _loc);
        ~FunctionDeclaration() override;
    };

    struct InterfaceDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(InterfaceDeclaration)

        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;
        std::vector<NodePtr<Identifier>> genericParameters;
        std::vector<NodePtr<FunctionDeclaration>> methods;

        InterfaceDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name,
            std::vector<NodePtr<Identifier>> _genericParameters, std::vector<NodePtr<FunctionDeclaration>> _methods, common::Location _loc);
        ~InterfaceDeclaration() override;
    };

    struct ComponentMember
    {
        std::vector<NodePtr<AttributeStatement>> attributes;
        AccessModifier access;
        NodePtr<Statement> declaration; 
    };

    struct ComponentDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(ComponentDeclaration)

        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;
        std::vector<NodePtr<Identifier>> genericParameters;
        std::vector<ComponentMember> members;

        ComponentDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name,
            std::vector<NodePtr<Identifier>> _genericParameters, std::vector<ComponentMember> _members, common::Location _loc);
        ~ComponentDeclaration() override;
    };

    struct ObjectMember
    {
        std::vector<NodePtr<AttributeStatement>> attributes;
        AccessModifier access; 
        NodePtr<Statement> declaration; 
    };

    struct ObjectDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(ObjectDeclaration)

        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;
        std::vector<NodePtr<Identifier>> genericParameters;
        std::vector<ObjectMember> members;

        ObjectDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name,
            std::vector<NodePtr<Identifier>> _genericParameters, std::vector<ObjectMember> _members, common::Location _loc);
        ~ObjectDeclaration() override;
    };

    struct EnumMember
    {
        NodePtr<Identifier> name;
        NodePtr<Expression> value;
    };

    struct EnumDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(EnumDeclaration)
        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;
        std::vector<EnumMember> members;

        EnumDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name, std::vector<EnumMember> _members, common::Location _loc = common::Location::invalid());
        ~EnumDeclaration() override;
    };

    struct FlagsetDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(FlagsetDeclaration)
        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;
        std::vector<EnumMember> members;

        FlagsetDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name, std::vector<EnumMember> _members, common::Location _loc = common::Location::invalid());
        ~FlagsetDeclaration() override;
    };

    struct FlagDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(FlagDeclaration)
        std::vector<NodePtr<AttributeStatement>> attributes;
        NodePtr<Identifier> name;

        FlagDeclaration(std::vector<NodePtr<AttributeStatement>> _attributes, NodePtr<Identifier> _name, common::Location _loc = common::Location::invalid());
        ~FlagDeclaration() override;
    };
    
    struct BlockStatement : Statement
    {
        WIO_STMT_NODE_BODY(BlockStatement)
        
        std::vector<NodePtr<Statement>> statements;
        
        explicit BlockStatement(std::vector<NodePtr<Statement>> _statements, common::Location _loc = common::Location::invalid());
        ~BlockStatement() override;
    };

    struct IfStatement : Statement
    {
        WIO_STMT_NODE_BODY(IfStatement)

        NodePtr<Expression> condition;
        NodePtr<Statement> thenBranch;
        NodePtr<Statement> elseBranch;
        Token matchVar;

        IfStatement(NodePtr<Expression> _cond, NodePtr<Statement> _then, NodePtr<Statement> _else, Token _matchVar, common::Location _loc);
        ~IfStatement() override;
    };

    struct WhileStatement : Statement
    {
        WIO_STMT_NODE_BODY(WhileStatement)

        NodePtr<Expression> condition;
        NodePtr<Statement> body;

        WhileStatement(NodePtr<Expression> _cond, NodePtr<Statement> _body, common::Location _loc);
        ~WhileStatement() override;
    };

    struct ForInStatement : Statement
    {
        WIO_STMT_NODE_BODY(ForInStatement)

        std::vector<NodePtr<Identifier>> bindings;
        std::vector<ForBindingMode> bindingModes;
        std::vector<std::string> bindingAccessors;
        NodePtr<Expression> iterable;
        NodePtr<Expression> step;
        NodePtr<Statement> body;

        ForInStatement(std::vector<NodePtr<Identifier>> _bindings, std::vector<ForBindingMode> _bindingModes, NodePtr<Expression> _iterable, NodePtr<Expression> _step, NodePtr<Statement> _body, common::Location _loc);
        ~ForInStatement() override;
    };

    struct CForStatement : Statement
    {
        WIO_STMT_NODE_BODY(CForStatement)

        NodePtr<Statement> initializer;
        NodePtr<Expression> condition;
        NodePtr<Expression> increment;
        NodePtr<Statement> body;

        CForStatement(NodePtr<Statement> _initializer, NodePtr<Expression> _condition, NodePtr<Expression> _increment, NodePtr<Statement> _body, common::Location _loc);
        ~CForStatement() override;
    };

    struct BreakStatement : Statement
    {
        WIO_STMT_NODE_BODY(BreakStatement)
        
        explicit BreakStatement(common::Location _loc = common::Location::invalid());
        ~BreakStatement() override;
    };

    struct ContinueStatement : Statement
    {
        WIO_STMT_NODE_BODY(ContinueStatement)
        
        explicit ContinueStatement(common::Location _loc = common::Location::invalid());
        ~ContinueStatement() override;
    };
    
    struct ReturnStatement : Statement
    {
        WIO_STMT_NODE_BODY(ReturnStatement)

        NodePtr<Expression> value;

        explicit ReturnStatement(NodePtr<Expression> _value, common::Location _loc = common::Location::invalid());
        ~ReturnStatement() override;
    };

    struct UseStatement : Statement
    {
        WIO_STMT_NODE_BODY(UseStatement)

        std::string moduleName;
        std::string modulePath;
        std::string aliasName;
        std::vector<std::string> importedSymbols;
        bool isStdLib = false;
        bool isCppHeader = false;
        
        explicit UseStatement(std::string _moduleName, std::string _modulePath, std::string _aliasName, bool _isStdLib, bool _isCppHeader, common::Location _loc = common::Location::invalid());
        ~UseStatement() override;
    };

    struct RealmDeclaration : Statement
    {
        WIO_STMT_NODE_BODY(RealmDeclaration)

        NodePtr<Identifier> name;
        std::vector<NodePtr<Statement>> statements;

        RealmDeclaration(NodePtr<Identifier> _name, std::vector<NodePtr<Statement>> _statements, common::Location _loc = common::Location::invalid());
        ~RealmDeclaration() override;
    };
    
    // NOLINTEND(cppcoreguidelines-special-member-functions)
}

#undef WIO_NODE_BODY_1
#undef WIO_AST_NODE_BODY
#undef WIO_EXP_NODE_BODY
#undef WIO_STMT_NODE_BODY
