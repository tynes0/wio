#include "wio/codegen/cpp_generator.h"

#include "wio/common/filesystem/filesystem.h"
#include "compiler.h"
#include "wio/common/logger.h"
#include "wio/sema/symbol.h"

#define EMIT_TABS() do { for (int _____I_____ = 0; _____I_____ < indentationLevel_; ++_____I_____) buffer_ << "    "; } while(false)

namespace wio::codegen
{
    namespace 
    {
        std::string toCppType(const Ref<sema::Type>& type)
        {
            if (!type) return "void"; // Fallback

            if (type->kind() == sema::TypeKind::Function)
            {
                auto funcType = type.AsFast<sema::FunctionType>();
                std::string result = "std::function<" + toCppType(funcType->returnType) + "(";
                for (size_t i = 0; i < funcType->paramTypes.size(); ++i) {
                    result += toCppType(funcType->paramTypes[i]);
                    if (i < funcType->paramTypes.size() - 1) result += ", ";
                }
                result += ")>";
                return result;
            }
            
            if (type->kind() == sema::TypeKind::Reference)
            {
                auto refType = type.AsFast<sema::ReferenceType>();
                if (refType->referredType && refType->referredType->kind() == sema::TypeKind::Struct)
                {
                    auto sType = refType->referredType.AsFast<sema::StructType>();
                    if (sType->isInterface)
                        return Mangler::mangleInterface(sType->name, sType->scopePath) + "*";
                }
            }
            else if (type->kind() == sema::TypeKind::Struct)
            {
                auto sType = type.AsFast<sema::StructType>();
                if (sType->isInterface)
                    return Mangler::mangleInterface(sType->name, sType->scopePath) + "*"; 
            }

            return type->toCppString();
        }

        std::vector<std::string> getBaseInterfaces(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            std::vector<std::string> bases;
            for (const auto& attr : attributes)
                if (attr->attribute == Attribute::From)
                    for (const auto& arg : attr->args)
                        if (arg.type == TokenType::identifier)
                            bases.push_back(arg.value);
            return bases;
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

        std::string mangleStructTypeName(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};

            return Mangler::mangleStruct(type->name, type->scopePath);
        }

        std::string mangleInterfaceTypeName(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};

            return Mangler::mangleInterface(type->name, type->scopePath);
        }

        std::string mangleNamedType(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};

