#include "wio/sema/analyzer.h"

#include "wio/common/exception.h"
#include "wio/common/logger.h"
#include "wio/common/utility.h"

#include "compiler.h"
#include "wio/runtime/loader.h"
#include "wio/runtime/std_loaders/io_loader.h"

namespace wio::sema
{
    namespace
    {
        Ref<Type> resolvePrimitiveType(const std::string& name)
        {
            auto& ctx = Compiler::get().getTypeContext();
        
            if (name == "i8") return ctx.getI8();
            if (name == "i16") return ctx.getI16();
            if (name == "i32") return ctx.getI32();
            if (name == "i64") return ctx.getI64();
            if (name == "u8") return ctx.getU8();
            if (name == "u16") return ctx.getU16();
            if (name == "u32") return ctx.getU32();
            if (name == "u64") return ctx.getU64();
            if (name == "f32") return ctx.getF32();
            if (name == "f64") return ctx.getF64();
            if (name == "bool") return ctx.getBool();
            if (name == "char") return ctx.getChar();
            if (name == "string") return ctx.getString();
            if (name == "void") return ctx.getVoid();

            return nullptr; 
        }

        bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            return std::ranges::any_of(attributes, [targetAttr](const auto& attr) { return attr->attribute == targetAttr; });
        }

        std::vector<Token> getAttributeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            for (const auto& attr : attributes) {
                if (attr->attribute == targetAttr) return attr->args;
            }
            return {};
        }
    }
    
    SemanticAnalyzer::SemanticAnalyzer() = default;

    SemanticAnalyzer::~SemanticAnalyzer()
    {
        for (auto& sym : symbols_)
        {
            if (sym)
            {
                sym->innerScope = nullptr;
                sym->overloads.clear();
            }
        }
        
        for (auto& scope : scopes_)
        {
            if (scope)
            {
                scope->clear();
            }
        }
    }

    void SemanticAnalyzer::analyze(const Ref<Program>& program)
    {
        scopes_.clear();
        symbols_.clear();
        currentScope_ = nullptr;
        
        program->accept(*this);

        WIO_LOG_PROCESS_WARNINGS();
        WIO_LOG_PROCESS_ERRORS(CompilationError);
    }

    void SemanticAnalyzer::enterScope(ScopeKind kind)
    {
        auto scope = Ref<Scope>::Create(currentScope_, kind);
        currentScope_ = scope;
        scopes_.push_back(std::move(scope));
    }

    void SemanticAnalyzer::exitScope()
    {
        if (currentScope_)
            currentScope_ = currentScope_->getParent().Lock();
    }

    Ref<Symbol> SemanticAnalyzer::createSymbol(std::string name, Ref<Type> type, SymbolKind kind, common::Location loc, SymbolFlags flags)
    {
        auto symbol = Ref<Symbol>::Create(std::move(name), std::move(type), kind, flags, loc);
        symbols_.push_back(symbol);
        return symbol;
    }

    void SemanticAnalyzer::visit(Program& node)
    {
        enterScope(ScopeKind::Global);

        isDeclarationPass_ = true;
        for (auto& stmt : node.statements)
            stmt->accept(*this);

        isDeclarationPass_ = false;
        isStructResolutionPass_ = true;
        for (auto& stmt : node.statements)
        {
            if (stmt->is<ComponentDeclaration>() || 
                stmt->is<ObjectDeclaration>() ||
                stmt->is<EnumDeclaration>() ||
                stmt->is<FlagsetDeclaration>() ||
                stmt->is<FlagDeclaration>())
            {
                stmt->accept(*this);
            }
        }
        
        isStructResolutionPass_ = false;
        for (auto& stmt : node.statements)
            stmt->accept(*this);
        
        exitScope();
    }

    void SemanticAnalyzer::visit(BlockStatement& node)
    {
        if (isDeclarationPass_) return;
        enterScope(ScopeKind::Block);

        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }

        exitScope();
    }

    void SemanticAnalyzer::visit(TypeSpecifier& node)
    {
        if (node.name.type == TokenType::kwFn)
        {
            node.generics[0]->accept(*this);
            auto retType = node.generics[0]->refType.Lock();
            
            std::vector<Ref<Type>> paramTypes;
            for (size_t i = 1; i < node.generics.size(); ++i)
            {
                node.generics[i]->accept(*this);
                paramTypes.push_back(node.generics[i]->refType.Lock());
            }
            
            node.refType = Compiler::get().getTypeContext().getFunctionType(retType, paramTypes);
            return;
        }
        
        Ref<Type> type = resolvePrimitiveType(node.name.value);

        if (!type)
        {
            if (node.name.type == TokenType::StaticArray)
            {
                node.generics[0]->accept(*this);
                type = Compiler::get().getTypeContext().getArrayType(node.generics[0]->refType.Lock(), ArrayType::ArrayKind::Static, node.size);
            }
            else if (node.name.type == TokenType::DynamicArray)
            {
                node.generics[0]->accept(*this);
                type = Compiler::get().getTypeContext().getArrayType(node.generics[0]->refType.Lock(), ArrayType::ArrayKind::Dynamic);
            }
            else if (node.name.type == TokenType::kwRef || node.name.type == TokenType::kwView)
            {
                node.generics[0]->accept(*this);
                bool isMut = (node.name.type == TokenType::kwRef); 
                type = Compiler::get().getTypeContext().getReferenceType(node.generics[0]->refType.Lock(), isMut);
            }
            else if (node.name.type == TokenType::identifier)
            {
                if (auto sym = currentScope_->resolve(node.name.value))
                {
                    if (sym->kind == SymbolKind::Struct) 
                    {
                        type = sym->type;
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "'{}' is not a type.", node.name.value);
                    }
                }
            }
        }

        if (!type)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Unknown type: '{}'", node.name.value);
            type = Compiler::get().getTypeContext().getUnknown();
        }

        node.refType = type;
    }

    void SemanticAnalyzer::visit(BinaryExpression& node)
    {
        node.left->accept(*this);
        node.right->accept(*this);

        if (node.op.type == TokenType::kwIn && node.right->is<RangeExpression>())
        {
            auto leftType = node.left->refType.Lock();
            if (leftType && !leftType->isNumeric())
            {
                WIO_LOG_ADD_ERROR(node.location(), "The left operand of 'in' operator must be numeric when used with a range.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
            return; 
        }

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();

        if (!lhsType || !rhsType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (!lhsType->isCompatibleWith(rhsType))
        {
            WIO_LOG_ADD_ERROR(
                node.op.loc,
                "Type mismatch in binary operation '{}': Cannot operate on '{}' and '{}'.",
                node.op.value,
                lhsType->toString(),
                rhsType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.isComparison() ||
            node.op.type == TokenType::opLogicalAnd ||
            node.op.type == TokenType::opLogicalOr ||
            node.op.type == TokenType::kwAnd ||
            node.op.type == TokenType::kwOr ||
            node.op.type == TokenType::kwIn)
        {
            node.refType = Compiler::get().getTypeContext().getBool();
        }
        else
        {
            // Todo: In arithmetic operations (for now), the result type is the same as the type of the left operand.
            node.refType = lhsType;
        }
    }
    
    void SemanticAnalyzer::visit(UnaryExpression& node)
    {
        node.operand->accept(*this);
        Ref<Type> opType = node.operand->refType.Lock();

        if (!opType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.type == TokenType::kwNot || node.op.type == TokenType::opLogicalNot)
        {
            if (opType != Compiler::get().getTypeContext().getBool())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Logical NOT (!) operator requires boolean operand.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
        }
        else if (node.op.type == TokenType::opMinus)
        {
            if (!opType->isNumeric())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Unary minus (-) operator requires numeric operand.");
            }
            node.refType = opType;
        }
        else
        {
            // Others (bitwise not vs.) should return the same type for now.
            node.refType = opType;
        }
    }

    void SemanticAnalyzer::visit(AssignmentExpression& node)
    {
        node.left->accept(*this);
        node.right->accept(*this);

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();

        bool isCompatible = false;
        bool isAutoDeref = false;

        if (lhsType && rhsType)
        {
            isCompatible = lhsType->isCompatibleWith(rhsType);

            if (!isCompatible && lhsType->kind() == TypeKind::Reference)
            {
                Ref<Type> currentRef = lhsType;
                bool canMutate = true;

                while (currentRef && currentRef->kind() == TypeKind::Reference)
                {
                    auto rType = currentRef.AsFast<ReferenceType>();
                    
                    if (!rType->isMutable) canMutate = false;
                    
                    if (rType->referredType->isCompatibleWith(rhsType) || 
                       (rType->referredType->isNumeric() && rhsType->isNumeric())) // YENİ: Deref atamada sayısal esneklik
                    {
                        isAutoDeref = true;
                        if (!canMutate) WIO_LOG_ADD_ERROR(node.op.loc, "Cannot modify data through a read-only reference (view).");
                        else isCompatible = true;
                        break;
                    }
                    currentRef = rType->referredType;
                }
            }
        }

        if (lhsType && rhsType && !isCompatible)
        {
            if (lhsType->isNumeric() && rhsType->isNumeric())
            {
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.op.loc,
                    "Type mismatch in assignment: Cannot assign '{}' to '{}'.",
                    rhsType->toString(),
                    lhsType->toString()
                );
            }
        }

        if (!isAutoDeref)
        {
            if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
            {
                if (!referSym->flags.get_isMutable() && !referSym->flags.get_isReadOnly())
                {
                    WIO_LOG_ADD_ERROR(node.op.loc, "Cannot assign to immutable variable '{0}'. Hint: Declare it as 'mut {0}'.", referSym->name);
                }
                if (referSym->flags.get_isReadOnly())
                {
                    bool isInsideObject = false;
                    auto currentSearch = currentScope_;
                    while (currentSearch)
                    {
                        if (currentSearch->resolveLocally(referSym->name)) { isInsideObject = true; break; }
                        currentSearch = currentSearch->getParent().Lock();
                    }

                    if (!isInsideObject)
                        WIO_LOG_ADD_ERROR(node.op.loc, "Cannot modify @Readonly member '{0}' from outside its object.", referSym->name);
                }
            }
        }

        node.refType = Compiler::get().getTypeContext().getVoid(); 
    }
    
    void SemanticAnalyzer::visit(IntegerLiteral& node)
    {
        IntegerResult result = common::getInteger(node.token.value);
        
        if (!result.isValid)
            WIO_LOG_ADD_ERROR(node.location(), "Invalid integer literal or value out of bounds: '{}'", node.token.value);
        
        Ref<Type> type = Type::getTypeFromIntegerResult(result);
        node.refType = type;
    }
    
    void SemanticAnalyzer::visit(FloatLiteral& node)
    {
        FloatResult result = common::getFloat(node.token.value);

        if (!result.isValid)
            WIO_LOG_ADD_ERROR(node.location(), "Invalid float literal or value out of bounds: '{}'", node.token.value);
        
        Ref<Type> type = Type::getTypeFromFloatResult(result);
        node.refType = type;
    }
    
    void SemanticAnalyzer::visit(StringLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getString();
    }
    
    void SemanticAnalyzer::visit(InterpolatedStringLiteral& node)
    {
        for(auto& part : node.parts)
        {
            part->accept(*this);
        }
        node.refType = Compiler::get().getTypeContext().getString();
    }
    
    void SemanticAnalyzer::visit(BoolLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getBool();
    }
    
    void SemanticAnalyzer::visit(CharLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getChar();
    }
    
    void SemanticAnalyzer::visit(ByteLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getU8();
    }
    
    void SemanticAnalyzer::visit(DurationLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getF32();
    }
    
    void SemanticAnalyzer::visit(ArrayLiteral& node)
    {
        if (node.elements.empty())
        {
            // Todo: Empty array: We can make the type Unknown array for now, or we can expect a generic one.
            // Let's call it [Unknown] for now.
            node.refType = Compiler::get().getTypeContext().getArrayType(Compiler::get().getTypeContext().getUnknown(), ArrayType::ArrayKind::Static, 0);
            return;
        }

        node.elements[0]->accept(*this);
        Ref<Type> baseType = node.elements[0]->refType.Lock();

        for (size_t i = 1; i < node.elements.size(); ++i)
        {
            node.elements[i]->accept(*this);
            if (auto lockedType = node.elements[i]->refType.Lock(); lockedType)
            {
                if (!baseType->isCompatibleWith(lockedType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.elements[i]->location(),
                        "Array elements must be of the same type. Expected '{}', but found '{}'.",
                        baseType->toString(),
                        lockedType->toString()
                    );
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.elements[i]->location(), "Undefined element type in array.");
            }
        }

        node.refType = Compiler::get().getTypeContext().getArrayType(baseType, ArrayType::ArrayKind::Literal, node.elements.size());
    }
    
    void SemanticAnalyzer::visit(DictionaryLiteral& node)
    {
        // Todo:: implement
        node.refType = Compiler::get().getTypeContext().getUnknown();
    }
    
    void SemanticAnalyzer::visit(Identifier& node)
    {
        Ref<Symbol> sym = currentScope_->resolve(node.token.value);

        if (!sym)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Undefined symbol: '{}'", node.token.value);
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.referencedSymbol = sym;
        node.refType = sym->type;
    }
    
    void SemanticAnalyzer::visit(NullExpression& node)
    {
        node.refType = Compiler::get().getTypeContext().getNull();
    }

    void SemanticAnalyzer::visit(ArrayAccessExpression& node)
    {
        node.object->accept(*this);
        Ref<Type> objType = node.object->refType.Lock();

        node.index->accept(*this);
        Ref<Type> idxType = node.index->refType.Lock();

        if (objType->kind() != TypeKind::Array)
        {
            WIO_LOG_ADD_ERROR(node.object->location(), "Type '{}' is not an array and cannot be indexed.", objType->toString());
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        if (!idxType->isNumeric())
        {
            WIO_LOG_ADD_ERROR(node.index->location(), "Array index must be an integer.");
        }
        
        auto arrType = objType.AsFast<ArrayType>();
        node.refType = arrType->elementType;
    }
    
    void SemanticAnalyzer::visit(MemberAccessExpression& node)
    {
        node.object->accept(*this);

        Ref<Symbol> leftSymbol = node.object->referencedSymbol.Lock();
        Ref<Type> leftType = nullptr;
        Ref<Symbol> foundMember = nullptr;
        
        if (auto lockedType = node.object->refType.Lock(); lockedType)
        {
            bool isNamespace = (leftSymbol && leftSymbol->kind == SymbolKind::Namespace);

            if (!isNamespace && (!lockedType || lockedType->isUnknown()))
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
            leftType = lockedType;
        }

        if (leftSymbol && leftSymbol->kind == SymbolKind::Namespace)
        {
            if (!leftSymbol->innerScope)
            {
                WIO_LOG_ADD_ERROR(node.location(), "The namespace contents are inaccessible. (No Scope): {}", leftSymbol->name);
                return;
            }
            foundMember = leftSymbol->innerScope->resolve(node.member->token.value);
        }
        else 
        {
            Ref<Type> baseType = leftType;
            while (baseType && baseType->kind() == TypeKind::Alias)
            {
                baseType = baseType.AsFast<AliasType>()->aliasedType;
            }
    
            if (baseType->kind() == TypeKind::Struct)
            {
                auto structType = baseType.AsFast<StructType>();
                if (auto lockedScope = structType->structScope.Lock())
                    foundMember = lockedScope->resolve(node.member->token.value);
            }
            else if (baseType->kind() == TypeKind::Reference)
            {
                Ref<Type> referredType = baseType.AsFast<ReferenceType>()->referredType;
    
                while (referredType && referredType->kind() == TypeKind::Alias)
                {
                    referredType = referredType.AsFast<AliasType>()->aliasedType;
                }
    
                if (referredType && referredType->kind() == TypeKind::Struct)
                {
                    auto structType = referredType.AsFast<StructType>();
                    if (auto lockedScope = structType->structScope.Lock())
                        foundMember = lockedScope->resolve(node.member->token.value);
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.member->location(), 
                        "Cannot access member '{}'. Reference points to '{}', which is not a struct.", 
                        node.member->token.value, 
                        referredType ? referredType->toString() : "Unknown");
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }
        }
    
        if (!foundMember)
        {
            WIO_LOG_ADD_ERROR(node.member->location(), "Member not found: '{}' in type '{}'", node.member->token.value, leftType->toString());
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        bool isInsideObject = false;
        auto currentSearch = currentScope_;
        while (currentSearch)
        {
            if (currentSearch == foundMember->innerScope || currentSearch->resolveLocally(foundMember->name))
            {
                isInsideObject = true;
                break;
            }
            currentSearch = currentSearch->getParent().Lock();
        }

        if (foundMember->flags.get_isPrivate() && !isInsideObject)
            WIO_LOG_ADD_ERROR(node.location(), "Cannot access private member '{}' from outside the object.", foundMember->name);

        if (foundMember->flags.get_isProtected() && !isInsideObject)
            WIO_LOG_ADD_ERROR(node.location(), "Cannot access protected member '{}' from outside the object hierarchy.", foundMember->name);
    
        node.referencedSymbol = foundMember;
        node.refType = foundMember->type;
        
        if (auto memberId = node.member.Get(); memberId)
        {
            memberId->referencedSymbol = foundMember;
            memberId->refType = foundMember->type;
        }
    }

    void SemanticAnalyzer::visit(FunctionCallExpression& node)
    {
        node.callee->accept(*this);
        Ref<Symbol> calleeSym = node.callee->referencedSymbol.Lock();

        bool isConstructorCall = false;
        Ref<Type> structReturnType = nullptr;

        if (calleeSym && calleeSym->kind == SymbolKind::Struct)
        {
            isConstructorCall = true;
            structReturnType = calleeSym->type;
            
            auto structType = structReturnType.AsFast<StructType>();
            if (auto lockedScope = structType->structScope.Lock())
            {
                calleeSym = lockedScope->resolveLocally("OnConstruct");
            }
                
            if (!calleeSym)
            {
                WIO_LOG_ADD_ERROR(node.location(), "No constructor found for type '{}'.", structType->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        std::vector<Ref<Type>> argTypes;
        for (auto& arg : node.arguments)
        {
            arg->accept(*this);
            argTypes.push_back(arg->refType.Lock());
        }

        auto isSafeRefCast = [](const Ref<Type>& dest, const Ref<Type>& src) -> bool
        {
            if (dest && src && dest->kind() == TypeKind::Reference && src->kind() == TypeKind::Reference)
            {
                auto dRef = dest.AsFast<ReferenceType>();
                auto sRef = src.AsFast<ReferenceType>();
                if (!dRef->isMutable && sRef->isMutable && dRef->referredType->isCompatibleWith(sRef->referredType)) return true;
            }
            return false;
        };

        if (calleeSym && calleeSym->kind == SymbolKind::FunctionGroup)
        {
            Ref<Symbol> bestMatch = nullptr;
            int bestScore = -1;
            bool isAmbiguous = false;

            for (auto& overload : calleeSym->overloads)
            {
                auto funcType = overload->type.AsFast<FunctionType>();
                if (funcType->paramTypes.size() != argTypes.size()) continue;

                bool isMatch = true;
                int currentScore = 0;

                for (size_t i = 0; i < argTypes.size(); ++i)
                {
                    auto dest = funcType->paramTypes[i];
                    const auto& src = argTypes[i];

                    if (dest->isCompatibleWith(src) || (dest->isNumeric() && src->isNumeric()))
                    {
                        if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive && 
                            dest.AsFast<PrimitiveType>()->name == src.AsFast<PrimitiveType>()->name)
                        {
                            currentScore += 1000; 
                        }
                        else if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive)
                        {
                            currentScore += 100;
                            auto destName = dest.AsFast<PrimitiveType>()->name;
                            auto srcName = src.AsFast<PrimitiveType>()->name;
                            bool destIsUn = destName.starts_with('u');
                            bool srcIsUn = srcName.starts_with('u');
                            bool destIsInt = destName.starts_with('i');
                            bool srcIsInt = srcName.starts_with('i');
                            bool destIsFlt = destName.starts_with('f');
                            bool srcIsFlt = srcName.starts_with('f');

                            if ((destIsUn && srcIsUn) || (destIsInt && srcIsInt) || (destIsFlt && srcIsFlt)) currentScore += 50;

                            auto getSize = [](const std::string& s) -> int
                            {
                                if (s.ends_with("8")) return 1;
                                if (s.ends_with("16")) return 2;
                                if (s.ends_with("32")) return 4;
                                if (s.ends_with("64") || s == "isize" || s == "usize") return 8;
                                return 4;
                            };

                            int sizeDiff = getSize(destName) - getSize(srcName);
                            if (sizeDiff >= 0) currentScore += (10 - sizeDiff); 
                        }
                        else
                        {
                            currentScore += 10;
                        }
                    }
                    else if (isSafeRefCast(dest, src)) 
                    {
                        currentScore += 5; 
                    }
                    else
                    {
                        isMatch = false;
                        break;
                    }
                }

                if (isMatch)
                {
                    if (currentScore > bestScore)
                    {
                        bestMatch = overload;
                        bestScore = currentScore;
                        isAmbiguous = false;
                    }
                    else if (currentScore == bestScore)
                    {
                        isAmbiguous = true;
                    }
                }
            }

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Ambiguous function call to '{}'.", calleeSym->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
            if (!bestMatch)
            {
                WIO_LOG_ADD_ERROR(node.location(), "No matching function/constructor overload found.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!isConstructorCall)
            {
                node.callee->referencedSymbol = bestMatch;
                node.callee->refType = bestMatch->type;
            }
            
            node.refType = isConstructorCall ? structReturnType : bestMatch->type.AsFast<FunctionType>()->returnType;
            return; 
        }

        Ref<Type> calleeType = calleeSym->type;
        if (!calleeType || calleeType->kind() != TypeKind::Function)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Called expression is not a function or struct.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto funcType = calleeType.AsFast<FunctionType>();
        if (argTypes.size() != funcType->paramTypes.size())
            WIO_LOG_ADD_ERROR(node.location(), "Function expects '{}' arguments, but got '{}'.", funcType->paramTypes.size(), argTypes.size());
        
        size_t argCount = std::min(argTypes.size(), funcType->paramTypes.size());
        for (size_t i = 0; i < argCount; ++i)
        {
            if (!funcType->paramTypes[i]->isCompatibleWith(argTypes[i]) && 
                !isSafeRefCast(funcType->paramTypes[i], argTypes[i]) &&
                !(funcType->paramTypes[i]->isNumeric() && argTypes[i]->isNumeric()))
            {
                WIO_LOG_ADD_ERROR(node.arguments[i]->location(), "Argument mismatch at index {}.", i);
            }
        }

        node.refType = isConstructorCall ? structReturnType : funcType->returnType;
    }

    void SemanticAnalyzer::visit(LambdaExpression& node)
    {
        enterScope(ScopeKind::Function);

        std::vector<Ref<Type>> paramTypes;

        for (auto& param : node.parameters)
            {
            Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
            if (param.type)
            {
                param.type->accept(*this);
                pType = param.type->refType.Lock();
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Lambda parameters must have explicit types.");
            }
            paramTypes.push_back(pType);

            Ref<Symbol> paramSym = createSymbol(param.name->token.value, pType, SymbolKind::Variable, param.name->location());
            currentScope_->define(param.name->token.value, paramSym);
            param.name->referencedSymbol = paramSym;
            param.name->refType = pType;
        }

        Ref<Type> retType = Compiler::get().getTypeContext().getVoid();
        if (node.returnType)
        {
            node.returnType->accept(*this);
            retType = node.returnType->refType.Lock();
        }

        if (node.body)
        {
            node.body->accept(*this);
            
            if (!node.returnType)
            {
                if (node.body->is<ExpressionStatement>())
                {
                    retType = node.body->as<ExpressionStatement>()->expression->refType.Lock();
                } 
                else if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    for (auto& stmt : block->statements)
                    {
                        if (stmt->is<ReturnStatement>())
                        {
                            auto retStmt = stmt->as<ReturnStatement>();
                            if (retStmt->value)
                            {
                                retType = retStmt->value->refType.Lock();
                            }
                            break;
                        }
                    }
                }
            }
        }

        exitScope();

        node.refType = Compiler::get().getTypeContext().getFunctionType(retType, paramTypes);
    }
    
    void SemanticAnalyzer::visit(RefExpression& node)
    {
        node.operand->accept(*this);
        
        bool isMut; 

        if (auto lockedSym = node.operand->referencedSymbol.Lock(); lockedSym)
            isMut = lockedSym->flags.get_isMutable();
        else
            isMut = false; 

        node.isMut = isMut;
        node.refType = Compiler::get().getTypeContext().getReferenceType(node.operand->refType.Lock(), isMut);
    }

    void SemanticAnalyzer::visit(FitExpression& node)
    {
        node.operand->accept(*this);
        node.targetType->accept(*this);

        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();

        if (srcType && destType)
        {
            if (!srcType->isNumeric() || !destType->isNumeric())
            {
                WIO_LOG_ADD_ERROR(node.location(), "The 'fit' operator can only be used with numeric types.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        node.refType = destType;
    }

    void SemanticAnalyzer::visit(SelfExpression& node)
    {
        if (isDeclarationPass_ || isStructResolutionPass_) return;
        
        if (!currentStructType_) {
            WIO_LOG_ADD_ERROR(node.location(), "'self' can only be used inside a component or object method.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        node.refType = Compiler::get().getTypeContext().getReferenceType(currentStructType_, true);
    }

    void SemanticAnalyzer::visit(SuperExpression& node)
    {
        if (isDeclarationPass_ || isStructResolutionPass_) return;
        
        if (!currentStructType_ || !currentBaseStructType_)
        {
            WIO_LOG_ADD_ERROR(node.location(), "'super' can only be used inside an object method that has a base class.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        node.refType = Compiler::get().getTypeContext().getReferenceType(currentBaseStructType_, true);
    }

    void SemanticAnalyzer::visit(RangeExpression& node)
    {
        node.start->accept(*this);
        node.end->accept(*this);
        
        auto sType = node.start->refType.Lock();
        auto eType = node.end->refType.Lock();
        
        if (sType && eType && (!sType->isNumeric() || !eType->isNumeric()))
        {
            WIO_LOG_ADD_ERROR(node.location(), "Range bounds must be numeric types.");
        }
        node.refType = Compiler::get().getTypeContext().getUnknown(); 
    }

    void SemanticAnalyzer::visit(MatchExpression& node)
    {
        node.value->accept(*this);
        
        Ref<Type> commonReturnType = nullptr;
        bool isVoidMatch = false;

        for (auto& matchCase : node.cases)
        {
            for (auto& val : matchCase.matchValues)
            {
                val->accept(*this);
            }
            matchCase.body->accept(*this);

            if (matchCase.body->is<BlockStatement>())
            {
                isVoidMatch = true;
            }
            else if (matchCase.body->is<ExpressionStatement>())
            {
                auto exprStmt = matchCase.body->as<ExpressionStatement>();
                auto bodyType = exprStmt->expression->refType.Lock();
                if (!commonReturnType)
                    commonReturnType = bodyType;
            }
        }
        
        if (isVoidMatch)
            node.refType = Compiler::get().getTypeContext().getVoid();
        else
            node.refType = commonReturnType ? commonReturnType : Compiler::get().getTypeContext().getVoid();
    }
    
    void SemanticAnalyzer::visit(ExpressionStatement& node) 
    {
        if (isDeclarationPass_) return;
        node.expression->accept(*this);
    }

    void SemanticAnalyzer::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }
    
    void SemanticAnalyzer::visit(VariableDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            if (currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block)
                return;
    
            Ref<Type> declaredType = nullptr;
            if (node.type)
            {
                node.type->accept(*this);
                declaredType = node.type->refType.Lock();
            }
            
            SymbolFlags flags = SymbolFlags::createAllFalse();
            if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            if (currentScope_->getKind() == ScopeKind::Global) flags.set_isGlobal(true);
    
            Ref<Symbol> sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
            currentScope_->define(node.name->token.value, sym);
            node.name->referencedSymbol = sym;
            
            return;
        }
    
        Ref<Symbol> sym = node.name->referencedSymbol.Lock();
        
        if (!sym)
        {
            Ref<Type> declaredType = nullptr;
            if (node.type)
            {
                node.type->accept(*this);
                declaredType = node.type->refType.Lock();
            }
            
            SymbolFlags flags = SymbolFlags::createAllFalse();
            if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            
            sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
            currentScope_->define(node.name->token.value, sym);
            node.name->referencedSymbol = sym;
        }
    
        if (node.initializer)
        {
            node.initializer->accept(*this);
            Ref<Type> initType = node.initializer->refType.Lock();
    
            if (!sym->type || sym->type->isUnknown()) 
            {
                sym->type = initType;
                node.name->refType = initType;
            }
            else if (!sym->type->isCompatibleWith(initType)) 
            {
                if (sym->type->isNumeric() && initType && initType->isNumeric())
                {
                    // No Problem!
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Type mismatch for '{}'.", node.name->token.value);
                }
            }
        }
    }
    
    void SemanticAnalyzer::visit(FunctionDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            Ref<Type> returnType = Compiler::get().getTypeContext().getVoid();
            if (node.returnType)
            {
                node.returnType->accept(*this);
                returnType = node.returnType->refType.Lock();
            }

            std::vector<Ref<Type>> paramTypes;
            for (auto& param : node.parameters)
            {
                Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
                if (param.type)
                {
                    param.type->accept(*this);
                    pType = param.type->refType.Lock();
                }
                paramTypes.push_back(pType);
            }

            auto funcType = Compiler::get().getTypeContext().getFunctionType(returnType, paramTypes);
            Ref<Symbol> funcSym = createSymbol(node.name->token.value, funcType, SymbolKind::Function, node.location());
            currentScope_->define(node.name->token.value, funcSym);

            node.name->refType = funcType;
            node.name->referencedSymbol = funcSym;
            return;
        }

        auto funcSym = node.name->referencedSymbol.Lock();
        auto funcType = funcSym->type.AsFast<FunctionType>();

        Ref<Type> prevRetType = currentFunctionReturnType_;
        currentFunctionReturnType_ = funcType->returnType;
        
        enterScope(ScopeKind::Function);

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            Ref<Symbol> pSym = createSymbol(param.name->token.value, funcType->paramTypes[i], SymbolKind::Parameter, param.name->location());
            currentScope_->define(param.name->token.value, pSym);
            
            param.name->referencedSymbol = pSym;
            param.name->refType = funcType->paramTypes[i];
        }

        if (node.whenCondition)
        {
            node.whenCondition->accept(*this);
            
            if (node.whenFallback)
            {
                node.whenFallback->accept(*this);
            }
            else if (funcType->returnType != Compiler::get().getTypeContext().getVoid())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Functions with a return value must provide an 'else' fallback for 'when' guards.");
            }
        }

        if (node.body)
            node.body->accept(*this);

        exitScope();
        currentFunctionReturnType_ = prevRetType;
    }

    void SemanticAnalyzer::visit(InterfaceDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto interfaceScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(interfaceScope);
            
            Ref<Type> interfaceType = Ref<StructType>::Create(node.name->token.value, interfaceScope);
            Ref<Symbol> interfaceSym = createSymbol(node.name->token.value, interfaceType, SymbolKind::Struct, node.location());
            interfaceSym->innerScope = interfaceScope;
            interfaceSym->flags.set_isInterface(true);
            currentScope_->define(node.name->token.value, interfaceSym);
            
            node.name->refType = interfaceType;
            node.name->referencedSymbol = interfaceSym;

            auto prevScope = currentScope_;
            currentScope_ = interfaceScope;
            for (auto& method : node.methods) method->accept(*this);
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;

        for (auto& method : node.methods)
            method->accept(*this); 
            
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ComponentDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope);
            Ref<Symbol> compSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            compSym->innerScope = structScope;
            currentScope_->define(node.name->token.value, compSym);
            
            node.name->refType = structType;
            node.name->referencedSymbol = compSym;

            auto prevScope = currentScope_;
            currentScope_ = structScope;
            for (auto& member : node.members) member.declaration->accept(*this);
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto structType = sym->type.AsFast<StructType>();
        
        if (isStructResolutionPass_)
        {
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;

            bool hasCustomCtor = false;
            bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
            auto bases = getAttributeArgs(node.attributes, Attribute::From);

            AccessModifier defaultAccess = AccessModifier::Public; 
            std::vector<Token> defaultArgs = getAttributeArgs(node.attributes, Attribute::Default);
            if (!defaultArgs.empty()) {
                if (defaultArgs[0].type == TokenType::kwPrivate) defaultAccess = AccessModifier::Private;
                else if (defaultArgs[0].type == TokenType::kwProtected) defaultAccess = AccessModifier::Protected;
            }

            for (const auto& baseToken : bases)
            {
                if (baseToken.type != TokenType::identifier)
                    continue;
                
                if (auto baseSym = prevScope->resolve(baseToken.value); baseSym)
                    if (!baseSym->flags.get_isInterface())
                        WIO_LOG_ADD_ERROR(node.location(), "Component '{}' cannot inherit from '{}'. Components can only inherit from interfaces.", node.name->token.value, baseToken.value);
            }

            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = nullptr;

                if (member.declaration->is<VariableDeclaration>())
                {
                    member.declaration->accept(*this);
                    auto varDecl = member.declaration->as<VariableDeclaration>();
                    memberSym = varDecl->name->referencedSymbol.Lock();
                    if (hasAttribute(varDecl->attributes, Attribute::ReadOnly)) memberSym->flags.set_isReadOnly(true);
                }
                else if (member.declaration->is<FunctionDeclaration>())
                {
                    auto funcDecl = member.declaration->as<FunctionDeclaration>();
                    memberSym = funcDecl->name->referencedSymbol.Lock();
                    std::string funcName = funcDecl->name->token.value;
                    
                    if (funcName == "OnConstruct")
                    {
                        hasCustomCtor = true;
                    }
                    else if (funcName != "OnDestruct") 
                    {
                        bool isOverride = false;
                        for (const auto& baseToken : bases)
                        {
                            if (baseToken.type != TokenType::identifier) continue;
                            if (auto baseSym = prevScope->resolve(baseToken.value))
                            {
                                if (baseSym->flags.get_isInterface())
                                {
                                    if (auto baseType = baseSym->type.AsFast<StructType>())
                                    {
                                        if (auto lockedScope = baseType->structScope.Lock())
                                        {
                                            if (lockedScope->resolveLocally(funcName))
                                            {
                                                isOverride = true; 
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        } 
                        if (!isOverride)
                        {
                            WIO_LOG_ADD_ERROR(funcDecl->location(), "Components can only contain data, lifecycle methods, or interface implementations.");
                        }
                        else
                        {
                            memberSym->flags.set_isOverride(true);
                        }
                    }
                }

                if (memberSym)
                {
                    AccessModifier finalAccess = member.access;
                    if (finalAccess == AccessModifier::None) finalAccess = defaultAccess;

                    if (finalAccess == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                    else if (finalAccess == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                    else if (finalAccess == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                }
            }

            if (!hasCustomCtor && !hasNoDefaultCtor) 
            {
                auto voidType = Compiler::get().getTypeContext().getVoid();

                auto defaultCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, {});
                Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
                currentScope_->define("OnConstruct", defaultSym);

                std::vector<Ref<Type>> memberTypes;
                for (auto& member : node.members)
                {
                    if (member.declaration->is<VariableDeclaration>())
                    {
                        auto vDecl = member.declaration->as<VariableDeclaration>();
                        memberTypes.push_back(vDecl->name->referencedSymbol.Lock()->type);
                    }
                }
                
                if (!memberTypes.empty())
                {
                    auto memberCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, memberTypes);
                    Ref<Symbol> memberSym = createSymbol("OnConstruct", memberCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", memberSym);
                }

                auto copyParamType = Ref<ReferenceType>::Create(structType, false);
                auto copyCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, { copyParamType });
                Ref<Symbol> copySym = createSymbol("OnConstruct", copyCtorType, SymbolKind::Function, node.location());
                currentScope_->define("OnConstruct", copySym);
            }
            
            currentScope_ = prevScope;
            return;
        }

        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        currentStructType_ = structType;
        
        for (auto& member : node.members)
            if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);

        currentStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ObjectDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope);
            Ref<Symbol> objSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            objSym->innerScope = structScope;
            currentScope_->define(node.name->token.value, objSym);
            
            node.name->refType = structType;
            node.name->referencedSymbol = objSym;

            auto prevScope = currentScope_;
            currentScope_ = structScope;
            for (auto& member : node.members) member.declaration->accept(*this);
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto structType = sym->type.AsFast<StructType>();
        
        if (isStructResolutionPass_)
        {
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;

            bool hasCustomCtor = false;
            bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
            auto bases = getAttributeArgs(node.attributes, Attribute::From);

            AccessModifier defaultAccess = AccessModifier::Private;
            std::vector<Token> defaultArgs = getAttributeArgs(node.attributes, Attribute::Default);
            if (!defaultArgs.empty())
            {
                if (defaultArgs[0].type == TokenType::kwPublic)
                    defaultAccess = AccessModifier::Public;
                else if (defaultArgs[0].type == TokenType::kwProtected)
                    defaultAccess = AccessModifier::Protected;
            }

            int structBaseCount = 0;
            for (const auto& baseToken : bases)
            {
                if (baseToken.type != TokenType::identifier)
                    continue;
                
                if (auto baseSym = prevScope->resolve(baseToken.value))
                {
                    if (baseSym->kind != SymbolKind::Struct)
                    { 
                        WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from '{}'.", node.name->token.value, baseToken.value);
                    }
                    else if (!baseSym->flags.get_isInterface())
                    {
                        structBaseCount++;
                        if (structBaseCount > 1)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from multiple objects/components. Single class inheritance only!", node.name->token.value);
                        }
                    }
                }
            }

            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = nullptr;

                if (member.declaration->is<VariableDeclaration>())
                {
                    member.declaration->accept(*this);
                    auto varDecl = member.declaration->as<VariableDeclaration>();
                    memberSym = varDecl->name->referencedSymbol.Lock();
                    
                    if (hasAttribute(varDecl->attributes, Attribute::ReadOnly)) memberSym->flags.set_isReadOnly(true);
                }
                else if (member.declaration->is<FunctionDeclaration>())
                {
                    auto funcDecl = member.declaration->as<FunctionDeclaration>();
                    memberSym = funcDecl->name->referencedSymbol.Lock();
                    std::string funcName = funcDecl->name->token.value;
                    
                    if (funcName == "OnConstruct")
                    {
                        hasCustomCtor = true;
                    }
                    else if (funcName != "OnDestruct") 
                    {
                        bool isOverride = false;
                        for (const auto& baseToken : bases)
                        {
                            if (baseToken.type != TokenType::identifier) continue;
                            if (auto baseSym = prevScope->resolve(baseToken.value); baseSym)
                            {
                                if (baseSym->kind == SymbolKind::Struct)
                                {
                                    if (auto baseType = baseSym->type.AsFast<StructType>(); baseType)
                                    {
                                        if (auto lockedScope = baseType->structScope.Lock(); lockedScope)
                                        {
                                            if (lockedScope->resolveLocally(funcName))
                                            {
                                                isOverride = true; 
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        } 
                        if (isOverride)
                        {
                            memberSym->flags.set_isOverride(true);
                        }
                    }
                }

                if (memberSym)
                {
                    AccessModifier finalAccess = member.access;
                    if (finalAccess == AccessModifier::None) finalAccess = defaultAccess;

                    if (finalAccess == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                    else if (finalAccess == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                    else if (finalAccess == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                }
            }

            if (!hasCustomCtor && !hasNoDefaultCtor) 
            {
                auto voidType = Compiler::get().getTypeContext().getVoid();

                auto defaultCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, {});
                Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
                currentScope_->define("OnConstruct", defaultSym);

                std::vector<Ref<Type>> memberTypes;
                for (auto& member : node.members)
                {
                    if (member.declaration->is<VariableDeclaration>())
                    {
                        auto vDecl = member.declaration->as<VariableDeclaration>();
                        memberTypes.push_back(vDecl->name->referencedSymbol.Lock()->type);
                    }
                }
                
                if (!memberTypes.empty())
                {
                    auto memberCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, memberTypes);
                    Ref<Symbol> memberSym = createSymbol("OnConstruct", memberCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", memberSym);
                }

                auto copyParamType = Ref<ReferenceType>::Create(structType, false);
                auto copyCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, { copyParamType });
                Ref<Symbol> copySym = createSymbol("OnConstruct", copyCtorType, SymbolKind::Function, node.location());
                currentScope_->define("OnConstruct", copySym);
            }
            
            currentScope_ = prevScope;
            return;
        }

        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        currentStructType_ = structType;
        currentBaseStructType_ = nullptr;

        auto bases = getAttributeArgs(node.attributes, Attribute::From);
        for (const auto& baseToken : bases)
        {
            if (baseToken.type != TokenType::identifier)
                continue;
            
            if (auto baseSym = prevScope->resolve(baseToken.value))
            {
                if (baseSym->kind == SymbolKind::Struct && !baseSym->flags.get_isInterface())
                {
                    currentBaseStructType_ = baseSym->type;
                    break;
                }
            }
        }
        
        for (auto& member : node.members)
            if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);
        
        currentStructType_ = nullptr;
        currentBaseStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(FlagDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> flagType = Ref<StructType>::Create(node.name->token.value, structScope);
            Ref<Symbol> flagSym = createSymbol(node.name->token.value, flagType, SymbolKind::Struct, node.location());
            
            flagSym->innerScope = structScope;
            flagSym->flags.set_isFlag(true); 
            
            currentScope_->define(node.name->token.value, flagSym);
            node.name->refType = flagType;
            node.name->referencedSymbol = flagSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            
            auto voidType = Compiler::get().getTypeContext().getVoid();
            auto defaultCtorType = Compiler::get().getTypeContext().getFunctionType(voidType, {});
            Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
            currentScope_->define("OnConstruct", defaultSym);
            
            currentScope_ = prevScope;
        }
    }

    void SemanticAnalyzer::visit(EnumDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto enumScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(enumScope);
            
            Ref<Type> enumType = Ref<StructType>::Create(node.name->token.value, enumScope);
            Ref<Symbol> enumSym = createSymbol(node.name->token.value, enumType, SymbolKind::Struct, node.location());
            
            enumSym->innerScope = enumScope;
            enumSym->flags.set_isEnum(true);
            
            currentScope_->define(node.name->token.value, enumSym);
            node.name->refType = enumType;
            node.name->referencedSymbol = enumSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            
            auto targetType = Compiler::get().getTypeContext().getI32();
           
            if (auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type); !typeArgs.empty())
            {
                auto& ctx = Compiler::get().getTypeContext();
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (typeArgs[0].type){
                case TokenType::kwI8:  targetType = ctx.getI8(); break;
                case TokenType::kwU8:  targetType = ctx.getU8(); break;
                case TokenType::kwI16: targetType = ctx.getI16(); break;
                case TokenType::kwU16: targetType = ctx.getU16(); break;
                case TokenType::kwI32: targetType = ctx.getI32(); break;
                case TokenType::kwU32: targetType = ctx.getU32(); break;
                case TokenType::kwI64: targetType = ctx.getI64(); break;
                case TokenType::kwU64: targetType = ctx.getU64(); break;
                default: WIO_LOG_ADD_ERROR(node.location(), "Invalid underlying type for enum.");
                }
            }
            
            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = createSymbol(member.name->token.value, sym->type, SymbolKind::Variable, member.name->location());
                memberSym->flags.set_isReadOnly(true);
                
                currentScope_->define(member.name->token.value, memberSym);
                member.name->referencedSymbol = memberSym;
                member.name->refType = targetType;
                
                if (member.value)
                    member.value->accept(*this);
            }
            currentScope_ = prevScope;
        }
    }

    void SemanticAnalyzer::visit(FlagsetDeclaration& node)
    {
        if (isDeclarationPass_) {
            auto flagsetScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(flagsetScope);
            
            Ref<Type> flagsetType = Ref<StructType>::Create(node.name->token.value, flagsetScope);
            Ref<Symbol> flagsetSym = createSymbol(node.name->token.value, flagsetType, SymbolKind::Struct, node.location());
            
            flagsetSym->innerScope = flagsetScope;
            flagsetSym->flags.set_isFlagset(true);
            
            currentScope_->define(node.name->token.value, flagsetSym);
            node.name->refType = flagsetType;
            node.name->referencedSymbol = flagsetSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            
            auto targetType = Compiler::get().getTypeContext().getU32();
           
            if (auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type); !typeArgs.empty())
            {
                auto& ctx = Compiler::get().getTypeContext();
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (typeArgs[0].type){
                case TokenType::kwI8:  targetType = ctx.getI8(); break;
                case TokenType::kwU8:  targetType = ctx.getU8(); break;
                case TokenType::kwI16: targetType = ctx.getI16(); break;
                case TokenType::kwU16: targetType = ctx.getU16(); break;
                case TokenType::kwI32: targetType = ctx.getI32(); break;
                case TokenType::kwU32: targetType = ctx.getU32(); break;
                case TokenType::kwI64: targetType = ctx.getI64(); break;
                case TokenType::kwU64: targetType = ctx.getU64(); break;
                default: WIO_LOG_ADD_ERROR(node.location(), "Invalid underlying type for flagset.");
                }
            }
            
            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = createSymbol(member.name->token.value, sym->type, SymbolKind::Variable, member.name->location());
                memberSym->flags.set_isReadOnly(true);
                
                currentScope_->define(member.name->token.value, memberSym);
                member.name->referencedSymbol = memberSym;
                member.name->refType = targetType;
                
                if (member.value) member.value->accept(*this);
            }
            currentScope_ = prevScope;
        }
    }
    
    void SemanticAnalyzer::visit(IfStatement& node)
    {
        if (isDeclarationPass_) return;
        node.condition->accept(*this);
        
        if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
        {
            // Todo: Check null (i.g. it's not works well)
            if (!condType->isNumeric() && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null) 
            {
                WIO_LOG_ADD_ERROR(
                    node.condition->location(),
                    "If condition must be a boolean, numeric, or reference type. Got: {}",
                    condType->toString()
                );
            }
        }

        if (node.thenBranch) node.thenBranch->accept(*this);
        if (node.elseBranch) node.elseBranch->accept(*this);
    }
    
    void SemanticAnalyzer::visit(WhileStatement& node)
    {
        if (isDeclarationPass_) return;
        node.condition->accept(*this);

        if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
        {
            // Todo: Check null (i.g. it's not works well)
            if (!condType->isNumeric() && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null) 
            {
                WIO_LOG_ADD_ERROR(
                    node.condition->location(),
                    "While condition must be a boolean, numeric, or reference type. Got: {}",
                    condType->toString()
                );
            }
        }

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;
    }

    void SemanticAnalyzer::visit(BreakStatement& node)
    {
        if (isDeclarationPass_) return;
        if (loopDepth_ == 0)
            WIO_LOG_ADD_ERROR(node.location(), "'break' statement can only be used inside a loop.");
    }

    void SemanticAnalyzer::visit(ContinueStatement& node)
    {
        if (isDeclarationPass_) return;
        if (loopDepth_ == 0)
            WIO_LOG_ADD_ERROR(node.location(), "'continue' statement can only be used inside a loop.");
    }
    
    void SemanticAnalyzer::visit(ReturnStatement& node)
    {
        if (isDeclarationPass_) return;
        Ref<Type> actualType = Compiler::get().getTypeContext().getVoid();

        if (node.value)
        {
            node.value->accept(*this);
            actualType = node.value->refType.Lock();
        }

        if (currentFunctionReturnType_)
        {
            if (!actualType->isCompatibleWith(currentFunctionReturnType_))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Return type mismatch! Expected '{}', but got '{}'.",
                    currentFunctionReturnType_->toString(),
                    actualType->toString()
                );
            }
        }
        else
        {
            WIO_LOG_ADD_ERROR(node.location(), "Return statement found outside of a function.");
        }
    }

    void SemanticAnalyzer::visit(UseStatement& node)
    {
        if (!isDeclarationPass_) return;
        
        auto getOrCreateNamespace = [&](const Ref<Scope>& targetScope, const std::string& name) -> Ref<Symbol>
        {
            if (auto existing = targetScope->resolve(name)) {
                if (existing->kind == SymbolKind::Namespace)
                    return existing;
                
                WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and is not a namespace.", name);
                return nullptr;
            }
            
            auto nsScope = Ref<Scope>::Create(targetScope, ScopeKind::Global); 
            
            auto nsSymbol = createSymbol(name, Compiler::get().getTypeContext().getUnknown(), SymbolKind::Namespace, node.location());
            nsSymbol->innerScope = nsScope;
            
            targetScope->define(name, nsSymbol);
            return nsSymbol;
        };
    
        if (node.isStdLib) // use std::io;
        {
            Ref<Symbol> stdNs = getOrCreateNamespace(currentScope_, "std");
            if (!stdNs) return;
    
            if (node.modulePath == "io")
            {
                Ref<Symbol> ioNs = getOrCreateNamespace(stdNs->innerScope, "io");
                if (!ioNs) return;
    
                runtime::Loader<runtime::IOLoader>::Load(ioNs->innerScope, symbols_);
                
                // OPSİYONEL KISAYOL: Wio dilinde kullanıcılar "std::io::print()" yazmak yerine
                // "io::print()" de yazabilsin diye, 'io' namespace'inin bir referansını (alias) 
                // bulunduğumuz scope'a da ekleyebiliriz:
                currentScope_->define(node.moduleName, ioNs); 
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Unknown standard library module: 'std::{}'", node.modulePath);
            }
        }
        else
        {
            WIO_LOG_ADD_ERROR(node.location(), "User-defined module loading is not implemented yet.");
        }
    }
}
