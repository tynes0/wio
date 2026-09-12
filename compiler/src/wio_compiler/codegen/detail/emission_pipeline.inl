// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

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

    void CppGenerator::emitSourceDirective(const common::Location& loc)
    {
        if (!loc.hasSourceContext())
            return;

        emitLine("#line " + std::to_string(loc.line) + " \"" + common::wioStringToEscapedCppString(loc.file) + "\"");
    }

    void CppGenerator::emitGeneratedDirective()
    {
        emitLine("#line 1 \"<wio-generated>\"");
    }

    void CppGenerator::emitMain(FunctionDeclaration& node)
    {
        Ref<sema::Type> lockedRefType = nullptr;
        if (auto funcSym = node.name ? node.name->referencedSymbol.Lock() : nullptr)
        {
            if (auto funcType = funcSym->type.AsFast<sema::FunctionType>())
                lockedRefType = funcType->returnType;
        }

        if (!lockedRefType && node.returnType)
            lockedRefType = node.returnType->refType.Lock();

        if (!lockedRefType)
            lockedRefType = Compiler::get().getTypeContext().getVoid();

        if (node.isAsync)
        {
            Ref<sema::Type> resolvedTaskType = unwrapAliasTypeForCodegen(lockedRefType);
            if (resolvedTaskType && resolvedTaskType->kind() == sema::TypeKind::AsyncTask)
                lockedRefType = resolvedTaskType.AsFast<sema::AsyncTaskType>()->valueType;
        }

        if (lockedRefType->toString() != "i32" && lockedRefType->toString() != "void")
        {
            throw InvalidEntryReturnType("Entry return type must be i32 or void.", node.location());
        }
        if(!node.body)
        {
            throw MissedEntryBody("The Entry function must have a body.", node.location());
        }

        if (node.isApplicationEntry)
        {
            const std::string cppType = Mangler::mangleStruct(node.applicationName);
            const std::string lifecyclePrefix = "_WF___extension_" + node.applicationName + "Lifecycle_";
            const std::string receiverSuffix = "_ref_mut_" + node.applicationName;
            const std::string startFunction = lifecyclePrefix + "Start" + receiverSuffix;
            const std::string updateFunction = lifecyclePrefix + "Update" + receiverSuffix + "_f64";
            const std::string closeFunction = lifecyclePrefix + "Close" + receiverSuffix;

            emitGeneratedDirective();
            emitLine();
            emitLine("int main()");
            emitLine("{");
            indent();
            emitLine(cppType + " application{};");
            emitLine("bool applicationStarted = false;");
            emitLine("bool applicationClosed = false;");
            emitLine("auto closeApplication = [&]() noexcept");
            emitLine("{");
            indent();
            emitLine("if (!applicationStarted || applicationClosed) return;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(closeFunction + "(&application);");
            emitLine("applicationClosed = true;");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            dedent();
            emitLine("}");
            emitLine("catch (...) { applicationClosed = true; }");
            dedent();
            emitLine("};");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::BindAsyncMainExecutor();");
            emitLine("applicationStarted = true;");
            emitLine(startFunction + "(&application);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("auto previousFrame = std::chrono::steady_clock::now();");
            emitLine("while (!application.__exitRequested)");
            emitLine("{");
            indent();
            emitLine("const auto currentFrame = std::chrono::steady_clock::now();");
            emitLine("const double elapsed = std::chrono::duration<double>(currentFrame - previousFrame).count();");
            emitLine("previousFrame = currentFrame;");
            emitLine("const double deltaSeconds = std::clamp(elapsed, 0.0, 0.25);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(updateFunction + "(&application, deltaSeconds);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            dedent();
            emitLine("}");
            emitLine("closeApplication();");
            emitLine("wio::runtime::ShutdownAsyncRuntime();");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("return application.__exitCode;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("closeApplication();");
            emitLine("wio::runtime::ShutdownAsyncRuntime();");
            emitLine("std::cout << \"Runtime Error: \" << error.what() << '\\n';");
            emitLine("return 1;");
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine("closeApplication();");
            emitLine("wio::runtime::ShutdownAsyncRuntime();");
            emitLine("std::cout << \"Runtime Error: Unknown native exception\" << '\\n';");
            emitLine("return 1;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            return;
        }

        emitGeneratedDirective();
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

            paramName = sanitizeCppIdentifier(param.name->token.value);
            hasArgs = true;

            emitLine("int main(int argc, char** argv) {");
        }
        indent();

        if (hasArgs)
        {
            emitLine("wio::DArray<wio::String> " + paramName + ";");
            emitLine("auto _wio_entry_arguments = wio::runtime::CollectEntryArguments(argc, argv);");
            emitLine(paramName + ".reserve(_wio_entry_arguments.size());");
            emitLine("for (const auto& _wio_entry_argument : _wio_entry_arguments) {");
            indent();
            emitLine(paramName + ".push_back(wio::String(_wio_entry_argument));");
            dedent();
            emitLine("}");
            emitLine("");
        }

        emitLine("try {");
        indent();

        if (node.isAsync)
        {
            auto funcSym = node.name ? node.name->referencedSymbol.Lock() : nullptr;
            auto funcType = funcSym ? funcSym->type.AsFast<sema::FunctionType>() : nullptr;
            const std::string entrySymbol = Mangler::mangleFunction(
                node.name->token.value,
                funcType ? funcType->paramTypes : std::vector<Ref<sema::Type>>{},
                funcSym ? funcSym->scopePath : ""
            );
            EMIT_TABS();
            if (!lockedRefType->isVoid())
                emit("return ");
            emit("wio::runtime::BlockOn(" + entrySymbol + "(");
            if (hasArgs)
                emit(paramName);
            emitLine("));");
            if (lockedRefType->isVoid())
                emitLine("return 0;");
        }
        else if (node.body->is<BlockStatement>())
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
        emitLine("catch (const std::exception& ex)");
        emitLine("{");
        indent();
        emitLine(R"(std::cout << "Runtime Error: Unhandled native exception: " << ex.what() << '\n';)");
        emitLine("return 1;");
        dedent();
        emitLine("}");
        emitLine("catch (...)");
        emitLine("{");
        indent();
        emitLine(R"(std::cout << "Runtime Error: Unknown native exception" << '\n';)");
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

                if (stmt->is<DeclarationGroup>())
                {
                    self(self, stmt->as<DeclarationGroup>()->declarations, emitter);
                    continue;
                }

                if (stmt->is<UsingAttributeStatement>())
                {
                    auto usingAttribute = stmt->as<UsingAttributeStatement>();
                    if (usingAttribute->body)
                        self(self, usingAttribute->body->declarations, emitter);
                    continue;
                }

                emitter(stmt);
            }
        };

        auto emitTemplateForwardDeclarationPrefix = [&](const std::vector<NodePtr<Identifier>>& genericParameters)
        {
            if (genericParameters.empty())
                return;

            std::string templateLine = "template <";
            for (size_t i = 0; i < genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack =
                    genericParameters.size() > 0 &&
                    i + 1 == genericParameters.size() &&
                    genericParameters[i] &&
                    genericParameters[i]->refType.Lock() &&
                    genericParameters[i]->refType.Lock()->kind() == sema::TypeKind::GenericParameterPack;
                templateLine += formatCppTemplateParameter(genericParameters[i], isGenericParameterPack);
                if (i + 1 < genericParameters.size())
                    templateLine += ", ";
            }
            templateLine += ">";
            emitLine(templateLine);
        };

        auto getEnumUnderlyingCppType = [&](const std::vector<NodePtr<AttributeStatement>>& attributes,
                                            const std::string& fallbackType) -> std::string
        {
            auto typeArgs = getFirstAttributeArgs(attributes, Attribute::Type);
            if (typeArgs.empty())
                return fallbackType;

            switch (typeArgs[0].type)
            {
            case TokenType::kwI8: return "int8_t";
            case TokenType::kwU8: return "uint8_t";
            case TokenType::kwI16: return "int16_t";
            case TokenType::kwU16: return "uint16_t";
            case TokenType::kwI32: return "int32_t";
            case TokenType::kwU32: return "uint32_t";
            case TokenType::kwI64: return "int64_t";
            case TokenType::kwU64: return "uint64_t";
            default: return fallbackType;
            }
        };

        auto getEnumUnderlyingWioTypeName = [&](const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                std::string_view fallbackTypeName) -> std::string
        {
            auto typeArgs = getFirstAttributeArgs(attributes, Attribute::Type);
            if (typeArgs.empty())
                return std::string(fallbackTypeName);

            switch (typeArgs[0].type)
            {
            case TokenType::kwI8: return "i8";
            case TokenType::kwU8: return "u8";
            case TokenType::kwI16: return "i16";
            case TokenType::kwU16: return "u16";
            case TokenType::kwI32: return "i32";
            case TokenType::kwU32: return "u32";
            case TokenType::kwI64: return "i64";
            case TokenType::kwU64: return "u64";
            default: return std::string(fallbackTypeName);
            }
        };

        auto emitStringViewArray = [&](std::string_view name, const std::vector<std::string>& values)
        {
            emit("static constexpr std::array<std::string_view, " + std::to_string(values.size()) + "> " +
                 std::string(name) + " = {");
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    emit(", ");
                emit("\"" + common::wioStringToEscapedCppString(values[i]) + "\"");
            }
            emitLine("};");
        };

        auto emitSimpleTypeReflectionSpecialization = [&](const std::string& cppTypeName,
                                                           const std::string& wioTypeName,
                                                           std::string_view kind)
        {
            emitLine("template <>");
            emitLine("struct wio::runtime::TypeReflection<" + cppTypeName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::string_view Name = \"" +
                     common::wioStringToEscapedCppString(wioTypeName) + "\";");
            emitLine("static constexpr wio::runtime::ReflectedTypeKind Kind = wio::runtime::ReflectedTypeKind::" +
                     std::string(kind) + ";");
            emitStringViewArray("FieldNames", {});
            emitStringViewArray("FieldTypes", {});
            emitStringViewArray("FieldAccess", {});
            emitStringViewArray("MethodNames", {});
            emitStringViewArray("MethodSignatures", {});
            emitStringViewArray("MethodAccess", {});
            emitStringViewArray("BaseTypes", {});
            dedent();
            emitLine("};");
        };

        auto emitEnumReflectionSpecialization = [&](const EnumDeclaration& declaration)
        {
            auto sym = declaration.name->referencedSymbol.Lock();
            std::string enumName = Mangler::mangleStruct(declaration.name->token.value, sym ? sym->scopePath : "");
            const std::string underlyingTypeName = getEnumUnderlyingWioTypeName(declaration.attributes, "i32");

            emitLine("template <>");
            emitLine("struct wio::runtime::EnumReflection<" + enumName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::size_t Count = " + std::to_string(declaration.members.size()) + "u;");
            emitLine("static constexpr std::size_t Size = sizeof(" + enumName + ");");
            emitLine("static constexpr std::string_view UnderlyingTypeName = \"" + underlyingTypeName + "\";");
            emitLine("static std::string Name(const " + enumName + " value)");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (const auto& member : declaration.members)
            {
                emitLine("case " + enumName + "::" + member.name->token.value + ": return \"" +
                         common::wioStringToEscapedCppString(member.name->token.value) + "\";");
            }
            emitLine("default: return \"<unknown>\";");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine("static " + enumName + " Value(const std::size_t index)");
            emitLine("{");
            indent();
            emitLine("switch (index)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + std::to_string(i) + "u: return " + enumName + "::" + declaration.members[i].name->token.value + ";");
            }
            emitLine("default: return " + enumName + "::" + declaration.members.front().name->token.value + ";");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine("static std::ptrdiff_t Index(const " + enumName + " value) noexcept");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + enumName + "::" + declaration.members[i].name->token.value + ": return " + std::to_string(i) + ";");
            }
            emitLine("default: return -1;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            dedent();
            emitLine("};");
            emitSimpleTypeReflectionSpecialization(
                enumName,
                sym && !sym->scopePath.empty()
                    ? sym->scopePath + "::" + declaration.name->token.value
                    : declaration.name->token.value,
                "enum_type");
        };

        auto emitFlagsetReflectionSpecialization = [&](const FlagsetDeclaration& declaration)
        {
            auto sym = declaration.name->referencedSymbol.Lock();
            std::string flagsetName = Mangler::mangleStruct(declaration.name->token.value, sym ? sym->scopePath : "");
            const std::string underlyingTypeName = getEnumUnderlyingWioTypeName(declaration.attributes, "u32");

            emitLine("template <>");
            emitLine("struct wio::runtime::EnumReflection<" + flagsetName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::size_t Count = " + std::to_string(declaration.members.size()) + "u;");
            emitLine("static constexpr std::size_t Size = sizeof(" + flagsetName + ");");
            emitLine("static constexpr std::string_view UnderlyingTypeName = \"" + underlyingTypeName + "\";");
            emitLine("static std::string Name(const " + flagsetName + " value)");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (const auto& member : declaration.members)
            {
                emitLine("case " + flagsetName + "::" + member.name->token.value + ": return \"" +
                         common::wioStringToEscapedCppString(member.name->token.value) + "\";");
            }
            emitLine("default: break;");
            dedent();
            emitLine("}");
            emitLine("using Under = std::underlying_type_t<" + flagsetName + ">;");
            emitLine("const Under raw = static_cast<Under>(value);");
            emitLine("std::string result;");
            emitLine("bool first = true;");
            emitLine("Under remaining = raw;");
            for (const auto& member : declaration.members)
            {
                const std::string memberName = member.name->token.value;
                emitLine("{");
                indent();
                emitLine("const Under memberValue = static_cast<Under>(" + flagsetName + "::" + memberName + ");");
                emitLine("if (memberValue != 0 && (raw & memberValue) == memberValue)");
                emitLine("{");
                indent();
                emitLine("if (!first) result += \"|\";");
                emitLine("result += \"" + common::wioStringToEscapedCppString(memberName) + "\";");
                emitLine("first = false;");
                emitLine("remaining = static_cast<Under>(remaining & static_cast<Under>(~memberValue));");
                dedent();
                emitLine("}");
                dedent();
                emitLine("}");
            }
            emitLine("if (remaining != 0)");
            emitLine("{");
            indent();
            emitLine("if (!result.empty()) result += \"|\";");
            emitLine("result += \"<unknown>\";");
            dedent();
            emitLine("}");
            emitLine("if (result.empty()) return raw == 0 ? \"0\" : \"<unknown>\";");
            emitLine("return result;");
            dedent();
            emitLine("}");
            emitLine("static " + flagsetName + " Value(const std::size_t index)");
            emitLine("{");
            indent();
            emitLine("switch (index)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + std::to_string(i) + "u: return " + flagsetName + "::" + declaration.members[i].name->token.value + ";");
            }
            emitLine("default: return " + flagsetName + "::" + declaration.members.front().name->token.value + ";");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine("static std::ptrdiff_t Index(const " + flagsetName + " value) noexcept");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + flagsetName + "::" + declaration.members[i].name->token.value + ": return " + std::to_string(i) + ";");
            }
            emitLine("default: return -1;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            dedent();
            emitLine("};");
            emitSimpleTypeReflectionSpecialization(
                flagsetName,
                sym && !sym->scopePath.empty()
                    ? sym->scopePath + "::" + declaration.name->token.value
                    : declaration.name->token.value,
                "flagset_type");
        };

        auto accessName = [](const AccessModifier access, const bool objectDefault) -> std::string
        {
            if (access == AccessModifier::Public)
                return "public";
            if (access == AccessModifier::Protected)
                return "protected";
            if (access == AccessModifier::Private)
                return "private";
            return objectDefault ? "private" : "public";
        };

        auto emitStructuredTypeReflectionSpecialization = [&](const auto& declaration,
                                                               const std::vector<ComponentMember>* componentMembers,
                                                               const std::vector<ObjectMember>* objectMembers,
                                                               const std::vector<NodePtr<FunctionDeclaration>>* interfaceMethods,
                                                               std::string_view reflectedKind)
        {
            auto symbol = declaration.name->referencedSymbol.Lock();
            auto structType = getStructTypeFromSymbol(symbol);
            if (!structType || usesNativePodAliasModelForCodegen(structType))
                return;

            std::vector<std::string> fieldNames;
            std::vector<std::string> fieldTypes;
            std::vector<std::string> fieldAccess;
            std::vector<std::string> methodNames;
            std::vector<std::string> methodSignatures;
            std::vector<std::string> methodAccess;
            std::vector<std::string> methodBehaviorAttributeNames;
            std::vector<std::string> methodBehaviorProcessorTypes;
            std::vector<std::string> methodBehaviorPhases;
            std::vector<std::string> methodBehaviorHooks;
            std::vector<std::string> methodBehaviorModes;
            std::vector<size_t> methodBehaviorOffsets{0};
            std::vector<std::string> baseTypes;
            std::vector<std::string> typeAttributes;
            std::vector<std::string> typeAttributeNames;
            std::vector<std::string> typeAttributeRetentions;
            std::vector<std::string> typeAttributeOrigins;
            std::vector<std::uint64_t> typeAttributeStableIds;
            std::vector<std::string> typeAttributeArgumentNames;
            std::vector<std::string> typeAttributeArgumentTypes;
            std::vector<std::string> typeAttributeArgumentValues;
            std::vector<std::uint8_t> typeAttributeArgumentUsedDefaults;
            std::vector<size_t> typeAttributeArgumentOffsets{0};
            std::vector<std::string> fieldAttributeNames;
            std::vector<size_t> fieldAttributeOffsets{0};

            auto reflectedAttributeName = [](const NodePtr<AttributeStatement>& attribute)
            {
                if (!attribute || !attribute->runtimeRetained || attribute->qualifiedName.empty())
                    return std::string{};
                std::string result = attribute->qualifiedName;
                if (!attribute->args.empty())
                {
                    result += "(";
                    for (size_t index = 0; index < attribute->args.size(); ++index)
                    {
                        if (index > 0) result += ",";
                        result += attribute->args[index].value;
                    }
                    result += ")";
                }
                return result;
            };

            for (const auto& attribute : declaration.attributes)
            {
                auto name = reflectedAttributeName(attribute);
                if (name.empty())
                    continue;
                typeAttributes.push_back(std::move(name));
                typeAttributeNames.push_back(attribute->qualifiedName);
                typeAttributeRetentions.push_back("runtime");
                switch (attribute->origin)
                {
                case AttributeOrigin::Direct: typeAttributeOrigins.push_back("direct"); break;
                case AttributeOrigin::Inherited: typeAttributeOrigins.push_back("inherited"); break;
                case AttributeOrigin::Scoped: typeAttributeOrigins.push_back("scoped"); break;
                case AttributeOrigin::Composed: typeAttributeOrigins.push_back("composed"); break;
                case AttributeOrigin::Generated: typeAttributeOrigins.push_back("generated"); break;
                case AttributeOrigin::Compiler: typeAttributeOrigins.push_back("compiler"); break;
                }

                std::uint64_t stableId = 14695981039346656037ull;
                const std::string& stableAttributeName = attribute->canonicalName.empty()
                    ? attribute->qualifiedName
                    : attribute->canonicalName;
                for (const unsigned char byte : stableAttributeName)
                {
                    stableId ^= byte;
                    stableId *= 1099511628211ull;
                }
                typeAttributeStableIds.push_back(stableId);

                for (size_t argumentIndex = 0; argumentIndex < attribute->args.size(); ++argumentIndex)
                {
                    const Token& argument = attribute->args[argumentIndex];
                    typeAttributeArgumentNames.push_back(
                        argumentIndex < attribute->argumentNames.size()
                            ? attribute->argumentNames[argumentIndex]
                            : std::string{});
                    std::string argumentType = "unknown";
                    if (argument.type == TokenType::stringLiteral)
                        argumentType = argument.isUnicodeString ? "text" : "string";
                    else if (argument.type == TokenType::integerLiteral)
                        argumentType = "integer";
                    else if (argument.type == TokenType::floatLiteral)
                        argumentType = "float";
                    else if (argument.type == TokenType::byteLiteral)
                        argumentType = "byte";
                    else if (argument.type == TokenType::kwTrue || argument.type == TokenType::kwFalse)
                        argumentType = "bool";
                    else if (argument.type == TokenType::identifier)
                        argumentType = "symbol";
                    typeAttributeArgumentTypes.push_back(std::move(argumentType));
                    typeAttributeArgumentValues.push_back(argument.value);
                    typeAttributeArgumentUsedDefaults.push_back(
                        argumentIndex < attribute->argumentUsedDefaults.size() &&
                        attribute->argumentUsedDefaults[argumentIndex]
                            ? static_cast<std::uint8_t>(1)
                            : static_cast<std::uint8_t>(0));
                }
                typeAttributeArgumentOffsets.push_back(typeAttributeArgumentValues.size());
            }

            auto addMember = [&](const auto& member, const bool objectDefault)
            {
                if (member.declaration->template is<VariableDeclaration>())
                {
                    auto variable = member.declaration->template as<VariableDeclaration>();
                    if (variable->mutability == Mutability::Const)
                        return;
                    fieldNames.push_back(variable->name->token.value);
                    Ref<sema::Type> type = variable->type ? variable->type->refType.Lock() : variable->name->refType.Lock();
                    fieldTypes.push_back(type ? type->toString() : "<unknown>");
                    fieldAccess.push_back(accessName(member.access, objectDefault));
                    for (const auto& attribute : variable->attributes)
                    {
                        auto name = reflectedAttributeName(attribute);
                        if (!name.empty()) fieldAttributeNames.push_back(std::move(name));
                    }
                    fieldAttributeOffsets.push_back(fieldAttributeNames.size());
                    return;
                }

                if (member.declaration->template is<FunctionDeclaration>())
                {
                    auto function = member.declaration->template as<FunctionDeclaration>();
                    methodNames.push_back(function->name->token.value);
                    std::string signature = function->name->token.value + "(";
                    for (size_t i = 0; i < function->parameters.size(); ++i)
                    {
                        Ref<sema::Type> parameterType = function->parameters[i].name->refType.Lock();
                        signature += parameterType ? parameterType->toString() : "<unknown>";
                        if (i + 1 < function->parameters.size())
                            signature += ", ";
                    }
                    Ref<sema::Type> returnType = function->returnType ? function->returnType->refType.Lock() : nullptr;
                    signature += ") -> " + std::string(returnType ? returnType->toString() : "void");
                    methodSignatures.push_back(std::move(signature));
                    methodAccess.push_back(accessName(member.access, objectDefault));

                    std::vector<const AttributeStatement*> orderedAttributes;
                    orderedAttributes.reserve(function->attributes.size());
                    for (const auto& attribute : function->attributes)
                    {
                        if (attribute)
                            orderedAttributes.push_back(attribute.Get());
                    }
                    std::ranges::stable_sort(
                        orderedAttributes,
                        {},
                        &AttributeStatement::processorOrder);
                    for (const auto* attribute : orderedAttributes)
                    {
                        for (const auto& processor : attribute->processorBindings)
                        {
                            if ((processor.phase != "pre" && processor.phase != "post" &&
                                 processor.phase != "finally" && processor.phase != "around") ||
                                processor.cppTypeName.empty() || processor.hookCppName.empty())
                            {
                                continue;
                            }
                            methodBehaviorAttributeNames.push_back(
                                attribute->canonicalName.empty()
                                    ? attribute->qualifiedName
                                    : attribute->canonicalName);
                            methodBehaviorProcessorTypes.push_back(processor.canonicalTypeName);
                            methodBehaviorPhases.push_back(processor.phase);
                            methodBehaviorHooks.push_back(
                                processor.phase == "pre" ? "Before" :
                                processor.phase == "post" ? "After" :
                                processor.phase == "finally" ? "Finally" : "Around");
                            methodBehaviorModes.push_back(processor.hookMode);
                        }
                    }
                    methodBehaviorOffsets.push_back(methodBehaviorPhases.size());
                }
            };

            if (componentMembers)
            {
                for (const auto& member : *componentMembers)
                    addMember(member, false);
            }
            if (objectMembers)
            {
                for (const auto& member : *objectMembers)
                    addMember(member, true);
            }
            if (interfaceMethods)
            {
                for (const auto& function : *interfaceMethods)
                {
                    methodNames.push_back(function->name->token.value);
                    std::string signature = function->name->token.value + "(";
                    for (size_t i = 0; i < function->parameters.size(); ++i)
                    {
                        Ref<sema::Type> parameterType = function->parameters[i].name->refType.Lock();
                        signature += parameterType ? parameterType->toString() : "<unknown>";
                        if (i + 1 < function->parameters.size())
                            signature += ", ";
                    }
                    Ref<sema::Type> returnType = function->returnType ? function->returnType->refType.Lock() : nullptr;
                    signature += ") -> " + std::string(returnType ? returnType->toString() : "void");
                    methodSignatures.push_back(std::move(signature));
                    methodAccess.push_back("public");
                    methodBehaviorOffsets.push_back(methodBehaviorPhases.size());
                }
            }

            for (const auto& baseType : structType->baseTypes)
            {
                Ref<sema::Type> resolvedBase = unwrapAliasType(baseType);
                if (!resolvedBase)
                    continue;
                if (resolvedBase->kind() == sema::TypeKind::Struct)
                {
                    auto baseStruct = resolvedBase.AsFast<sema::StructType>();
                    if (baseStruct && baseStruct->name == "object" && baseStruct->scopePath.empty())
                        continue;
                }
                baseTypes.push_back(resolvedBase->toString());
            }

            std::string cppTypeName = reflectedKind == "interface_type"
                ? Mangler::mangleInterface(structType->name, structType->scopePath)
                : mangleStructTypeName(structType);
            std::vector<std::string> reflectionParameterNames;
            reflectionParameterNames.reserve(declaration.genericParameters.size());
            for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                reflectionParameterNames.push_back("_WIO_REFLECT_ARG_" + std::to_string(i));
            if (structType->isExplicitSpecialization)
            {
                cppTypeName = mangleStructTypeName(structType);
            }
            else if (!declaration.genericParameters.empty())
            {
                cppTypeName =
                    (reflectedKind == "interface_type"
                        ? Mangler::mangleInterface(structType->name, structType->scopePath)
                        : Mangler::mangleStruct(structType->name, structType->scopePath)) +
                    "<";
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    if (i > 0)
                        cppTypeName += ", ";
                    cppTypeName += reflectionParameterNames[i];
                    if (declaration.hasGenericParameterPack && i + 1 == declaration.genericParameters.size())
                        cppTypeName += "...";
                }
                cppTypeName += ">";
            }
            if (structType->isExplicitSpecialization && !declaration.genericParameters.empty())
            {
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    replaceCppIdentifier(
                        cppTypeName,
                        declaration.genericParameters[i]->token.value,
                        reflectionParameterNames[i]);
                }
            }

            std::vector<std::string> genericMetadataParameterNames;
            std::vector<std::string> genericMetadataArgumentExpressions;
            Ref<sema::StructType> genericMetadataPrimary = structType->genericPrimaryType.Lock();
            if (structType->isExplicitSpecialization && genericMetadataPrimary)
                genericMetadataParameterNames = genericMetadataPrimary->genericParameterNames;
            else
                genericMetadataParameterNames = structType->genericParameterNames;

            auto reflectionParameterNameFor = [&](std::string_view sourceName) -> std::string
            {
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    if (declaration.genericParameters[i]->token.value == sourceName)
                        return reflectionParameterNames[i];
                }
                return sanitizeCppIdentifier(sourceName);
            };
            std::function<std::string(const Ref<sema::Type>&)> genericArgumentExpression;
            genericArgumentExpression = [&](const Ref<sema::Type>& argument) -> std::string
            {
                if (!argument)
                    return "std::string(\"<unknown>\")";

                Ref<sema::Type> resolvedArgument = argument;
                while (resolvedArgument && resolvedArgument->kind() == sema::TypeKind::Alias)
                    resolvedArgument = resolvedArgument.AsFast<sema::AliasType>()->aliasedType;
                if (!resolvedArgument)
                    return "std::string(\"<unknown>\")";

                if (resolvedArgument->kind() == sema::TypeKind::GenericParameter)
                {
                    const auto parameter = resolvedArgument.AsFast<sema::GenericParameterType>();
                    return "wio::runtime::ReflectedTypeName<" +
                           reflectionParameterNameFor(parameter->name) + ">()";
                }
                if (resolvedArgument->kind() == sema::TypeKind::ConstGenericParameter)
                {
                    const auto parameter = resolvedArgument.AsFast<sema::ConstGenericParameterType>();
                    const std::string backendName = reflectionParameterNameFor(parameter->name);
                    Ref<sema::Type> valueType = unwrapAliasTypeForCodegen(parameter->valueType);
                    if (valueType && valueType->kind() == sema::TypeKind::Primitive)
                    {
                        const std::string& valueTypeName = valueType.AsFast<sema::PrimitiveType>()->name;
                        if (valueTypeName == "string")
                            return backendName + ".RuntimeValue()";
                        if (valueTypeName == "text")
                            return backendName + ".RuntimeValue().Utf8()";
                    }
                    return "std::to_string(" + backendName + ")";
                }
                if (resolvedArgument->kind() == sema::TypeKind::ConstValue)
                {
                    const auto value = resolvedArgument.AsFast<sema::ConstValueType>();
                    return "std::string(\"" +
                           common::wioStringToEscapedCppString(value->value) + "\")";
                }
                if (resolvedArgument->kind() == sema::TypeKind::Struct)
                {
                    const auto nested = resolvedArgument.AsFast<sema::StructType>();
                    const std::string nestedName = nested->scopePath.empty()
                        ? nested->name
                        : nested->scopePath + "::" + nested->name;
                    if (nested->genericArguments.empty())
                        return "std::string(\"" +
                               common::wioStringToEscapedCppString(nestedName) + "\")";

                    std::string expression = "std::string(\"" +
                        common::wioStringToEscapedCppString(nestedName + "<") + "\")";
                    for (size_t i = 0; i < nested->genericArguments.size(); ++i)
                    {
                        if (i > 0)
                            expression += " + std::string(\", \")";
                        expression += " + " + genericArgumentExpression(nested->genericArguments[i]);
                    }
                    return expression + " + std::string(\">\")";
                }

                return "std::string(\"" +
                       common::wioStringToEscapedCppString(resolvedArgument->toString()) + "\")";
            };

            if (structType->isExplicitSpecialization)
            {
                for (const auto& argument : structType->genericArguments)
                    genericMetadataArgumentExpressions.push_back(genericArgumentExpression(argument));
            }
            else
            {
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    const auto& parameter = declaration.genericParameters[i];
                    const std::string& backendName = reflectionParameterNames[i];
                    if (parameter->isConstGenericParameter)
                    {
                        Ref<sema::Type> valueType = parameter->genericValueType
                            ? unwrapAliasTypeForCodegen(parameter->genericValueType->refType.Lock())
                            : nullptr;
                        if (valueType && valueType->kind() == sema::TypeKind::Primitive)
                        {
                            const std::string& valueTypeName = valueType.AsFast<sema::PrimitiveType>()->name;
                            if (valueTypeName == "string")
                                genericMetadataArgumentExpressions.push_back(backendName + ".RuntimeValue()");
                            else if (valueTypeName == "text")
                                genericMetadataArgumentExpressions.push_back(backendName + ".RuntimeValue().Utf8()");
                            else
                                genericMetadataArgumentExpressions.push_back("std::to_string(" + backendName + ")");
                        }
                        else
                        {
                            genericMetadataArgumentExpressions.push_back("std::to_string(" + backendName + ")");
                        }
                    }
                    else
                    {
                        genericMetadataArgumentExpressions.push_back(
                            "wio::runtime::ReflectedTypeName<" + backendName + ">()" +
                            (declaration.hasGenericParameterPack && i + 1 == declaration.genericParameters.size()
                                ? "..."
                                : ""));
                    }
                }
            }
            const std::string wioTypeName = structType->scopePath.empty()
                ? structType->name
                : structType->scopePath + "::" + structType->name;
            if (declaration.genericParameters.empty())
            {
                emitLine("template <>");
            }
            else
            {
                emit("template <");
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    if (i > 0)
                        emit(", ");
                    const bool isPack = declaration.hasGenericParameterPack &&
                                        i + 1 == declaration.genericParameters.size();
                    emit(formatCppTemplateParameter(
                        declaration.genericParameters[i],
                        isPack,
                        reflectionParameterNames[i]));
                }
                emitLine(">");
            }
            emitLine("struct wio::runtime::TypeReflection<" + cppTypeName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::string_view Name = \"" +
                     common::wioStringToEscapedCppString(wioTypeName) + "\";");
            emitLine("static constexpr wio::runtime::ReflectedTypeKind Kind = wio::runtime::ReflectedTypeKind::" +
                     std::string(reflectedKind) + ";");
            emitStringViewArray("FieldNames", fieldNames);
            emitStringViewArray("FieldTypes", fieldTypes);
            emitStringViewArray("FieldAccess", fieldAccess);
            emitStringViewArray("MethodNames", methodNames);
            emitStringViewArray("MethodSignatures", methodSignatures);
            emitStringViewArray("MethodAccess", methodAccess);
            emitStringViewArray("MethodBehaviorAttributeNames", methodBehaviorAttributeNames);
            emitStringViewArray("MethodBehaviorProcessorTypes", methodBehaviorProcessorTypes);
            emitStringViewArray("MethodBehaviorPhases", methodBehaviorPhases);
            emitStringViewArray("MethodBehaviorHooks", methodBehaviorHooks);
            emitStringViewArray("MethodBehaviorModes", methodBehaviorModes);
            emit("static constexpr std::array<std::size_t, " +
                 std::to_string(methodBehaviorOffsets.size()) + "> MethodBehaviorOffsets{ ");
            for (size_t index = 0; index < methodBehaviorOffsets.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(methodBehaviorOffsets[index]));
            }
            emitLine(" };");
            emitStringViewArray("BaseTypes", baseTypes);
            emitStringViewArray("TypeAttributes", typeAttributes);
            emitStringViewArray("TypeAttributeNames", typeAttributeNames);
            emitStringViewArray("TypeAttributeRetentions", typeAttributeRetentions);
            emitStringViewArray("TypeAttributeOrigins", typeAttributeOrigins);
            emitStringViewArray("TypeAttributeArgumentNames", typeAttributeArgumentNames);
            emitStringViewArray("TypeAttributeArgumentTypes", typeAttributeArgumentTypes);
            emitStringViewArray("TypeAttributeArgumentValues", typeAttributeArgumentValues);
            emit("static constexpr std::array<std::uint64_t, " +
                 std::to_string(typeAttributeStableIds.size()) + "> TypeAttributeStableIds{ ");
            for (size_t index = 0; index < typeAttributeStableIds.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(typeAttributeStableIds[index]) + "ull");
            }
            emitLine(" };");
            emit("static constexpr std::array<std::uint8_t, " +
                 std::to_string(typeAttributeArgumentUsedDefaults.size()) + "> TypeAttributeArgumentUsedDefaults{ ");
            for (size_t index = 0; index < typeAttributeArgumentUsedDefaults.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(typeAttributeArgumentUsedDefaults[index]));
            }
            emitLine(" };");
            emit("static constexpr std::array<std::size_t, " +
                 std::to_string(typeAttributeArgumentOffsets.size()) + "> TypeAttributeArgumentOffsets{ ");
            for (size_t index = 0; index < typeAttributeArgumentOffsets.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(typeAttributeArgumentOffsets[index]));
            }
            emitLine(" };");
            emitStringViewArray("FieldAttributeNames", fieldAttributeNames);
            emit("static constexpr std::array<std::size_t, " +
                 std::to_string(fieldAttributeOffsets.size()) + "> FieldAttributeOffsets{ ");
            for (size_t index = 0; index < fieldAttributeOffsets.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(fieldAttributeOffsets[index]));
            }
            emitLine(" };");
            if (!genericMetadataParameterNames.empty())
            {
                emitLine("static std::vector<std::string> _WIOGenericParameterNames()");
                emitLine("{");
                indent();
                emit("return { ");
                for (size_t i = 0; i < genericMetadataParameterNames.size(); ++i)
                {
                    if (i > 0) emit(", ");
                    const std::string suffix = structType->hasGenericParameterPack &&
                                               i + 1 == genericMetadataParameterNames.size()
                        ? "..."
                        : "";
                    emit("\"" + common::wioStringToEscapedCppString(
                        genericMetadataParameterNames[i] + suffix) + "\"");
                }
                emitLine(" };");
                dedent();
                emitLine("}");
                emitLine("static std::vector<std::string> _WIOGenericArguments()");
                emitLine("{");
                indent();
                emit("return { ");
                for (size_t i = 0; i < genericMetadataArgumentExpressions.size(); ++i)
                {
                    if (i > 0) emit(", ");
                    emit(genericMetadataArgumentExpressions[i]);
                }
                emitLine(" };");
                dedent();
                emitLine("}");
            }
            dedent();
            emitLine("};");
        };

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<UseStatement>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FlagDeclaration>())
                stmt->accept(*this);

            if (stmt->template is<FlagsetDeclaration>())
            {
                auto declaration = stmt->template as<FlagsetDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto enumType = getStructTypeFromSymbol(sym);
                if (enumType && usesNativePodAliasModelForCodegen(enumType))
                {
                    std::string nativeTypeName = enumType->nativeCppName.empty()
                        ? enumType->name
                        : enumType->nativeCppName;

                    emitLine(common::formatString(
                        "using {} = {};",
                        Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : ""),
                        nativeTypeName
                    ));
                }
                else
                {
                    stmt->accept(*this);
                }

                emitFlagsetReflectionSpecialization(*declaration);
            }
            else if (stmt->template is<EnumDeclaration>())
            {
                auto declaration = stmt->template as<EnumDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto enumType = getStructTypeFromSymbol(sym);
                if (enumType && usesNativePodAliasModelForCodegen(enumType))
                {
                    std::string nativeTypeName = enumType->nativeCppName.empty()
                        ? enumType->name
                        : enumType->nativeCppName;

                    emitLine(common::formatString(
                        "using {} = {};",
                        Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : ""),
                        nativeTypeName
                    ));
                }
                else
                {
                    stmt->accept(*this);
                }

                emitEnumReflectionSpecialization(*declaration);
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<VariableDeclaration>())
            {
                auto variableDecl = stmt->template as<VariableDeclaration>();
                if (variableDecl->mutability == Mutability::Const)
                    stmt->accept(*this);
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<ComponentDeclaration>())
            {
                auto declaration = stmt->template as<ComponentDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto componentType = getStructTypeFromSymbol(sym);
                if (componentType && componentType->isExplicitSpecialization &&
                    usesNativePodAliasModelForCodegen(componentType))
                {
                    // The primary alias already maps every concrete argument
                    // to the native C++ template. Wio specializations refine
                    // semantic layout only and must not emit an illegal alias
                    // template specialization.
                }
                else if (componentType && componentType->isExplicitSpecialization)
                {
                    if (componentType->isPartialSpecialization)
                        emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    else
                        emitLine("template <>");
                    emitLine(common::formatString("struct {};", mangleStructTypeName(componentType)));
                }
                else if (componentType && usesNativePodAliasModelForCodegen(componentType))
                {
                    emitTemplateForwardDeclarationPrefix(declaration->genericParameters);

                    std::string nativeTypeName = componentType->nativeCppName.empty()
                        ? componentType->name
                        : componentType->nativeCppName;
                    if (!declaration->genericParameters.empty())
                    {
                        nativeTypeName += "<";
                        for (size_t i = 0; i < declaration->genericParameters.size(); ++i)
                        {
                            if (i > 0)
                                nativeTypeName += ", ";

                            nativeTypeName += declaration->genericParameters[i]->token.value;
                            if (declaration->hasGenericParameterPack && i + 1 == declaration->genericParameters.size())
                                nativeTypeName += "...";
                        }
                        nativeTypeName += ">";
                    }

                    emitLine(common::formatString(
                        "using {} = {};",
                        Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : ""),
                        nativeTypeName
                    ));
                }
                else
                {
                    emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    emitLine(common::formatString("struct {};", Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : "")));
                }
            }
            else if (stmt->template is<ObjectDeclaration>())
            {
                auto declaration = stmt->template as<ObjectDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto objectType = getStructTypeFromSymbol(sym);
                if (objectType && objectType->isExplicitSpecialization)
                {
                    if (objectType->isPartialSpecialization)
                        emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    else
                        emitLine("template <>");
                    emitLine(common::formatString("struct {};", mangleStructTypeName(objectType)));
                }
                else
                {
                    emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    emitLine(common::formatString("struct {};", Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : "")));
                }
            }
            else if (stmt->template is<InterfaceDeclaration>())
            {
                auto declaration = stmt->template as<InterfaceDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                emitLine(common::formatString("struct {};", Mangler::mangleInterface(declaration->name->token.value, sym ? sym->scopePath : "")));
            }
        });

        // Member functions are emitted inline with their owning type. Declare
        // globals before those type definitions so lifecycle hooks and ordinary
        // methods can safely reference globals declared later in the module.
        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (!stmt->template is<VariableDeclaration>())
                return;

            auto variable = stmt->template as<VariableDeclaration>();
            auto symbol = variable->name->referencedSymbol.Lock();
            Ref<sema::Type> type = symbol && symbol->type
                ? symbol->type
                : variable->name->refType.Lock();
            if (!type)
                return;

            const std::string declarationType =
                variable->mutability != Mutability::Mutable
                    ? "const " + toCppType(type)
                    : toCppType(type);
            emitLine(common::formatString(
                "extern {} {};",
                declarationType,
                Mangler::mangleGlobalVar(variable->name->token.value, symbol ? symbol->scopePath : "")
            ));
        });

        isEmittingPrototypes_ = true;
        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FunctionDeclaration>())
                stmt->accept(*this);
            else if (stmt->template is<ExtensionDeclaration>())
                stmt->accept(*this);
        });
        isEmittingPrototypes_ = false;

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<InterfaceDeclaration>() || stmt->template is<ComponentDeclaration>() || stmt->template is<ObjectDeclaration>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<ComponentDeclaration>())
            {
                auto declaration = stmt->template as<ComponentDeclaration>();
                emitStructuredTypeReflectionSpecialization(
                    *declaration,
                    &declaration->members,
                    nullptr,
                    nullptr,
                    "component_type");
            }
            else if (stmt->template is<ObjectDeclaration>())
            {
                auto declaration = stmt->template as<ObjectDeclaration>();
                emitStructuredTypeReflectionSpecialization(
                    *declaration,
                    nullptr,
                    &declaration->members,
                    nullptr,
                    "object_type");
            }
            else if (stmt->template is<InterfaceDeclaration>())
            {
                auto declaration = stmt->template as<InterfaceDeclaration>();
                emitStructuredTypeReflectionSpecialization(
                    *declaration,
                    nullptr,
                    nullptr,
                    &declaration->methods,
                    "interface_type");
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<VariableDeclaration>())
            {
                auto variableDecl = stmt->template as<VariableDeclaration>();
                if (variableDecl->mutability != Mutability::Const)
                    stmt->accept(*this);
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FunctionDeclaration>())
                stmt->accept(*this);
            else if (stmt->template is<ExtensionDeclaration>())
                stmt->accept(*this);
        });
    }
