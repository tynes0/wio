#include "wio/codegen/cpp_generator.h"

#include "wio/common/filesystem/filesystem.h"
#include "compiler.h"
#include "wio/sema/symbol.h"

namespace wio::codegen
{
    namespace 
    {
        std::string toCppType(const Ref<sema::Type>& type)
        {
            if (!type) return "void"; // Fallback
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
        emitHeaderLine("#include <exception.h>");
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
        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
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
        
        if (node.parameters.empty())
        {
            emitLine("int main() {");
        }
        else
        {
            if (node.parameters.size() > 1)
            {
                throw InvalidEntryParameter("The `Entry` function should have only one parameter. (string[])", node.location());
            }

            Parameter& param = node.parameters.front();
            auto lockedParamRefType = param.name->refType.Lock();
            if (lockedParamRefType->toString() != "string[]")
            {
                throw InvalidEntryParameter("The `Entry` functions parameter type must be `string[]`", node.location());
            }

            emit("int main(");
            emit(common::formatString("{} {}", toCppType(lockedParamRefType), param.name->token.value));
            emitLine(") {");
        }
        indent();

        emitLine("try");
        node.body->accept(*this);
        emitLine("catch (const wio::runtime::RuntimeException& ex)"); 
        emitLine("{");
        indent();
        emitLine(R"(std::cout << ex.what() << '\n';)"); 
        emitLine("return 1;");
        dedent();
        emitLine("}"); 
        dedent();
        emitLine("}"); 
    }

    void CppGenerator::indent() { indentationLevel_++; }
    void CppGenerator::dedent() { indentationLevel_--; }
    
    void CppGenerator::visit(Program& node)
    {
        // TURN 0: Imports (Use Statements)
        for (auto& stmt : node.statements)
            if (stmt->is<UseStatement>()) stmt->accept(*this);
        emitLine("");
        
        // TURN 1: Interfaces
        for (auto& stmt : node.statements)
            if (stmt->is<InterfaceDeclaration>()) stmt->accept(*this);
        emitLine("");

        // TURN 2: Component & Object (Forward Decl)
        for (auto& stmt : node.statements)
        {
            if (stmt->is<ComponentDeclaration>())
                emitLine(std::format("struct {};", Mangler::mangleStruct(stmt->as<ComponentDeclaration>()->name->token.value)));
            else if (stmt->is<ObjectDeclaration>())
                emitLine(std::format("struct {};", Mangler::mangleStruct(stmt->as<ObjectDeclaration>()->name->token.value)));
        }
        emitLine("");

        // TURN 3: Global Vars
        for (auto& stmt : node.statements)
            if (stmt->is<VariableDeclaration>()) stmt->accept(*this);
        emitLine("");

        // TUR 4: Function Prototypes
        isEmittingPrototypes_ = true; 
        for (auto& stmt : node.statements)
            if (stmt->is<FunctionDeclaration>()) stmt->accept(*this);
        
        isEmittingPrototypes_ = false;
        emitLine("");

        // TURN 5:  Object & Component Bodies
        for (auto& stmt : node.statements)
            if (stmt->is<ComponentDeclaration>() || stmt->is<ObjectDeclaration>())
                stmt->accept(*this);
        
        emitLine("");

        // TUR 6: Function Bodies & Entry (Main)
        for (auto& stmt : node.statements)
            if (stmt->is<FunctionDeclaration>()) stmt->accept(*this);
    }

    void CppGenerator::visit(TypeSpecifier& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(BinaryExpression& node)
    {
        emit("(");
        node.left->accept(*this);
        emit(" " + node.op.value + " ");
        node.right->accept(*this);
        emit(")");
    }

    void CppGenerator::visit(UnaryExpression& node)
    {
        if (node.opType == UnaryExpression::UnaryOperatorType::Prefix)
        {
            emit(node.op.value);
            node.operand->accept(*this);
        }
        else
        {
            node.operand->accept(*this);
            emit(node.op.value);
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
    
        const char* suffixes[] = { "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64", "isz", "usz" };
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
        // TODO: impl
        WIO_UNUSED(node);
    }
    
    void CppGenerator::visit(LambdaLiteral& node)
    {
        // TODO: impl
        WIO_UNUSED(node);
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
                emit(Mangler::mangleFunction(sym->name, funcType->paramTypes));
                return;
            }
            
            if (sym->kind == sema::SymbolKind::Variable && sym->flags.get_isGlobal())
            {
                emit(Mangler::mangleGlobalVar(sym->name));
                return;
            }

            if (sym->kind == sema::SymbolKind::Struct)
            {
                emit(Mangler::mangleStruct(sym->name));
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
        node.object->accept(*this);
    
        std::string op = ".";
    
        if (auto objSym = node.object->referencedSymbol.Lock())
        {
            if (objSym->kind == sema::SymbolKind::Namespace)
                op = "::";
        }
    
        if (op == ".") 
        {
            if (auto objType = node.object->refType.Lock())
            {
                auto baseType = objType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
            
                if (baseType && baseType->kind() == sema::TypeKind::Reference)
                    op = "->";
            }
        }

        emit(op);
    
        node.member->accept(*this);
    }

    void CppGenerator::visit(FunctionCallExpression& node)
    {
        node.callee->accept(*this);
        emit("(");
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            node.arguments[i]->accept(*this);
            if (i < node.arguments.size() - 1) emit(", ");
        }
        emit(")");
    }

    void CppGenerator::visit(RefExpression& node)
    {
        emit("&");
        node.operand->accept(*this);
    }
    
    void CppGenerator::visit(FitExpression& node)
    {
        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();

        std::string cppDestType = destType->toCppString();
        std::string cppSrcType = srcType->toCppString();

        std::string minLimit = "static_cast<" + cppSrcType + ">(std::numeric_limits<" + cppDestType + ">::lowest())";
        std::string maxLimit = "static_cast<" + cppSrcType + ">(std::numeric_limits<" + cppDestType + ">::max())";

        emit("static_cast<" + cppDestType + ">(std::clamp<" + cppSrcType + ">(");
    
        node.operand->accept(*this);
    
        emit(", " + minLimit + ", " + maxLimit + "))");
    }

    void CppGenerator::visit(ExpressionStatement& node)
    {
        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
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
            if (!typeStr.empty() && typeStr.back() == '*')
                suffix = " const"; 
            else
                prefix = "const ";
        }

        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";

        buffer_ << prefix << typeStr << suffix << " ";

        std::string varName = node.name->token.value;

        buffer_ << ((sym && sym->flags.get_isGlobal()) ? Mangler::mangleGlobalVar(varName) : varName);
        
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

        if (funcName == "Entry")
        { 
            if (!isEmittingPrototypes_)
                emitMain(node);
            return;
        }

        emitLine();

        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";

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
            emit(Mangler::mangleFunction(funcName, funcType->paramTypes) + "(");
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
        }

