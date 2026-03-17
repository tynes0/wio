#include "wio/sema/analyzer.h"

#include "wio/codegen/mangler.h"
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
    }
    
    SemanticAnalyzer::SemanticAnalyzer()
        : currentScope_(nullptr)
    {
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

    std::string SemanticAnalyzer::mangleName(const std::string& original, SymbolKind kind)
    {
        return ""; // TODO: IMPL
    }

    void SemanticAnalyzer::visit(Program& node)
    {
        enterScope(ScopeKind::Global);

        // TODO: add built-in's 

        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }

        exitScope();
    }

    void SemanticAnalyzer::visit(BlockStatement& node)
    {
        enterScope(ScopeKind::Block);

        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }

        exitScope();
    }

    void SemanticAnalyzer::visit(TypeSpecifier& node)
    {
        Ref<Type> type = resolvePrimitiveType(node.name.value);

        // Todo: If it's not primitive (it might become a struct or enum later), search for it in the symbol table.
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

        if (node.op.isComparison())
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

        if (lhsType && rhsType && !lhsType->isCompatibleWith(rhsType))
        {
            WIO_LOG_ADD_ERROR(node.op.loc,
                "Type mismatch in assignment: Cannot assign '{}' to '{}'.",
                rhsType->toString(),
                lhsType->toString()
            );
        }

        if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
        {
            if (!referSym->flags.get_isMutable())
            {
                WIO_LOG_ADD_ERROR(
                    node.op.loc,
                    "Cannot assign to immutable variable '{0}'. Hint: Declare it as 'mut {0}'.",
                    referSym->name
                );
            }
        }
        else
        {
            // If the left side is not a variable (e.g., 5 = x;)
            // Parser usually catches this, but it's good to be sure.
            // (This will be expanded on later for array access or member access cases)
        }

        // The result of an assignment expression is usually void or the assigned value.
        // Todo: If we want to support C-style (x = y = 5) (Yes we want.), we should return rhsType.
        // Let's say void for now; we'll change it if we want to make it expression-based.
        node.refType = Compiler::get().getTypeContext().getVoid(); 
    }
    
    void SemanticAnalyzer::visit(IntegerLiteral& node)
    {
        IntegerResult result = common::getInteger(node.token.value);
        Ref<Type> type = Type::getTypeFromIntegerResult(result);
        node.refType = type;
    }
    
    void SemanticAnalyzer::visit(FloatLiteral& node)
    {
        FloatResult result = common::getFloat(node.token.value);
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
    
    void SemanticAnalyzer::visit(LambdaLiteral& node)
    {
        // Todo: implement
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

    // 1. Durum: Namespace  (Örn: std::io)
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
        // YARDIMCI ADIM: Tipimiz bir Alias (Takma ad) ise asıl tipe inelim.
        // Örn: type MyStruct = RealStruct; yapılmış olabilir.
        Ref<Type> baseType = leftType;
        while (baseType && baseType->kind() == TypeKind::Alias)
        {
            baseType = baseType.AsFast<AliasType>()->aliasedType;
        }

        // 2. Durum: Doğrudan Struct Erişimi (Örn: obj.val)
        if (baseType->kind() == TypeKind::Struct)
        {
            // NOT: İstersen dil kurallarına göre burada operatör kontrolü yapabilirsin.
            // if (node.opType != TokenType::opDot) { Hata: "Struct'lar için '.' kullanmalısın" }

            auto structType = baseType.AsFast<StructType>();
            if (structType->structScope)
                 foundMember = structType->structScope->resolve(node.member->token.value);
        }
        // 3. Durum: Pointer/Referans Erişimi (Örn: ptr->val veya ref.val)
        else if (baseType->kind() == TypeKind::Reference)
        {
            // NOT: Yine diline özel operatör kuralı koyabilirsin. 
            // C++ gibi yapacaksan: if (node.opType != TokenType::opArrow) { Hata ver }

            // Referansın tuttuğu asıl tipe (referredType) iniyoruz.
            Ref<Type> referredType = baseType.AsFast<ReferenceType>()->referredType;

            // Referans edilen tip de bir Alias olabilir, asıl tipe inene kadar onu da soyalım
            while (referredType && referredType->kind() == TypeKind::Alias)
            {
                referredType = referredType.AsFast<AliasType>()->aliasedType;
            }

            // Nihayet referansın ucundaki tip bir struct ise:
            if (referredType && referredType->kind() == TypeKind::Struct)
            {
                auto structType = referredType.AsFast<StructType>();
                if (structType->structScope)
                     foundMember = structType->structScope->resolve(node.member->token.value);
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

    // Üye bulunamadıysa hata ver ve bitir
    if (!foundMember)
    {
        WIO_LOG_ADD_ERROR(node.member->location(), "Member not found: '{}' in type '{}'", node.member->token.value, leftType->toString());
        node.refType = Compiler::get().getTypeContext().getUnknown();
        return;
    }

    // Bulunan üyeyi ve tipini AST düğümlerine (node) kaydet
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
        Ref<Type> calleeType = node.callee->refType.Lock();

        if (!calleeType || calleeType->kind() != TypeKind::Function)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Called expression is not a function.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto funcType = calleeType.AsFast<FunctionType>();

        if (node.arguments.size() != funcType->paramTypes.size())
        {
            WIO_LOG_ADD_ERROR(node.location(), "Function expects '{}' arguments, but got '{}'.", funcType->paramTypes.size(), node.arguments.size());
        }

        size_t argCount = std::min(node.arguments.size(), funcType->paramTypes.size());
        for (size_t i = 0; i < argCount; ++i)
        {
            node.arguments[i]->accept(*this);
            Ref<Type> argType = node.arguments[i]->refType.Lock();
            Ref<Type> paramType = funcType->paramTypes[i];

            if (!paramType->isCompatibleWith(argType))
            {
                WIO_LOG_ADD_ERROR(
                    node.arguments[i]->location(),
                    "Argument mismatch at index '{}': Expected '{}', but got '{}'.",
                    i,
                    paramType->toString(),
                    argType->toString()
                );
            }
        }

        node.refType = funcType->returnType;
    }
    
    void SemanticAnalyzer::visit(RefExpression& node)
    {
        node.operand->accept(*this);
        bool isMut = node.isMut;
        
        if (isMut)
        {
            if (auto lockedSym = node.operand->referencedSymbol.Lock(); lockedSym)
            {
                if (!lockedSym->flags.get_isMutable())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Cannot take mutable reference of immutable variable.");
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Cannot take mutable reference of a temporary value.");
            }
        }
        
        node.refType = Compiler::get().getTypeContext().getReferenceType(node.operand->refType.Lock(), isMut);
    }
    
    void SemanticAnalyzer::visit(ExpressionStatement& node) 
    {
        node.expression->accept(*this);
    }
    
    void SemanticAnalyzer::visit(VariableDeclaration& node)
    {
        Ref<Type> initType = nullptr;
        if (node.initializer)
        {
            node.initializer->accept(*this);
            initType = node.initializer->refType.Lock();
        }

        Ref<Type> declaredType = nullptr;
        if (node.type)
        {
            node.type->accept(*this);
            declaredType = node.type->refType.Lock();
        }

        Ref<Type> finalType;

        if (declaredType && initType)
        {
            if (!Type::matchTypes(declaredType, initType))
                
            if (!declaredType->isCompatibleWith(initType))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Type mismatch! Variable '{}' expects {}, but got {}.",
                    node.name->token.value,
                    declaredType->toString(),
                    initType->toString()
                );
            }

            finalType = declaredType;
        }
        else if (declaredType)
        {
            finalType = declaredType;
        }
        else if (initType)
        {
            finalType = initType;
        }
        else
        {
            WIO_LOG_ADD_ERROR(node.location(), "Variable '{}' must have a type or an initializer.", node.name->token.value);
            finalType = Compiler::get().getTypeContext().getUnknown();
        }

        SymbolFlags flags = SymbolFlags::createAllFalse();
        if (node.mutability == Mutability::Mutable)
            flags.set_isMutable(true);

        Ref<Symbol> sym = createSymbol(node.name->token.value, finalType, SymbolKind::Variable, node.location(), flags);

        currentScope_->define(node.name->token.value, sym);
        
        node.name->refType = finalType;
    }
    
    void SemanticAnalyzer::visit(FunctionDeclaration& node)
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
            else
            {
                WIO_LOG_ADD_ERROR(param.name->location(), "Parameter '{}' must have a type.", param.name->token.value);
            }
            paramTypes.push_back(pType);
        }

        auto funcType = Compiler::get().getTypeContext().getFunctionType(returnType, paramTypes);
        
        Ref<Symbol> funcSym = createSymbol(node.name->token.value, funcType, SymbolKind::Function, node.location());
        currentScope_->define(node.name->token.value, funcSym);

        node.name->refType = funcType;
        node.name->referencedSymbol = funcSym;

        Ref<Type> prevRetType = currentFunctionReturnType_;
        currentFunctionReturnType_ = returnType;
        
        enterScope(ScopeKind::Function);

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            
            Ref<Symbol> pSym = createSymbol(param.name->token.value, paramTypes[i], SymbolKind::Parameter, param.name->location());
            currentScope_->define(param.name->token.value, pSym);
            
            param.name->referencedSymbol = pSym;
            param.name->refType = paramTypes[i];
        }

        if (node.body)
        {
            node.body->accept(*this);
        }

        exitScope();
        currentFunctionReturnType_ = prevRetType;
    }
    
    void SemanticAnalyzer::visit(IfStatement& node)
    {
        node.condition->accept(*this);
        
        if (node.condition->refType.Lock() != Compiler::get().getTypeContext().getBool()) // Todo: improve this
        {
            WIO_LOG_ADD_ERROR(node.condition->location(), "If condition must be a boolean expression.");
        }

        if (node.thenBranch) node.thenBranch->accept(*this);
        if (node.elseBranch) node.elseBranch->accept(*this);
    }
    
    void SemanticAnalyzer::visit(WhileStatement& node)
    {
        node.condition->accept(*this);

        if (node.condition->refType.Lock() != Compiler::get().getTypeContext().getBool()) // Todo: improve this
        {
            WIO_LOG_ADD_ERROR(node.condition->location(), "While condition must be a boolean expression.");
        }

        if (node.body) node.body->accept(*this);
    }
    
    void SemanticAnalyzer::visit(ReturnStatement& node)
    {
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
    auto getOrCreateNamespace = [&](Ref<Scope> targetScope, const std::string& name) -> Ref<Symbol>
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
        // Todo: Kullanıcı modüllerini (Örn: use "math.wio";) dosyadan okuyup AST'ye çevirme 
        // ve Semantic Analyzer'dan geçirme işlemleri burada olacak.
        WIO_LOG_ADD_ERROR(node.location(), "User-defined module loading is not implemented yet.");
    }
}

    void SemanticAnalyzer::visit(AttributeStatement& node)
    {
        switch (node.attribute)
        {
        case Attribute::CppNamespaceStart:
        case Attribute::CppNamespaceEnd:
            break;
        case Attribute::Unknown:
            WIO_LOG_ADD_WARN(node.location(), "Unknown attribute!");
            break;
        }
    }
}
