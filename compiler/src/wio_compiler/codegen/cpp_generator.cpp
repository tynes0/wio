#include "wio/codegen/cpp_generator.h"

#include <Windows.h>

#include "wio/common/filesystem/filesystem.h"
#include "compiler.h"

namespace wio::codegen
{
    CppGenerator::CppGenerator() = default;

    std::string CppGenerator::generate(Ref<Program> program)
    {
        buffer_.str("");
        indentationLevel_ = 0;

        generateHeader();
        program->accept(*this);

        return header_.str() + buffer_.str();
    }

    // TODO: Refactor
    void CppGenerator::generateHeader()
    {
        header_.str("");
        header_ << "#include <cstdint>\n";
        header_ << "#include <string>\n";
        header_ << "#include <vector>\n";
        header_ << "#include <array>\n";
        header_ << "#include <format>\n";
        header_ << "#include <iostream>\n"; // todo: remove
        header_ << "#include <exception.h>\n";
        header_ << '\n';
        header_ << "namespace wio\n{\n\tusing String = std::string;\n\tusing WString = std::wstring;\n}\n\n";
        header_ << "namespace wio\n{\n\ttemplate <typename T>\n\tusing DArray = std::vector<T>;\n\n\ttemplate <typename T, size_t N>\n\tusing SArray = std::array<T, N>;\n}\n";
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
        emitLine("catch (const wio::runtime::RuntimeException& ex)"); // try {
        emitLine("{");
        indent();
        emitLine(R"(std::cout << ex.what() << '\n';)"); // TODO: Refactor with own logging system
        dedent();
        emitLine("}"); // catch {
        dedent();
        emitLine("}"); // int main(...) {
    }

    void CppGenerator::indent() { indentationLevel_++; }
    void CppGenerator::dedent() { indentationLevel_--; }

    std::string CppGenerator::toCppType(const Ref<sema::Type>& type)
    {
        if (!type) return "void"; // Fallback
        return type->toCppString();
    }
    
    void CppGenerator::visit(Program& node)
    {
        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }
    }

    void CppGenerator::visit(FunctionDeclaration& node)
    {
        std::string returnType = node.name->refType.Lock()
        ? toCppType(node.name->refType.Lock().AsFast<sema::FunctionType>()->returnType)
        : "void";
        
        std::string funcName = node.name->token.value;

        // Todo: Mangling will be added here later: Mangler::mangle(...)mangle
        
        if (funcName == "Entry")
        { 
            emitMain(node);
            return;
        }

        emitLine();
        emit(returnType + " " + funcName + "(");

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            emit(common::formatString("{} {}", toCppType(param.name->refType.Lock()), param.name->token.value));
            if (i < node.parameters.size() - 1) emit(", ");
        }

        emit(")\n");
        
        if (node.body)
        {
            node.body->accept(*this);
        }
        else
        {
            emitLine(";");
        }
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

    void CppGenerator::visit(VariableDeclaration& node)
    {
        std::string typeStr = toCppType(node.name->refType.Lock());
        std::string prefix;

        if (node.mutability == Mutability::Const)
            prefix = "const ";

        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";

        buffer_ << prefix << typeStr << " " << node.name->token.value;

        if (node.initializer)
        {
            buffer_ << " = ";
            node.initializer->accept(*this);
        }

        buffer_ << ";\n";
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

    void CppGenerator::visit(ExpressionStatement& node)
    {
        for (int i = 0; i < indentationLevel_; ++i) buffer_ << "    ";
        node.expression->accept(*this);
        buffer_ << ";\n";
    }

    // ==========================================
    // (Expressions)
    // ==========================================

    void CppGenerator::visit(IntegerLiteral& node)
    {
        emit(node.token.value);
    }

    void CppGenerator::visit(FloatLiteral& node)
    {
        emit(node.token.value);
    }
    
    void CppGenerator::visit(BoolLiteral& node)
    {
        emit(node.token.value);
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
            
            if (sym->flags.get_isStd() && sym->kind == sema::SymbolKind::Function)
            {
                emit("b" + node.token.value);
                return;
            }
        }

        emit(node.token.value);
    }

    void CppGenerator::visit(BinaryExpression& node)
    {
        emit("(");
        node.left->accept(*this);
        emit(" " + node.op.value + " ");
        node.right->accept(*this);
        emit(")");
    }
    
    void CppGenerator::visit(AssignmentExpression& node)
    {
        node.left->accept(*this);
        emit(" " + node.op.value + " "); // =, +=, -= ...
        node.right->accept(*this);
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

    void CppGenerator::visit(TypeSpecifier& node)
    {
        // I guess we do not need this func
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(UnaryExpression& node)
    {
        if (node.opType == UnaryExpression::UnaryOperatorType::Prefix)
        {
            emit(node.op.value); // "!" or "-"
            node.operand->accept(*this);
        }
        else
        {
            node.operand->accept(*this);
            emit(node.op.value);
        }
    }
    
    void CppGenerator::visit(StringLiteral& node)
    {
        emit("\"" + node.token.value + "\"");
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
            if (auto strLiteral = part.As<StringLiteral>(); strLiteral) 
            {
                formatString += strLiteral->token.value;
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
            arg->accept(*this);
        }
    
        emit(")");
    }
    
    void CppGenerator::visit(CharLiteral& node)
    {
        emit("\'" + node.token.value + "\'");
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
        // Wio: [1, 2, 3] -> C++: {1, 2, 3}

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
        // MAP OR U MAP?
    }
    
    void CppGenerator::visit(LambdaLiteral& node)
    {
        
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
    
    void CppGenerator::visit(RefExpression& node)
    {
        emit("&");
        node.operand->accept(*this);
    }

    void CppGenerator::visit(UseStatement& node)
    {
        if (node.isStdLib)
        {
            if (node.modulePath == "io")
            {
                header_ << "#include \"io.h\"\n";
            }
        }
        else
        {
            // Kullanıcı tanımlı bir modül ise (Örn: use "my_lib";)
            // İleride Wio'nun çoklu dosya derleme (multi-file compilation) sistemini 
            // nasıl kurduğuna bağlı olarak uzantıyı ayarlayabilirsin (.h, .hpp veya .wio.h)
            header_ << "#include \"" << node.modulePath << ".h\"\n";
        }
    }

    void CppGenerator::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }
}