        if (isEmittingPrototypes_)
        {
            emitLine(";\n");
            return;
        }

        if (node.body)
        {
            emitLine();
            node.body->accept(*this);
        }
        else
        {
            emitLine(";\n");
        }
    }

    void CppGenerator::visit(InterfaceDeclaration& node)
    {
        std::string interfaceName = Mangler::mangleInterface("", node.name->token.value);
        emitLine("struct " + interfaceName);
        emitLine("{");
        indent();
        
        emitLine("virtual ~" + interfaceName + "() = default;\n");

        for (auto& method : node.methods)
        {
            auto sym = method->name->referencedSymbol.Lock();
            auto funcType = sym->type.AsFast<sema::FunctionType>();
            std::string retType = funcType->returnType ? toCppType(funcType->returnType) : "void";

            for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
            emit("virtual " + retType + " " + Mangler::mangleFunction(method->name->token.value, funcType->paramTypes) + "(");
            
            for (size_t i = 0; i < method->parameters.size(); ++i)
            {
                auto& param = method->parameters[i];
                emit(common::formatString("{} {}", toCppType(param.name->refType.Lock()), param.name->token.value));
                if (i < method->parameters.size() - 1) emit(", ");
            }
            emit(") = 0;\n");
        }

        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(ComponentDeclaration& node)
    {
        std::string structName = Mangler::mangleStruct(node.name->token.value);
        emit("struct " + structName);
        
        auto bases = getBaseInterfaces(node.attributes);
        if (!bases.empty())
        {
            emit(" : ");
            for (size_t i = 0; i < bases.size(); ++i)
            {
                emit("public " + Mangler::mangleInterface("", bases[i]));
                if (i < bases.size() - 1) emit(", ");
            }
        }
        emitLine("\n{");
        indent();

        currentClassName_ = structName;
        AccessModifier currentAccess = AccessModifier::Public;

        for (auto& member : node.members)
        {
            if (member.access != currentAccess && member.access != AccessModifier::None)
            {
                dedent();
                if (member.access == AccessModifier::Public)
                    emitLine("public:");
                else if (member.access == AccessModifier::Private)
                    emitLine("private:");
                else if (member.access == AccessModifier::Protected)
                    emitLine("protected:");
                indent();
                currentAccess = member.access;
            }
            member.declaration->accept(*this);
        }

        currentClassName_ = "";
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(ObjectDeclaration& node)
    {
        std::string structName = Mangler::mangleStruct(node.name->token.value);
        
        emit("struct " + structName); 
        
        auto bases = getBaseInterfaces(node.attributes);
        if (!bases.empty()) {
            emit(" : ");
            for (size_t i = 0; i < bases.size(); ++i) {
                emit("public " + Mangler::mangleInterface("", bases[i]));
                if (i < bases.size() - 1) emit(", ");
            }
        }
        emitLine("\n{");
        indent();

        currentClassName_ = structName;
        AccessModifier currentAccess = AccessModifier::Public;

        for (auto& member : node.members)
        {
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

        currentClassName_ = "";
        dedent();
        emitLine("};\n");
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
        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
        buffer_ << "if (";
        node.condition->accept(*this);
        buffer_ << ")\n";
        
        node.thenBranch->accept(*this);

        if (node.elseBranch)
        {
            for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
            buffer_ << "else\n";
            node.elseBranch->accept(*this);
        }
    }
    
    void CppGenerator::visit(WhileStatement& node)
    {
        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
        buffer_ << "while (";
        node.condition->accept(*this);
        buffer_ << ")\n";
        
        node.body->accept(*this);
    }

    void CppGenerator::visit(ReturnStatement& node)
    {
        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
        
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
        else
        {
            emitHeaderLine("#include \"" + node.modulePath + ".h\"");
        }
    }
}
