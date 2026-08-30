// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    CppGenerator::CppGenerator() = default;

    bool CppGenerator::emitAnyBoxingIfNeeded(const NodePtr<Expression>& expression, const Ref<sema::Type>& expectedType)
    {
        if (!expression || !expectedType)
            return false;

        Ref<sema::Type> resolvedExpectedType = unwrapAliasTypeForCodegen(expectedType);
        if (resolvedExpectedType && resolvedExpectedType->kind() == sema::TypeKind::Reference)
            resolvedExpectedType = unwrapAliasTypeForCodegen(resolvedExpectedType.AsFast<sema::ReferenceType>()->referredType);

        if (!isAnyTypeForCodegen(resolvedExpectedType))
            return false;

        Ref<sema::Type> resolvedActualType = unwrapAliasTypeForCodegen(expression->refType.Lock());
        if (resolvedActualType && resolvedActualType->kind() == sema::TypeKind::Reference)
            resolvedActualType = unwrapAliasTypeForCodegen(resolvedActualType.AsFast<sema::ReferenceType>()->referredType);

        if (!resolvedActualType)
            return false;

        if (isAnyTypeForCodegen(resolvedActualType))
            return false;

        if (resolvedActualType->kind() == sema::TypeKind::Null)
        {
            emit("wio::runtime::Any()");
            return true;
        }

        if (isOpaqueTypeForCodegen(resolvedActualType))
        {
            emit("wio::runtime::Any::FromOpaque(");
            expression->accept(*this);
            emit(")");
            return true;
        }

        if (resolvedActualType->kind() == sema::TypeKind::Primitive)
        {
            auto primitiveType = resolvedActualType.AsFast<sema::PrimitiveType>();
            if (primitiveType && primitiveType->name == "string")
            {
                emit("wio::runtime::Any(");
                expression->accept(*this);
                emit(")");
                return true;
            }
        }

        if (resolvedActualType->kind() == sema::TypeKind::Struct)
        {
            auto structType = resolvedActualType.AsFast<sema::StructType>();
            if (!structType)
                return false;

            if (structType->isInterface)
            {
                emit("wio::runtime::Any::FromInterface<" + mangleInterfaceTypeName(structType) + ">(");
                expression->accept(*this);
                emit(")");
                return true;
            }

            if (structType->isObject)
            {
                emit("wio::runtime::Any::FromObject<" + mangleStructTypeName(structType) + ">(");
                expression->accept(*this);
                emit(")");
                return true;
            }
        }

        if (expression->is<ArrayLiteral>() || expression->is<DictionaryLiteral>())
        {
            emit("wio::runtime::Any::Box(");
            emit(toCppType(resolvedActualType));
            expression->accept(*this);
            emit(")");
            return true;
        }

        emit("wio::runtime::Any::Box(");
        expression->accept(*this);
        emit(")");
        return true;
    }

    void CppGenerator::emitReadableExpression(const NodePtr<Expression>& expression)
    {
        const std::size_t derefCount = getAutoReadableReferenceDepth(expression ? expression->refType.Lock() : nullptr);
        for (std::size_t i = 0; i < derefCount; ++i)
            emit("*(");

        if (expression)
            expression->accept(*this);

        for (std::size_t i = 0; i < derefCount; ++i)
            emit(")");
    }

    void CppGenerator::emitExpressionWithExpectedType(const NodePtr<Expression>& expression,
                                                      const Ref<sema::Type>& expectedType,
                                                      const bool allowAutoRef)
    {
        auto actualType = expression ? expression->refType.Lock() : nullptr;
        auto resolvedExpected = unwrapAliasTypeForCodegen(expectedType);
        auto resolvedActual = unwrapAliasTypeForCodegen(actualType);

        if (emitAnyBoxingIfNeeded(expression, expectedType))
            return;

        if (resolvedExpected &&
            resolvedExpected->kind() == sema::TypeKind::Function &&
            expression &&
            expression->is<LambdaExpression>())
        {
            emit(toCppType(expectedType));
            emit("(");
            expression->accept(*this);
            emit(")");
            return;
        }

        if (resolvedExpected &&
            resolvedExpected->kind() != sema::TypeKind::Reference &&
            shouldAutoReadReferenceType(actualType))
        {
            emitReadableExpression(expression);
            return;
        }

        if (resolvedExpected &&
            resolvedActual &&
            resolvedExpected->kind() == sema::TypeKind::Reference)
        {
            auto expectedRef = resolvedExpected.AsFast<sema::ReferenceType>();
            auto expectedTarget = unwrapAliasTypeForCodegen(expectedRef->referredType);

            // Keep the borrow wrapper visible at the call site. In particular,
            // C++ template argument deduction does not consider the converting
            // constructors from Ref<T> to BorrowedObject{Ref,View}<T>.
            if (expectedTarget && expectedTarget->kind() == sema::TypeKind::Struct)
            {
                auto expectedStruct = expectedTarget.AsFast<sema::StructType>();
                Ref<sema::Type> actualTarget = resolvedActual;
                if (actualTarget && actualTarget->kind() == sema::TypeKind::Reference)
                {
                    actualTarget = unwrapAliasTypeForCodegen(
                        actualTarget.AsFast<sema::ReferenceType>()->referredType
                    );
                }

                const bool actualIsObject =
                    actualTarget &&
                    actualTarget->kind() == sema::TypeKind::Struct &&
                    actualTarget.AsFast<sema::StructType>()->isObject;

                const bool actualCarriesReferenceLayer =
                    resolvedActual->kind() == sema::TypeKind::Reference;

                if (expectedStruct->isObject &&
                    actualIsObject &&
                    (!expectedRef->isMutable || actualCarriesReferenceLayer))
                {
                    emit(toCppType(expectedType));
                    emit("(");
                    expression->accept(*this);
                    emit(")");
                    return;
                }
            }

            if (allowAutoRef &&
                expectedTarget &&
                (expectedTarget->isCompatibleWith(resolvedActual) ||
                 (expectedTarget->isNumeric() && resolvedActual->isNumeric())))
            {
                bool isSmartPtrOrInterface = false;
                if (resolvedActual && resolvedActual->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedActual.AsFast<sema::StructType>();
                    isSmartPtrOrInterface = structType && (structType->isObject || structType->isInterface);
                }

                if (!isSmartPtrOrInterface)
                {
                    const bool materializeImmutableComponentView =
                        !expectedRef->isMutable &&
                        expectedTarget->kind() == sema::TypeKind::Struct &&
                        !expectedTarget.AsFast<sema::StructType>()->isObject &&
                        !expectedTarget.AsFast<sema::StructType>()->isInterface;

                    if (materializeImmutableComponentView)
                    {
                        // Binding the component expression to a const reference
                        // extends a temporary through the complete call expression.
                        // Emitting a raw &expression is invalid for extension calls
                        // such as Parse(...).Value().ToString().
                        emit("&static_cast<const ");
                        emit(toCppType(expectedTarget));
                        emit("&>(");
                        expression->accept(*this);
                        emit(")");
                        return;
                    }

                    emit("&");
                }
                expression->accept(*this);
                return;
            }

            if (!resolvedExpected->isCompatibleWith(resolvedActual))
            {
                if (expectedTarget && expectedTarget->kind() == sema::TypeKind::Struct)
                {
                    auto expectedStruct = expectedTarget.AsFast<sema::StructType>();
                    if (expectedStruct->isInterface)
                    {
                        bool canCastObjectHandle = resolvedActual && resolvedActual->kind() == sema::TypeKind::Struct;
                        if (!canCastObjectHandle && resolvedActual && resolvedActual->kind() == sema::TypeKind::Reference)
                            canCastObjectHandle = true;

                        if (canCastObjectHandle)
                        {
                            emit("static_cast<" + toCppType(expectedType) + ">((");
                            expression->accept(*this);
                            emit(")->_WF_CastTo(" + mangleInterfaceTypeName(expectedStruct) + "::TYPE_ID))");
                            return;
                        }
                    }
                }
            }
        }

        if (expression)
            expression->accept(*this);
    }

    Ref<sema::FunctionType> CppGenerator::getMangledCallableFunctionType(const Ref<sema::Symbol>& callableSymbol,
                                                                         const Ref<sema::FunctionType>& resolvedFunctionType,
                                                                         const size_t argumentCount) const
    {
        if (!callableSymbol)
            return resolvedFunctionType;

        Ref<sema::Type> declarationType = callableSymbol->type;
        if ((!declarationType || declarationType->kind() != sema::TypeKind::Function) &&
            callableSymbol->kind == sema::SymbolKind::FunctionGroup &&
            !callableSymbol->overloads.empty())
        {
            declarationType = callableSymbol->overloads.front()->type;
        }

        auto declarationFunctionType = declarationType ? declarationType.AsFast<sema::FunctionType>() : nullptr;
        if (!declarationFunctionType)
            return resolvedFunctionType;

        if (!callableSymbol->genericParameterNames.empty())
        {
            if (declarationFunctionType->hasParameterPack)
                return declarationFunctionType;

            return Compiler::get().getTypeContext().getOrCreateFunctionType(
                declarationFunctionType->returnType,
                getLeadingParameterTypes(declarationFunctionType, argumentCount)
            ).AsFast<sema::FunctionType>();
        }

        return declarationFunctionType;
    }

    std::string CppGenerator::generate(const Ref<Program>& program)
    {
        buffer_.str("");
        header_.str("");
        indentationLevel_ = 0;
        variableDeclarationsBySymbol_.clear();

        collectVariableDeclarationsBySymbol(program, variableDeclarationsBySymbol_);

        generateHeader();

        std::unordered_set<std::string> seenHeaders;
        std::vector<std::string> nativeHeaders;
        collectCppHeaders(program->statements, seenHeaders, nativeHeaders);
        for (const auto& header : nativeHeaders)
        {
            emitHeaderLine("#include \"" + common::wioStringToEscapedCppString(header) + "\"");
        }
        if (!nativeHeaders.empty())
            emitHeaderLine("");

        program->accept(*this);
        emitModuleApiTable(program);

        return header_.str() + buffer_.str();
    }

    void CppGenerator::generateHeader()
    {
        header_.str("");
        emitHeaderLine("// Generated by the Wio compiler.");
        emitHeaderLine("//");
        emitHeaderLine("// This file is an implementation artifact that can be inspected for");
        emitHeaderLine("// debugging, backend portability work, or native interop troubleshooting.");
        emitHeaderLine("// Source-level diagnostics are remapped back to .wio files through #line");
        emitHeaderLine("// directives, so backend failures should still point to Wio locations.");
        emitHeaderLine();
        emitHeaderLine("#include <cstdint>");
        emitHeaderLine("#include <cstddef>");
        emitHeaderLine("#include <algorithm>");
        emitHeaderLine("#include <limits>");
        emitHeaderLine("#include <string>");
        emitHeaderLine("#include <utility>");
        emitHeaderLine("#include <vector>");
        emitHeaderLine("#include <array>");
        emitHeaderLine("#include <chrono>");
        emitHeaderLine("#include \"format.h\"");
        emitHeaderLine("#include <iostream>");
        emitHeaderLine("#include <functional>");
        emitHeaderLine("#include <map>");
        emitHeaderLine("#include <memory>");
        emitHeaderLine("#include <new>");
        emitHeaderLine("#include <stdexcept>");
        emitHeaderLine("#include <thread>");
        emitHeaderLine("#include <unordered_map>");
        emitHeaderLine();
        emitHeaderLine("#include <exception.h>");
        emitHeaderLine("#include <constant_value.h>");
        emitHeaderLine("#include <any.h>");
        emitHeaderLine("#include <enum_reflection.h>");
        emitHeaderLine("#include <type_reflection.h>");
        emitHeaderLine("#include <fit.h>");
        emitHeaderLine("#include <intrinsics.h>");
        emitHeaderLine("#include <meta.h>");
        emitHeaderLine("#include <module_api.h>");
        emitHeaderLine("#include <wio_values.h>");
        emitHeaderLine("#include <ref.h>");
        emitHeaderLine("#include <std_async.h>");
        emitHeaderLine("#include <text.h>");
        emitHeaderLine();
        emitHeaderLine("namespace wio::runtime");
        emitHeaderLine("{");
        emitHeaderLine("    std::vector<std::string> CollectEntryArguments(int argc, char* const argv[]);");
        emitHeaderLine("}");
        emitHeaderLine();
        emitHeaderLine("#if defined(_WIN32)");
        emitHeaderLine("#define WIO_EXPORT __declspec(dllexport)");
        emitHeaderLine("#else");
        emitHeaderLine("#define WIO_EXPORT __attribute__((visibility(\"default\")))");
        emitHeaderLine("#endif");
        emitHeaderLine();
        emitHeaderLine("namespace wio");
        emitHeaderLine("{");
        indent();
        emitHeaderLine("using String = std::string;");
        emitHeaderLine("using WString = std::wstring;");
        dedent();
        emitHeaderLine("}");
        emitHeaderLine();
        emitHeaderLine("namespace wio");
        emitHeaderLine("{");
        indent();
        emitHeaderLine("template <typename T>");
        emitHeaderLine("using DArray = std::vector<T>;");
        emitHeaderLine();
        emitHeaderLine("template <typename T, size_t N>");
        emitHeaderLine("using SArray = std::array<T, N>;");
        emitHeaderLine();
        emitHeaderLine("template <typename K, typename V>");
        emitHeaderLine("using Dict = std::unordered_map<K, V>;");
        emitHeaderLine();
        emitHeaderLine("template <typename K, typename V>");
        emitHeaderLine("using Tree = std::map<K, V>;");
        dedent();
        emitHeaderLine("}");
        emitHeaderLine();

    }