            return type->isInterface ? mangleInterfaceTypeName(type) : mangleStructTypeName(type);
        }

        Ref<sema::StructType> getStructTypeFromSymbol(const Ref<sema::Symbol>& symbol)
        {
            if (!symbol || symbol->kind != sema::SymbolKind::Struct || !symbol->type || symbol->type->kind() != sema::TypeKind::Struct)
                return nullptr;

            return symbol->type.AsFast<sema::StructType>();
        }

        std::string mangleNamedType(const Ref<sema::Symbol>& symbol)
        {
            return mangleNamedType(getStructTypeFromSymbol(symbol));
        }
    }
    
    CppGenerator::CppGenerator() = default;

    std::string CppGenerator::generate(const Ref<Program>& program)
    {
        buffer_.str("");
        indentationLevel_ = 0;

        generateHeader();
        program->accept(*this);

        return header_.str() + buffer_.str();
    }

    void CppGenerator::generateHeader()
    {
        header_.str("");
        emitHeaderLine("#include <cstdint>");
        emitHeaderLine("#include <string>");
        emitHeaderLine("#include <vector>");
        emitHeaderLine("#include <array>");
        emitHeaderLine("#include <format>");
        emitHeaderLine("#include <iostream>");
        emitHeaderLine("#include <functional>");
        emitHeaderLine("#include <map>");
        emitHeaderLine("#include <unordered_map>");
        emitHeaderLine("#include <exception.h>");
        emitHeaderLine("#include <ref.h>");
        emitHeaderLine("");

        emitHeaderLine("namespace wio");
        emitHeaderLine("{");
        indent();
        emitHeaderLine("using String = std::string;");
        emitHeaderLine("using WString = std::wstring;");
        dedent();
        emitHeaderLine("}");
        emitHeaderLine("");

        emitHeaderLine("namespace wio");
        emitHeaderLine("{");
        indent();
        emitHeaderLine("template <typename T>");
        emitHeaderLine("using DArray = std::vector<T>;");
        emitHeaderLine("");
        emitHeaderLine("template <typename T, size_t N>");
        emitHeaderLine("using SArray = std::array<T, N>;");
        emitHeaderLine("");
        emitHeaderLine("template <typename K, typename V>");
        emitHeaderLine("using Dict = std::unordered_map<K, V>;");
        emitHeaderLine("");
        emitHeaderLine("template <typename K, typename V>");
        emitHeaderLine("using Tree = std::map<K, V>;");
        dedent();
        emitHeaderLine("}");
        emitHeaderLine("");
    }

    void CppGenerator::emit(const std::string& str)
    {
        buffer_ << str;
    }

    void CppGenerator::emitLine(const std::string& str)
    {
        EMIT_TABS();
        buffer_ << str << "\n";
    }

    void CppGenerator::emitHeader(const std::string& str)
    {
        header_ << str << "\n";
    }

    void CppGenerator::emitHeaderLine(const std::string& str)
    {
        for (int i = 0; i < indentationLevel_; ++i) header_ << "    ";
        header_ << str << "\n";
    }

    void CppGenerator::emitMain(FunctionDeclaration& node)
    {
        auto lockedRefType = node.returnType->refType.Lock();
        
        if (lockedRefType->toString() != "i32" && lockedRefType->toString() != "void")
        {
            throw InvalidEntryReturnType("Entry return type must be i32 or void.", node.location());
        }
        if(!node.body)
        {
            throw MissedEntryBody("The Entry function must have a body.", node.location());
        }
        
        emitLine();
        
        std::string paramName;
        bool hasArgs = false;

        if (node.parameters.empty())
        {
            emitLine("int main() {");
        }
        else
        {
            if (node.parameters.size() > 1)
                throw InvalidEntryParameter("The `Entry` function should have only one parameter. (string[])", node.location());

            Parameter& param = node.parameters.front();
            auto lockedParamRefType = param.name->refType.Lock();
            if (lockedParamRefType->toString() != "string[]")
            {
                throw InvalidEntryParameter("The `Entry` function's parameter type must be `string[]`", node.location());
            }

            paramName = param.name->token.value;
            hasArgs = true;
            
            emitLine("int main(int argc, char** argv) {");
        }
        indent();

        if (hasArgs)
        {
            emitLine("wio::DArray<wio::String> " + paramName + ";");
            emitLine(paramName + ".reserve(argc);");
            emitLine("for (int i = 0; i < argc; ++i) {");
            indent();
            emitLine(paramName + ".push_back(wio::String(argv[i]));");
            dedent();
            emitLine("}");
            emitLine("");
        }

        emitLine("try {");
        indent();
        
        if (node.body->is<BlockStatement>())
        {
            auto block = node.body->as<BlockStatement>();
            for (auto& stmt : block->statements)
                stmt->accept(*this);
        }
        else
        {
            node.body->accept(*this);
        }
        
        if (lockedRefType->toString() == "void")
            emitLine("return 0;");

        dedent();
        emitLine("}");
        emitLine("catch (const wio::runtime::RuntimeException& ex)"); 
        emitLine("{");
        indent();
        emitLine(R"(std::cout << "Runtime Error: " << ex.what() << '\n';)"); 
        emitLine("return 1;");
        dedent();
        emitLine("}"); 
        
        emitLine("return 0;"); 
        dedent();
        emitLine("}");
    }
    
    void CppGenerator::indent() { indentationLevel_++; }
    void CppGenerator::dedent() { indentationLevel_--; }

    void CppGenerator::emitStatements(const std::vector<NodePtr<Statement>>& statements)
    {
        auto emitPhase = [&](auto&& self, const std::vector<NodePtr<Statement>>& group, const auto& emitter) -> void
        {
            for (const auto& stmt : group)
            {
                if (stmt->is<RealmDeclaration>())
                {
                    self(self, stmt->as<RealmDeclaration>()->statements, emitter);
                    continue;
                }

                emitter(stmt);
            }
        };

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<UseStatement>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<EnumDeclaration>() || stmt->is<FlagsetDeclaration>() || stmt->is<FlagDeclaration>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<ComponentDeclaration>())
            {
                auto sym = stmt->as<ComponentDeclaration>()->name->referencedSymbol.Lock();
                emitLine(std::format("struct {};", Mangler::mangleStruct(stmt->as<ComponentDeclaration>()->name->token.value, sym ? sym->scopePath : "")));
            }
            else if (stmt->is<ObjectDeclaration>())
            {
                auto sym = stmt->as<ObjectDeclaration>()->name->referencedSymbol.Lock();
                emitLine(std::format("struct {};", Mangler::mangleStruct(stmt->as<ObjectDeclaration>()->name->token.value, sym ? sym->scopePath : "")));
            }
            else if (stmt->is<InterfaceDeclaration>())
            {
                auto sym = stmt->as<InterfaceDeclaration>()->name->referencedSymbol.Lock();
                emitLine(std::format("struct {};", Mangler::mangleInterface(stmt->as<InterfaceDeclaration>()->name->token.value, sym ? sym->scopePath : "")));
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<InterfaceDeclaration>() || stmt->is<ComponentDeclaration>() || stmt->is<ObjectDeclaration>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<VariableDeclaration>())
                stmt->accept(*this);
        });

        isEmittingPrototypes_ = true;
        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<FunctionDeclaration>())
                stmt->accept(*this);
        });
        isEmittingPrototypes_ = false;

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->is<FunctionDeclaration>())
                stmt->accept(*this);
        });
    }
    
    void CppGenerator::visit(Program& node)
    {
        emitStatements(node.statements);
    }

    void CppGenerator::visit(TypeSpecifier& node)
    {
        if (node.name.type == TokenType::kwFn)
        {//
            emit("std::function<");
            node.generics[0]->accept(*this);
            emit("(");
            
            for (size_t i = 1; i < node.generics.size(); ++i)
            {
                node.generics[i]->accept(*this);
                if (i < node.generics.size() - 1) emit(", ");
            }
            
            emit(")>");
        }
    }

    void CppGenerator::visit(BinaryExpression& node)
    {
        if (node.op.type == TokenType::kwIn)
        {
            if (node.right->is<RangeExpression>())
            {
                auto range = node.right->as<RangeExpression>();
                // C++ Output: [&](){ auto _val = (x); return _val >= (1) && _val <= (5); }()
                emit("[&](){ auto _val = (");
                node.left->accept(*this);
                emit("); return _val >= (");
                range->start->accept(*this);
                emit(range->isInclusive ? ") && _val <= (" : ") && _val < (");
                range->end->accept(*this);
                emit("); }() ");
                return;
            }
        }

        if (node.op.type == TokenType::kwIs)
        {
            node.left->accept(*this);
            
            bool isFatPointer = false;
            const auto& lhsType = node.left->refType.Lock();
            if (lhsType)
            {
                auto baseType = lhsType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                    
                if (baseType->kind() == sema::TypeKind::Struct &&
                    baseType.AsFast<sema::StructType>()->isInterface)
                {
                    isFatPointer = true;
                }
                else if (baseType->kind() == sema::TypeKind::Reference)
                {
                    auto rType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    if (rType && rType->kind() == sema::TypeKind::Struct &&
                        rType.AsFast<sema::StructType>()->isInterface)
                        isFatPointer = true;
                }
            }

            emit(isFatPointer ? "._WF_IsA(" : "->_WF_IsA(");

            if (node.right->is<Identifier>())
            {
                auto rightSym = node.right->as<Identifier>()->referencedSymbol.Lock();
                if (rightSym && rightSym->kind == sema::SymbolKind::Struct)
                {
                    auto sType = rightSym->type.AsFast<sema::StructType>();
                    if (sType->isInterface) 
                        emit(mangleInterfaceTypeName(sType) + "::TYPE_ID");
                    else 
                        emit(mangleStructTypeName(sType) + "::TYPE_ID");
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "{} not a type!", node.right->kindName());
                    emit("0 /* Error: Not a type! */");
                }
            }
            emit(")");
            return;//
        }
        
        emit("(");
        node.left->accept(*this);
        
        std::string opStr = node.op.value;
        if (node.op.type == TokenType::kwAnd)
            opStr = "&&";
        else if (node.op.type == TokenType::kwOr)
            opStr = "||";
        
        emit(" " + opStr + " ");
        node.right->accept(*this);
        emit(")");
    }

    void CppGenerator::visit(UnaryExpression& node)
    {
        std::string opStr = node.op.value;
        if (node.op.type == TokenType::kwNot) opStr = "!";
        
        if (node.opType == UnaryExpression::UnaryOperatorType::Prefix)
        {
            emit(opStr);
            node.operand->accept(*this);
        }
        else
        {
            node.operand->accept(*this);
            emit(opStr);
        }
    }

    void CppGenerator::visit(AssignmentExpression& node)
    {
        int derefCount = 0;

        auto lhsType = node.left->refType.Lock();
        auto rhsType = node.right->refType.Lock();

        if (lhsType && rhsType && !lhsType->isCompatibleWith(rhsType))
        {
            Ref<sema::Type> current = lhsType;
        
            while (current && current->kind() == sema::TypeKind::Reference)
            {
                auto rType = current.AsFast<sema::ReferenceType>();
                derefCount++;
            
                if (rType->referredType->isCompatibleWith(rhsType)) {
                    break;
                }
                current = rType->referredType;
            }
        }

        for (int i = 0; i < derefCount; ++i) emit("*(");
    
        node.left->accept(*this);
    
        for (int i = 0; i < derefCount; ++i) emit(")");

        emit(" " + node.op.value + " "); // =, +=, -= ...
    
        node.right->accept(*this);
    }

    void CppGenerator::visit(IntegerLiteral& node)
    {
        std::string valStr = node.token.value;
    
        const char* suffixes[] = { "i", "u", "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "isz", "usz" };
        for (const auto& suf : suffixes)
        {
            if (valStr.ends_with(suf))
            {
                valStr.erase(valStr.length() - strlen(suf));
                break;
            }
        }

        auto type = node.refType.Lock();
        std::string tName = type ? type->toString() : "i32";

        if (tName == "u32")
            emit(valStr + "u");
        else if (tName == "i64")
            emit(valStr + "ll");
        else if (tName == "u64" || tName == "usize")
            emit(valStr + "ull");
        else if (tName == "i8" || tName == "u8" || tName == "i16" || tName == "u16")
            emit("static_cast<" + type->toCppString() + ">(" + valStr + ")");
        else
            emit(valStr);
    }

    void CppGenerator::visit(FloatLiteral& node)
    {
        std::string valStr = node.token.value;
    
        if (valStr.ends_with("f32") || valStr.ends_with("f64"))
            valStr.erase(valStr.length() - 3);
        else if (valStr.ends_with('f'))
            valStr.erase(valStr.length() - 1);

        auto type = node.refType.Lock();
        if (type && type->toString() == "f64")
            emit(valStr); 
        else
            emit(valStr + "f"); 
    }

        void CppGenerator::visit(StringLiteral& node)
    {
        emit("\"" + common::wioStringToEscapedCppString(node.token.value) + "\"");
    }
    
    void CppGenerator::visit(InterpolatedStringLiteral& node)
    {
        if (node.parts.empty())
        {
            emit("\"\"");
            return;
        }

        std::string formatString;
        std::vector<Ref<Expression>> arguments;

        for (auto& part : node.parts)
        {
            if (auto strLiteral = part.As<StringLiteral>()) 
            {
                formatString += common::wioStringToEscapedCppString(strLiteral->token.value);
            }
            else 
            {
                formatString += "{}";
                arguments.push_back(part);
            }
        }

        emit("std::format(\"" + formatString + "\"");

        for (auto& arg : arguments)
        {
            emit(", ");
        
            int derefCount = 0;
            Ref<sema::Type> currentType = arg->refType.Lock();
        
            while (currentType && currentType->kind() == sema::TypeKind::Reference)
            {
                derefCount++;
                currentType = currentType.AsFast<sema::ReferenceType>()->referredType;
            }

            for (int i = 0; i < derefCount; ++i) emit("*(");
        
            arg->accept(*this);
        
            for (int i = 0; i < derefCount; ++i) emit(")");
        }
    
        emit(")");
    }

    void CppGenerator::visit(BoolLiteral& node)
    {
        emit(node.token.value);
    }
    
    void CppGenerator::visit(CharLiteral& node)
    {
        emit("\'" + common::wioStringToEscapedCppString(node.token.value) + "\'");
    }
    
    void CppGenerator::visit(ByteLiteral& node)
    {
        emit("static_cast<uint8_t>(" + node.token.value + ")");
    }
    
    void CppGenerator::visit(DurationLiteral& node)
    {
        std::string valStr = node.token.value; 
        double multiplier = 1.0;
        std::string numPart;
    
        if (valStr.ends_with("ms"))      { multiplier = 0.001;       numPart = valStr.substr(0, valStr.size() - 2); }
        else if (valStr.ends_with("us")) { multiplier = 0.000001;    numPart = valStr.substr(0, valStr.size() - 2); }
        else if (valStr.ends_with("ns")) { multiplier = 0.000000001; numPart = valStr.substr(0, valStr.size() - 2); }
        else if (valStr.ends_with("s"))  { multiplier = 1.0;         numPart = valStr.substr(0, valStr.size() - 1); }
        else if (valStr.ends_with("m"))  { multiplier = 60.0;        numPart = valStr.substr(0, valStr.size() - 1); }
        else if (valStr.ends_with("h"))  { multiplier = 3600.0;      numPart = valStr.substr(0, valStr.size() - 1); }
        else { numPart = valStr; }

        double value = std::stod(numPart) * multiplier;
        emit(std::to_string(value) + "f");
    }
    
    void CppGenerator::visit(ArrayLiteral& node)
    {
        const bool nested = !node.elements.empty() && node.elements[0].As<ArrayLiteral>();
        
        if (nested)
            emit("{");
        
        emit("{");
        for (size_t i = 0; i < node.elements.size(); ++i)
        {
            node.elements[i]->accept(*this);
            if (i < node.elements.size() - 1) emit(", ");
        }
        emit("}");

        if (nested)
            emit("}");
    }
    
    void CppGenerator::visit(DictionaryLiteral& node)
    {
        emit("{");
        for (size_t i = 0; i < node.pairs.size(); ++i)
        {
            emit("{ ");
            node.pairs[i].first->accept(*this);
            emit(", ");
            node.pairs[i].second->accept(*this);
            emit(" }");
            
            if (i < node.pairs.size() - 1) emit(", ");
        }
        emit("}");
    }

    void CppGenerator::visit(Identifier& node)
    {
        if (auto sym = node.referencedSymbol.Lock())
        {
            if (sym->kind == sema::SymbolKind::Namespace && node.token.value == "std")
            {
                emit("wio::runtime");
                return;
            }
            
            if (sym->flags.get_isStd() && (sym->kind == sema::SymbolKind::Function || sym->kind == sema::SymbolKind::FunctionGroup))
            {
                emit("b" + node.token.value);
                return;
            }

            if (sym->kind == sema::SymbolKind::Function)
            {
                auto funcType = sym->type.AsFast<sema::FunctionType>();
                emit(Mangler::mangleFunction(sym->name, funcType->paramTypes, sym->scopePath));
                return;
            }
            
            if (sym->kind == sema::SymbolKind::Variable && sym->flags.get_isGlobal())
            {
                emit(Mangler::mangleGlobalVar(sym->name, sym->scopePath));
                return;
            }

            if (sym->kind == sema::SymbolKind::Struct)
            {
                emit(Mangler::mangleStruct(sym->name, sym->scopePath));
                return;
            }
        }

        emit(node.token.value);
    }

    void CppGenerator::visit(NullExpression& node)
    {
        auto lockedRefType = node.refType.Lock();
        if (lockedRefType && lockedRefType->kind() == sema::TypeKind::Null)
        {
            emit(lockedRefType.AsFast<sema::NullType>()->transformedType->toCppString());
            emit("{}");
        }
        else
        {
            emit("nullptr");
        }
    }

    void CppGenerator::visit(ArrayAccessExpression& node)
    {
        // Wio: arr[0] -> C++: arr[0]
        node.object->accept(*this);
        emit("[");
        node.index->accept(*this);
        emit("]");
    }
    
    void CppGenerator::visit(MemberAccessExpression& node)
    {
        if (auto objSym = node.object->referencedSymbol.Lock();
            objSym && objSym->kind == sema::SymbolKind::Namespace && objSym->flags.get_isStd())
        {
            emit("wio::runtime::io::");
            node.member->accept(*this);
            return;
        }

        if (auto objSym = node.object->referencedSymbol.Lock();
            objSym && objSym->kind == sema::SymbolKind::Namespace && objSym->name != "std")
        {
            node.member->accept(*this);
            return;
        }

        if (node.object->is<SuperExpression>()) 
        {
            auto lockedType = node.object->refType.Lock();
            if (lockedType && lockedType->kind() == sema::TypeKind::Reference) 
            {
                auto refType = lockedType.AsFast<sema::ReferenceType>();
                auto baseStruct = refType->referredType.AsFast<sema::StructType>();
                
                emit(Mangler::mangleStruct(baseStruct->name, baseStruct->scopePath) + "::");
                node.member->accept(*this);
                return;
            }
        }
        
        node.object->accept(*this);
    
        std::string op = ".";
    
        if (auto objSym = node.object->referencedSymbol.Lock())
        {
            if (objSym->kind == sema::SymbolKind::Namespace || 
                objSym->flags.get_isEnum() || 
                objSym->flags.get_isFlagset())
            {
                op = "::";
            }
        }
    
        if (op == ".") 
        {
            if (auto objType = node.object->refType.Lock())
            {
                auto baseType = objType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
            
                bool isFatPointer = false;
                if (baseType->kind() == sema::TypeKind::Struct && baseType.AsFast<sema::StructType>()->isInterface)
                    isFatPointer = true;
                else if (baseType->kind() == sema::TypeKind::Reference) {
                    auto refType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    if (refType->kind() == sema::TypeKind::Struct && refType.AsFast<sema::StructType>()->isInterface)
                        isFatPointer = true;
                }

                if (isFatPointer)
                {
                    op = ".";
                }
                if (baseType->kind() == sema::TypeKind::Reference ||
                   (baseType->kind() == sema::TypeKind::Struct && (baseType.AsFast<sema::StructType>()->isObject || baseType.AsFast<sema::StructType>()->isInterface)))
                {
                    op = "->";
                }
            }
        }

        emit(op);
    
        node.member->accept(*this);
    }

    void CppGenerator::visit(FunctionCallExpression& node)
    {
        auto calleeType = node.callee->refType.Lock();
        
        if (calleeType && calleeType->kind() == sema::TypeKind::Struct)
        {
            auto structType = calleeType.AsFast<sema::StructType>();
            if (structType->isObject)
            {
                emit("wio::runtime::Ref<" + Mangler::mangleStruct(structType->name, structType->scopePath) + ">::Create(");
            }
            else
            {
                emit(Mangler::mangleStruct(structType->name, structType->scopePath) + "(");
            }
            
            for (size_t i = 0; i < node.arguments.size(); ++i)
            {
                node.arguments[i]->accept(*this);
                if (i < node.arguments.size() - 1)
                    emit(", ");
            }
            emit(")");
            return;
        }

        node.callee->accept(*this);
        emit("(");
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            node.arguments[i]->accept(*this);
            if (i < node.arguments.size() - 1)
                emit(", ");
        }
        emit(")");
    }

    void CppGenerator::visit(LambdaExpression& node)
    {
        emit("[&](");
        
        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            if (node.parameters[i].type)
                emit(toCppType(node.parameters[i].type->refType.Lock()));
            else
                emit("auto"); 
            emit(" ");
            emit(node.parameters[i].name->token.value);
            if (i < node.parameters.size() - 1)
                emit(", ");
        }
        emit(")");

        if (node.returnType)
        {
            emit(" -> ");
            node.returnType->accept(*this);
        }

        emit(" {\n");
        indent();

        if (node.body->is<ExpressionStatement>())
        {
            EMIT_TABS();
            emit("return ");
            node.body->as<ExpressionStatement>()->expression->accept(*this);
            emit(";\n");
        }
        else if (node.body->is<BlockStatement>())
        {
            auto block = node.body->as<BlockStatement>();
            for (auto& stmt : block->statements)
                stmt->accept(*this);
        }

        dedent();
        EMIT_TABS(); emit("}");
    }

    void CppGenerator::visit(RefExpression& node)
    {
        auto lockedType = node.operand->refType.Lock();
        bool isSmartPtrOrInterface = false;

        if (lockedType)
        {
            auto baseType = lockedType;
            while (baseType && baseType->kind() == sema::TypeKind::Alias)
                baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                
            if (baseType->kind() == sema::TypeKind::Struct)
            {
                auto sType = baseType.AsFast<sema::StructType>();
                if (sType->isObject || sType->isInterface) isSmartPtrOrInterface = true;
            }
            else if (baseType->kind() == sema::TypeKind::Reference)
            {
                auto refType = baseType.AsFast<sema::ReferenceType>()->referredType;
                if (refType->kind() == sema::TypeKind::Struct)
                {
                    auto sType = refType.AsFast<sema::StructType>();
                    if (sType->isObject || sType->isInterface) isSmartPtrOrInterface = true;
                }
            }
        }
        
        if (!isSmartPtrOrInterface) emit("&");
        node.operand->accept(*this);
    }
    
    void CppGenerator::visit(FitExpression& node)
    {
        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();


        if (srcType->isNumeric() && destType->isNumeric())
        {
            std::string cppDestType = destType->toCppString();
            std::string cppSrcType = srcType->toCppString();

            std::string minLimit = "static_cast<" + cppSrcType + ">(std::numeric_limits<" + cppDestType + ">::lowest())";
            std::string maxLimit = "static_cast<" + cppSrcType + ">(std::numeric_limits<" + cppDestType + ">::max())";

            emit("static_cast<" + cppDestType + ">(std::clamp<" + cppSrcType + ">(");
    
            node.operand->accept(*this);
    
            emit(", " + minLimit + ", " + maxLimit + "))");
        }
        else
        {
            std::string destCpp = toCppType(destType); 
            std::string typeIdStr;
            Ref<sema::StructType> sType;
            
            if (destType->kind() == sema::TypeKind::Reference) 
                sType = destType.AsFast<sema::ReferenceType>()->referredType.AsFast<sema::StructType>();
            else if (destType->kind() == sema::TypeKind::Struct) 
                sType = destType.AsFast<sema::StructType>();

            if (sType)
            {
                if (sType->isInterface)
                    typeIdStr = mangleInterfaceTypeName(sType) + "::TYPE_ID";
                else
                    typeIdStr = mangleStructTypeName(sType) + "::TYPE_ID";
            }

            if (sType && sType->isInterface)
            {
                emit(common::formatString("static_cast<{}>((", destCpp));
                node.operand->accept(*this);
                emit(common::formatString(")->_WF_CastTo({}))", typeIdStr));
            }
            else
            {
                std::string objectTypeName = mangleStructTypeName(sType);
                emit(common::formatString("wio::runtime::Ref<{}>(static_cast<{}*>((", objectTypeName, objectTypeName));
                node.operand->accept(*this);
                emit(common::formatString(")->_WF_CastTo({})))", typeIdStr));
            }
        }
    }

    void CppGenerator::visit(SelfExpression& node)
    {
        WIO_UNUSED(node);
        emit("this");
    }

    void CppGenerator::visit(SuperExpression& node)
    {
        auto lockedType = node.refType.Lock();
        
        if (lockedType && lockedType->kind() == sema::TypeKind::Reference)
        {
            auto refType = lockedType.AsFast<sema::ReferenceType>();
            auto baseStruct = refType->referredType.AsFast<sema::StructType>();
            
            std::string mangledBase = mangleStructTypeName(baseStruct);
            
            emit("static_cast<" + mangledBase + "*>(this)");
        }
        else
        {
            emit("this");
        }
    }

    void CppGenerator::visit(RangeExpression& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(MatchExpression& node)
    {
        bool producesValue = false;
        if (auto t = node.refType.Lock(); t)
        {
            producesValue = !t->isVoid() && !t->isUnknown();
        }

        emit("[&]() {\n");
        indent();
        EMIT_TABS();
        emit("auto _match_val = (");
        node.value->accept(*this);
        emit(");\n");
        
        bool first = true;
        for (auto& matchCase : node.cases)
        {
            EMIT_TABS();
            if (matchCase.matchValues.empty()) // assumed
            {
                emit("else {\n");
            }
            else
            {
                if (!first) emit("else ");
                emit("if (");
                
                for (size_t i = 0; i < matchCase.matchValues.size(); ++i)
                {
                    auto& mVal = matchCase.matchValues[i];
                    if (mVal->is<RangeExpression>())
                    {
                        auto r = mVal->as<RangeExpression>();
                        emit("(_match_val >= ");
                        r->start->accept(*this);
                        emit(r->isInclusive ? " && _match_val <= " : " && _match_val < ");
                        r->end->accept(*this);
                        emit(")");
                    }
                    else
                    {
                        emit("_match_val == ");
                        mVal->accept(*this);
                    }
                    
                    if (i < matchCase.matchValues.size() - 1)
                        emit(" || ");
                }
                emit(") {\n");
            }
            first = false;
            
            indent();
            
            if (producesValue && matchCase.body->is<ExpressionStatement>())
            {
                EMIT_TABS();
                emit("return ");
                matchCase.body->as<ExpressionStatement>()->expression->accept(*this);
                emit(";\n");
            }
            else
            {
                matchCase.body->accept(*this);
            }
            
            dedent();
            EMIT_TABS();
            emit("}\n");
        }
        
        dedent();
        EMIT_TABS();
        emit("}()");
    }

    void CppGenerator::visit(ExpressionStatement& node)
    {
        EMIT_TABS();
        node.expression->accept(*this);
        buffer_ << ";\n";
    }

    void CppGenerator::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(VariableDeclaration& node)
    {
        auto sym = node.name->referencedSymbol.Lock();

        Ref<sema::Type> varType = (sym && sym->type) ? sym->type : node.name->refType.Lock();
        std::string typeStr = toCppType(varType);
        std::string prefix;
        std::string suffix;

        if (node.mutability == Mutability::Const)
        {
            prefix = "constexpr ";
        }
        else if (node.mutability == Mutability::Immutable)
        {
            bool isStruct = (varType && varType->kind() == sema::TypeKind::Struct);
            if (!isStruct)
            {
                if (!typeStr.empty() && typeStr.back() == '*')
                    suffix = " const"; 
                else
                    prefix = "const ";
            }
        }
        EMIT_TABS();

        buffer_ << prefix << typeStr << suffix << " ";

        std::string varName = node.name->token.value;

        buffer_ << ((sym && sym->flags.get_isGlobal()) ? Mangler::mangleGlobalVar(varName, sym->scopePath) : varName);
        
        if (node.initializer)
        {
            buffer_ << " = ";
            node.initializer->accept(*this);
        }

        buffer_ << ";\n";
    }

    void CppGenerator::visit(FunctionDeclaration& node)
    {
        auto sym = node.name->referencedSymbol.Lock();
        auto funcType = sym->type.AsFast<sema::FunctionType>();

        std::string returnType = funcType->returnType ? toCppType(funcType->returnType) : "void";
        std::string funcName = node.name->token.value;

        if (funcName == "Entry" && (!sym || sym->scopePath.empty()))
        { 
            if (!isEmittingPrototypes_)
                emitMain(node);
            return;
        }

        emitLine();

        EMIT_TABS();

        if (!currentClassName_.empty())
        {
            if (funcName == "OnConstruct")
            {
                emit(currentClassName_ + "(");
            } else if (funcName == "OnDestruct")
            {
                emit("~" + currentClassName_ + "() ");
            }
            else
            {
                emit("virtual " + returnType + " "); 
                emit(Mangler::mangleFunction(funcName, funcType->paramTypes) + "(");
            }
        }
        else 
        {
            emit(returnType + " ");
            emit(Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : "") + "(");
        }

        if (funcName != "OnDestruct")
        {
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                auto& param = node.parameters[i];
                emit(common::formatString("{} {}", toCppType(param.name->refType.Lock()), param.name->token.value));
                if (i < node.parameters.size() - 1) emit(", ");
            }
            emit(")");

            if (!currentClassName_.empty())
            {
                if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
            }
        }

        if (isEmittingPrototypes_)
        {
            emitLine(";\n");
            return;
        }

        if (node.body)
        {
            emitLine();
            
            if (node.whenCondition) 
            {
                emitLine("{");
                indent();
                
                EMIT_TABS();
                emit("if (!(");
                node.whenCondition->accept(*this);
                emit(")) return");
                if (node.whenFallback)
                {
                    emit(" ");
                    node.whenFallback->accept(*this);
                }
                emit(";\n\n");
                
                if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    for (auto& stmt : block->statements)
                        stmt->accept(*this);
                }
                else
                {
                    node.body->accept(*this);
                }
                
                dedent();
                emitLine("}");
            }
            else 
            {
                node.body->accept(*this);
            }
        }
        else
        {
            emitLine(";\n");
        }
    }

    void CppGenerator::visit(RealmDeclaration& node)
    {
        emitStatements(node.statements);
    }

    void CppGenerator::visit(InterfaceDeclaration& node)
    {
        auto interfaceType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string interfaceName = mangleInterfaceTypeName(interfaceType);
        emitLine(common::formatString("struct {}", interfaceName));
        emitLine("{");
        indent();
    
        uint64_t typeId = common::fnv1a(interfaceName.c_str());
        emitLine(common::formatString("static constexpr uint64_t TYPE_ID = {}ull;", typeId));
        emitLine(common::formatString("virtual ~{}() = default;\n", interfaceName));
    
        for (auto& method : node.methods)
        {
            EMIT_TABS();
            auto sym = method->name->referencedSymbol.Lock();
            auto funcType = sym->type.AsFast<sema::FunctionType>();
            std::string retType = funcType->returnType ? toCppType(funcType->returnType) : "void";
            std::string mangledName = Mangler::mangleFunction(method->name->token.value, funcType->paramTypes);
        
            emit(common::formatString("virtual {} {}(", retType, mangledName));
            for (size_t i = 0; i < method->parameters.size(); ++i) {
                emit(common::formatString("{} {}", toCppType(method->parameters[i].name->refType.Lock()), method->parameters[i].name->token.value));
                if (i < method->parameters.size() - 1) emit(", ");
            }
            emit(") = 0;\n");
        }
    
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(ComponentDeclaration& node)
        {
        auto componentSym = node.name->referencedSymbol.Lock();
        auto componentType = getStructTypeFromSymbol(componentSym);
        auto enclosingScope = componentSym && componentSym->innerScope ? componentSym->innerScope->getParent().Lock() : nullptr;

        std::string structName = mangleStructTypeName(componentType);
        emit("struct " + structName);
        
        if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
        
        auto bases = getBaseInterfaces(node.attributes);
        if (!bases.empty())
        {
            emit(" : ");
            for (size_t i = 0; i < bases.size(); ++i)
            {
                auto baseSym = enclosingScope ? enclosingScope->resolve(bases[i]) : nullptr;
                std::string baseName = mangleNamedType(baseSym);
                if (baseName.empty())
                    baseName = Mangler::mangleInterface(bases[i]);

                emit("public " + baseName);
                if (i < bases.size() - 1) emit(", ");
            }
        }
        emitLine("\n{");
        indent();
    
        auto trustArgs = getAttributeArgs(node.attributes, Attribute::Trust);
        for (const auto& t : trustArgs)
        {
            if (t.type == TokenType::identifier)
            {
                auto trustSym = enclosingScope ? enclosingScope->resolve(t.value) : nullptr;
                std::string trustName = mangleNamedType(trustSym);
                if (trustName.empty())
                    trustName = Mangler::mangleStruct(t.value);

                emitLine("friend struct " + trustName + ";");
            }
        }
    
        currentClassName_ = structName;
        AccessModifier currentAccess = AccessModifier::Public;
    
        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                auto sym = vDecl->name->referencedSymbol.Lock();
                Ref<sema::Type> varType = (sym && sym->type) ? sym->type : vDecl->name->refType.Lock();
                memberVars.emplace_back(toCppType(varType), vDecl->name->token.value);
            }
        }
    
        bool hasCustomCtor = false;
        bool hasEmptyCtor = false;
        bool hasCopyCtor = false;
        bool hasMemberCtor = false;
    
        for (auto& member : node.members)
        {
            if (member.declaration->is<FunctionDeclaration>())
            {
                auto funcDecl = member.declaration->as<FunctionDeclaration>();
                if (funcDecl->name->token.value == "OnConstruct") 
                {
                    hasCustomCtor = true;
                    size_t pCount = funcDecl->parameters.size();
                    
                    if (pCount == 0) hasEmptyCtor = true;
                    else if (pCount == 1) 
                    {
                        std::string pType = toCppType(funcDecl->parameters[0].name->refType.Lock());
                        if (pType.find(currentClassName_) != std::string::npos) hasCopyCtor = true;
                    }
                    
                    if (pCount == memberVars.size() && !(pCount == 1 && hasCopyCtor)) 
                    {
                        bool typesMatch = true;
                        for (size_t i = 0; i < pCount; ++i) {
                            if (toCppType(funcDecl->parameters[i].name->refType.Lock()) != memberVars[i].first) {
                                typesMatch = false; break;
                            }
                        }
                        if (typesMatch) hasMemberCtor = true;
                    }
                }
            }
    
            if (member.access != currentAccess && member.access != AccessModifier::None)
            {
                dedent();
                if (member.access == AccessModifier::Public) emitLine("public:");
                else if (member.access == AccessModifier::Private) emitLine("private:");
                else if (member.access == AccessModifier::Protected) emitLine("protected:");
                indent();
                currentAccess = member.access;
            }
            member.declaration->accept(*this);
        }
    
        bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
        bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
    
        if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors) 
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
            }
    
            if (!hasEmptyCtor)
                emitLine(currentClassName_ + "() = default;");
                
            if (!hasCopyCtor)
                emitLine(currentClassName_ + "(const " + currentClassName_ + "&) = default;");
    
            if (!memberVars.empty() && !hasMemberCtor)
            {
                EMIT_TABS();
                emit(currentClassName_ + "(");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].first + " _" + memberVars[i].second);
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(") : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "(_" + memberVars[i].second + ")");
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(" {}\n");
            }
        }
    
        currentClassName_ = "";
        dedent();
        emitLine("};\n");
    }
    
    void CppGenerator::visit(ObjectDeclaration& node)
    {
        auto symb = node.name->referencedSymbol.Lock();
        auto objectType = getStructTypeFromSymbol(symb);
        std::string structName = mangleStructTypeName(objectType);
        emit("struct " + structName); 
        
        if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
        
        auto globalScope = symb->innerScope->getParent().Lock();
        
        auto bases = getBaseInterfaces(node.attributes);
    
        bool hasBaseObject = false;
        if (globalScope)
        {
            for (const auto& baseName : bases)
            {
                auto baseSym = globalScope->resolve(baseName);
                if (baseSym && !baseSym->flags.get_isInterface())
                {
                    hasBaseObject = true;
                    break;
                }
            }
        }
    
        std::string baseList;
    
        if (!hasBaseObject)
        {
            emit(" : public wio::runtime::RefCountedObject");
        }
        
        for (const auto& base : bases)
        {
            auto baseSym = globalScope ? globalScope->resolve(base) : nullptr;
            if (!baseList.empty()) baseList += ", ";
        
            if (baseSym && baseSym->flags.get_isInterface())
                baseList += "public " + mangleNamedType(baseSym);
            else
                baseList += "public " + (mangleNamedType(baseSym).empty() ? Mangler::mangleStruct(base) : mangleNamedType(baseSym));
        }
    
        if (!baseList.empty())
        {
            if (hasBaseObject) emitLine(" : " + baseList);
            else emitLine(", " + baseList);
        }
        emitLine("{");
        indent();
    
        uint64_t typeId = common::fnv1a(structName.c_str());
        emitLine(common::formatString("static constexpr uint64_t TYPE_ID = {}ull;", typeId));
        emitLine(common::formatString("virtual uint64_t _WF_GetTypeID() const {{ return {}ull; }}", typeId));
        
        emitLine("virtual bool _WF_IsA(uint64_t id) const override {");
        indent();
        emitLine(common::formatString("if (id == {}ull) return true;", typeId));
        for (const auto& base : bases) {
            auto baseSym = globalScope ? globalScope->resolve(base) : nullptr;
            if (baseSym && baseSym->flags.get_isInterface()) {
                emitLine(common::formatString("if (id == {}::TYPE_ID) return true;", mangleNamedType(baseSym)));
            } else {
                std::string baseName = mangleNamedType(baseSym);
                if (baseName.empty())
                    baseName = Mangler::mangleStruct(base);

                emitLine(common::formatString("if ({}::_WF_IsA(id)) return true;", baseName));
            }
        }
        emitLine("return false;");
        dedent();
        emitLine("}\n");
    
        emitLine("virtual void* _WF_CastTo(uint64_t id) override {");
        indent();
        emitLine(common::formatString("if (id == {}ull) return this;", typeId));
        for (const auto& base : bases)
        {
            auto baseSym = globalScope ? globalScope->resolve(base) : nullptr;
            if (baseSym && baseSym->flags.get_isInterface())
            {
                std::string intf = mangleNamedType(baseSym);
                emitLine(common::formatString("if (id == {}::TYPE_ID) return static_cast<{}*>(this);", intf, intf));
            }
            else
            {
                std::string bas = mangleNamedType(baseSym);
                if (bas.empty())
                    bas = Mangler::mangleStruct(base);
                emitLine(common::formatString("if (void* base_cast = {}::_WF_CastTo(id)) return base_cast;", bas));
            }
        }
        emitLine("return nullptr;");
        dedent();
        emitLine("}\n");
    
        emitLine("friend class wio::runtime::Ref<" + structName + ">;");
        auto trustArgs = getAttributeArgs(node.attributes, Attribute::Trust);
        for (const auto& t : trustArgs)
        {
            if (t.type == TokenType::identifier)
            {
                auto trustSym = globalScope ? globalScope->resolve(t.value) : nullptr;
                std::string trustName = mangleNamedType(trustSym);
                if (trustName.empty())
                    trustName = Mangler::mangleStruct(t.value);

                emitLine("friend struct " + trustName + ";");
            }
        }
    
        currentClassName_ = structName;
        AccessModifier currentAccess = AccessModifier::Public;
    
        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                const auto& sym = vDecl->name->referencedSymbol.Lock();
                Ref<sema::Type> varType = (sym && sym->type) ? sym->type : vDecl->name->refType.Lock();
                memberVars.emplace_back(toCppType(varType), vDecl->name->token.value);
            }
        }
    
        bool hasCustomCtor = false;
        bool hasEmptyCtor = false;
        bool hasCopyCtor = false;
        bool hasMemberCtor = false;
    
        for (auto& member : node.members)
        {
            if (member.declaration->is<FunctionDeclaration>())
            {
                auto funcDecl = member.declaration->as<FunctionDeclaration>();
                if (funcDecl->name->token.value == "OnConstruct") 
                {
                    hasCustomCtor = true;
                    size_t pCount = funcDecl->parameters.size();
                    
                    if (pCount == 0) hasEmptyCtor = true;
                    else if (pCount == 1) 
                    {
                        std::string pType = toCppType(funcDecl->parameters[0].name->refType.Lock());
                        if (pType.find(currentClassName_) != std::string::npos) hasCopyCtor = true;
                    }
                    
                    if (pCount == memberVars.size() && !(pCount == 1 && hasCopyCtor)) 
                    {
                        bool typesMatch = true;
                        for (size_t i = 0; i < pCount; ++i) {
                            if (toCppType(funcDecl->parameters[i].name->refType.Lock()) != memberVars[i].first) {
                                typesMatch = false; break;
                            }
                        }
                        if (typesMatch) hasMemberCtor = true;
                    }
                }
            }
    
            AccessModifier targetAccess = (member.access == AccessModifier::None) ? AccessModifier::Private : member.access;
    
            if (targetAccess != currentAccess)
            {
                dedent();
                if (targetAccess == AccessModifier::Public) emitLine("public:");
                else if (targetAccess == AccessModifier::Private) emitLine("private:");
                else if (targetAccess == AccessModifier::Protected) emitLine("protected:");
                indent();
                currentAccess = targetAccess;
            }
            member.declaration->accept(*this);
        }
    
        bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
        bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
    
        if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors) 
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
            }
    
            if (!hasEmptyCtor)
                emitLine(currentClassName_ + "() = default;");
                
            if (!hasCopyCtor)
                emitLine(currentClassName_ + "(const " + currentClassName_ + "&) = default;");
    
            if (!memberVars.empty() && !hasMemberCtor)
            {
                EMIT_TABS();
                emit(currentClassName_ + "(");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].first + " _" + memberVars[i].second);
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(") : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "(_" + memberVars[i].second + ")");
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(" {}\n");
            }
        }
    
        currentClassName_ = "";
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagDeclaration& node)
    {
        auto flagType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string structName = mangleStructTypeName(flagType);
        emitLine(common::formatString("struct {0} {{ explicit {0}() = default; };\n", structName));
    }

    void CppGenerator::visit(EnumDeclaration& node)
    {
        auto enumType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(enumType);
        std::string underType = "int32_t";
        
        auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type);
        if (!typeArgs.empty()) {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (typeArgs[0].type)
            {
                case TokenType::kwI8: underType = "int8_t"; break;
                case TokenType::kwU8: underType = "uint8_t"; break;
                case TokenType::kwI16: underType = "int16_t"; break;
                case TokenType::kwU16: underType = "uint16_t"; break;
                case TokenType::kwI32: underType = "int32_t"; break;
                case TokenType::kwU32: underType = "uint32_t"; break;
                case TokenType::kwI64: underType = "int64_t"; break;
                case TokenType::kwU64: underType = "uint64_t"; break;
                default: break;
            }
        }

        emitLine("enum " + enumName + " : " + underType + "\n{");
        indent();

        for (size_t i = 0; i < node.members.size(); ++i)
        {
            EMIT_TABS();
            emit(node.members[i].name->token.value);
            if (node.members[i].value)
            {
                emit(" = ");
                node.members[i].value->accept(*this);
            }
            
            if (i < node.members.size() - 1)
                emit(",");
            emit("\n");
        }
        
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagsetDeclaration& node)
    {
        auto flagsetType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(flagsetType);
        std::string underType = "uint32_t";
        
        auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type);
        if (!typeArgs.empty())
        {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (typeArgs[0].type)
            {
                case TokenType::kwI8: underType = "int8_t"; break;
                case TokenType::kwU8: underType = "uint8_t"; break;
                case TokenType::kwI16: underType = "int16_t"; break;
                case TokenType::kwU16: underType = "uint16_t"; break;
                case TokenType::kwI32: underType = "int32_t"; break;
                case TokenType::kwU32: underType = "uint32_t"; break;
                case TokenType::kwI64: underType = "int64_t"; break;
                case TokenType::kwU64: underType = "uint64_t"; break;
                default: break;
            }
        }

        emitLine("enum class " + enumName + " : " + underType + "\n{");
        indent();
        
        for (size_t i = 0; i < node.members.size(); ++i)
        {
            EMIT_TABS();
            emit(node.members[i].name->token.value);
            if (node.members[i].value)
            {
                emit(" = ");
                node.members[i].value->accept(*this);
            }
            
            if (i < node.members.size() - 1)
                emit(",");
            emit("\n");
        }
        
        dedent();
        emitLine("};");

        emitLine(common::formatString("inline constexpr {0} operator|({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) | static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator&({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) & static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator^({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) ^ static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator~({0} a) {{ return static_cast<{0}>(~static_cast<{1}>(a)); }}", enumName, underType));
    }

    void CppGenerator::visit(BlockStatement& node)
    {
        emitLine("{");
        indent();
        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }
        dedent();
        emitLine("}");
    }

    void CppGenerator::visit(IfStatement& node)
    {
        if (node.matchVar.isValid() && node.condition->is<BinaryExpression>())
        {
            auto binExpr = node.condition->as<BinaryExpression>();
            auto typeSym = binExpr->right->as<Identifier>()->referencedSymbol.Lock();
            auto sType = typeSym->type.AsFast<sema::StructType>();
            
            std::string destCpp = toCppType(typeSym->type);
            std::string typeIdStr = sType->isInterface
                ? (mangleInterfaceTypeName(sType) + "::TYPE_ID")
                : (mangleStructTypeName(sType) + "::TYPE_ID");

            EMIT_TABS();
            if (sType->isInterface)
            {
                emit(common::formatString("if ({} {} = static_cast<{}>((", destCpp, node.matchVar.value, destCpp));
                binExpr->left->accept(*this);
                emit(common::formatString(")->_WF_CastTo({})); {})", typeIdStr, node.matchVar.value));
            }
            else
            {
                std::string objectTypeName = mangleStructTypeName(sType);
                emit(common::formatString("if ({}* _raw_{} = static_cast<{}*>((", objectTypeName, node.matchVar.value, objectTypeName));
                binExpr->left->accept(*this);
                emit(common::formatString(")->_WF_CastTo({})))", typeIdStr));
            }
            emit("\n");
            emitLine("{");
            indent();
            
            if (!sType->isInterface)
            {
                emitLine(common::formatString("wio::runtime::Ref<{}> {}(_raw_{});", mangleStructTypeName(sType), node.matchVar.value, node.matchVar.value));
            }
            
            if (node.thenBranch) node.thenBranch->accept(*this);
            
            dedent();
            emitLine("}");
            
            if (node.elseBranch)
            {
                emit("else ");
                node.elseBranch->accept(*this);
            }
            return;
        }

        EMIT_TABS();
        emit("if (");
        node.condition->accept(*this);
        emit(")\n");
        
        if (node.thenBranch) {
            if (!node.thenBranch->is<BlockStatement>()) { emitLine("{"); indent(); }
            node.thenBranch->accept(*this);
            if (!node.thenBranch->is<BlockStatement>()) { dedent(); emitLine("}"); }
        }
        
        if (node.elseBranch)
        {
            emit("else ");
            node.elseBranch->accept(*this);
        }
    }
    
    void CppGenerator::visit(WhileStatement& node)
    {
        EMIT_TABS();
        buffer_ << "while (";
        node.condition->accept(*this);
        buffer_ << ")\n";
        
        node.body->accept(*this);
    }

    void CppGenerator::visit(BreakStatement& node)
    {
        WIO_UNUSED(node);
        emitLine("break;");
    }

    void CppGenerator::visit(ContinueStatement& node)
    {
        WIO_UNUSED(node);
        emitLine("continue;");
    }

    void CppGenerator::visit(ReturnStatement& node)
    {
        EMIT_TABS();
        
        buffer_ << "return";
        if (node.value)
        {
            buffer_ << " ";
            node.value->accept(*this);
        }
        buffer_ << ";\n";
    }

    void CppGenerator::visit(UseStatement& node)
    {
        if (node.isStdLib)
        {
            if (node.modulePath == "io")
            {
                emitHeaderLine("#include \"io.h\"");
            }
        }
    }
}
