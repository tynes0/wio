// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        Ref<sema::Type> unwrapAliasTypeForCodegen(Ref<sema::Type> type)
        {
            return unwrapAliasType(std::move(type));
        }

        using sema::ConstVariableDeclarationMap;

        void collectVariableDeclarationsBySymbol(const Ref<Program>& program, ConstVariableDeclarationMap& variableDeclarationsBySymbol)
        {
            if (!program)
                return;

            auto collectFromStatementList = [&](const std::vector<NodePtr<Statement>>& statements, auto&& collectFromStatementListRef) -> void
            {
                for (const auto& statement : statements)
                {
                    if (!statement)
                        continue;

                    if (auto* variableDeclaration = statement->as<VariableDeclaration>())
                    {
                        if (variableDeclaration->name)
                        {
                            Ref<sema::Symbol> symbol = variableDeclaration->name->referencedSymbol.Lock();
                            if (symbol)
                                variableDeclarationsBySymbol[symbol.Get()] = variableDeclaration;
                        }
                        continue;
                    }

                    if (auto* realmDeclaration = statement->as<RealmDeclaration>())
                    {
                        collectFromStatementListRef(realmDeclaration->statements, collectFromStatementListRef);
                    }
                }
            };

            collectFromStatementList(program->statements, collectFromStatementList);
        }

        std::string getBackendTypeEquivalenceKey(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolved = unwrapAliasTypeForCodegen(type);
            if (!resolved)
                return "<null>";

            if (resolved->kind() != sema::TypeKind::Primitive)
                return resolved->toCppString();

            const std::string& name = resolved.AsFast<sema::PrimitiveType>()->name;
            if (name == "i8") return "int8_t";
            if (name == "i16") return "int16_t";
            if (name == "i32") return "int32_t";
            if (name == "i64") return "int64_t";
            if (name == "u8") return "uint8_t";
            if (name == "u16") return "uint16_t";
            if (name == "u32") return "uint32_t";
            if (name == "u64") return "uint64_t";
            if (name == "isize") return sizeof(std::intptr_t) == sizeof(std::int64_t) ? "int64_t" : "int32_t";
            if (name == "usize") return sizeof(std::size_t) == sizeof(std::uint64_t) ? "uint64_t" : "uint32_t";
            if (name == "f32") return "float";
            if (name == "f64") return "double";
            if (name == "char") return "char";
            if (name == "uchar" || name == "byte") return "unsigned char";
            if (name == "bool") return "bool";
            if (name == "string") return "wio::String";
            if (name == "any") return "wio::runtime::Any";
            if (name == "opaque") return "void*";
            return resolved->toCppString();
        }

        bool isAnyTypeForCodegen(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolved = unwrapAliasTypeForCodegen(type);
            return resolved &&
                   resolved->kind() == sema::TypeKind::Primitive &&
                   resolved.AsFast<sema::PrimitiveType>()->name == "any";
        }

        bool isOpaqueTypeForCodegen(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolved = unwrapAliasTypeForCodegen(type);
            return resolved &&
                   resolved->kind() == sema::TypeKind::Primitive &&
                   resolved.AsFast<sema::PrimitiveType>()->name == "opaque";
        }

        bool isStructMemberFunctionSymbol(const Ref<sema::Symbol>& symbol)
        {
            if (!symbol)
                return false;

            if (symbol->kind == sema::SymbolKind::Function)
                return symbol->innerScope && symbol->innerScope->getKind() == sema::ScopeKind::Struct;

            if (symbol->kind != sema::SymbolKind::FunctionGroup)
                return false;

            if (symbol->innerScope && symbol->innerScope->getKind() == sema::ScopeKind::Struct)
                return true;

            for (const auto& overload : symbol->overloads)
            {
                if (isStructMemberFunctionSymbol(overload))
                    return true;
            }

            return false;
        }

        std::string getBackendInstantiationEquivalenceKey(const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string key;
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                if (i > 0)
                    key += "|";
                key += getBackendTypeEquivalenceKey(instantiationTypes[i]);
            }
            return key;
        }

        std::string toCppType(const Ref<sema::Type>& type)
        {
            if (!type) return "void"; // Fallback

            Ref<sema::Type> current = unwrapAliasTypeForCodegen(type);
            if (!current)
                current = type;

            if (current->kind() == sema::TypeKind::Nullable)
                return toCppType(current.AsFast<sema::NullableType>()->valueType);

            auto buildReferenceCppType = [](const Ref<sema::ReferenceType>& refType) -> std::string
            {
                if (!refType || !refType->referredType)
                    return "void*";

                std::string referredCppType = toCppType(refType->referredType);
                Ref<sema::Type> resolvedReferredType = unwrapAliasTypeForCodegen(refType->referredType);

                if (resolvedReferredType && resolvedReferredType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedReferredType.AsFast<sema::StructType>();
                    if (structType->isInterface)
                        return referredCppType;

                    if (structType->isObject)
                    {
                        std::string objectType = Mangler::mangleStruct(structType->name, structType->scopePath);
                        if (!structType->genericArguments.empty())
                        {
                            objectType += "<";
                            for (size_t i = 0; i < structType->genericArguments.size(); ++i)
                            {
                                objectType += toCppType(structType->genericArguments[i]);
                                if (i + 1 < structType->genericArguments.size())
                                    objectType += ", ";
                            }
                            objectType += ">";
                        }

                        return std::string("wio::runtime::") +
                               (refType->isMutable ? "BorrowedObjectRef<" : "BorrowedObjectView<") +
                               objectType + ">";
                    }
                }

                if (refType->isMutable)
                    return referredCppType + "*";

                if (!referredCppType.empty() && referredCppType.back() == '*')
                    return referredCppType + " const*";

                return "const " + referredCppType + "*";
            };

            auto appendGenericArguments = [&](std::string baseName, const Ref<sema::StructType>& structType)
            {
                if (!structType || structType->genericArguments.empty())
                    return baseName;

                baseName += "<";
                for (size_t i = 0; i < structType->genericArguments.size(); ++i)
                {
                    baseName += toCppType(structType->genericArguments[i]);
                    if (i + 1 < structType->genericArguments.size())
                        baseName += ", ";
                }
                baseName += ">";
                return baseName;
            };

            enum class SyntheticPackElementKind : uint8_t
            {
                Absolute,
                FromEnd
            };

            struct SyntheticPackElement
            {
                std::string packName;
                std::size_t value = 0;
                SyntheticPackElementKind kind = SyntheticPackElementKind::Absolute;
            };

            auto tryParseSyntheticPackElementType = [](std::string_view name) -> std::optional<SyntheticPackElement>
            {
                const size_t openBracket = name.find('[');
                const size_t closeBracket = name.find(']');
                if (openBracket == std::string_view::npos || closeBracket == std::string_view::npos || closeBracket <= openBracket + 1)
                    return std::nullopt;

                const std::string packName(name.substr(0, openBracket));
                const std::string indexString(name.substr(openBracket + 1, closeBracket - openBracket - 1));
                if (packName.empty() || indexString.empty())
                    return std::nullopt;

                if (indexString == "last")
                    return SyntheticPackElement{packName, 1, SyntheticPackElementKind::FromEnd};

                if (indexString.starts_with("last-"))
                {
                    const std::string offsetText = indexString.substr(5);
                    if (offsetText.empty())
                        return std::nullopt;

                    try
                    {
                        return SyntheticPackElement{
                            packName,
                            static_cast<std::size_t>(std::stoull(offsetText)) + 1,
                            SyntheticPackElementKind::FromEnd
                        };
                    }
                    catch (...)
                    {
                        return std::nullopt;
                    }
                }

                try
                {
                    return SyntheticPackElement{
                        packName,
                        static_cast<std::size_t>(std::stoull(indexString)),
                        SyntheticPackElementKind::Absolute
                    };
                }
                catch (...)
                {
                    return std::nullopt;
                }
            };

            if (current->kind() == sema::TypeKind::GenericParameter)
            {
                const auto genericParameter = current.AsFast<sema::GenericParameterType>();
                if (auto syntheticPackElement = tryParseSyntheticPackElementType(genericParameter->name))
                {
                    if (syntheticPackElement->kind == SyntheticPackElementKind::FromEnd)
                    {
                        return common::formatString(
                            "typename wio::meta::TypePackView<{}...>::template At<(wio::meta::TypePackView<{}...>::size - {})>",
                            syntheticPackElement->packName,
                            syntheticPackElement->packName,
                            syntheticPackElement->value
                        );
                    }

                    return common::formatString(
                        "typename wio::meta::TypePackView<{}...>::template At<{}>",
                        syntheticPackElement->packName,
                        syntheticPackElement->value
                    );
                }
            }

            if (current->kind() == sema::TypeKind::GenericParameterPack)
                return current.AsFast<sema::GenericParameterPackType>()->name + "...";

            if (current->kind() == sema::TypeKind::Function)
            {
                auto funcType = current.AsFast<sema::FunctionType>();
                std::string result = "std::function<" + toCppType(funcType->returnType) + "(";
                for (size_t i = 0; i < funcType->paramTypes.size(); ++i) {
                    result += toCppType(funcType->paramTypes[i]);
                    if (i < funcType->paramTypes.size() - 1) result += ", ";
                }
                result += ")>";
                return result;
            }

            if (current->kind() == sema::TypeKind::AsyncTask)
            {
                auto taskType = current.AsFast<sema::AsyncTaskType>();
                return "wio::runtime::AsyncTask<" + toCppType(taskType->valueType) + ">";
            }

            if (current->kind() == sema::TypeKind::Reference)
            {
                auto refType = current.AsFast<sema::ReferenceType>();
                if (!refType)
                    return current->toCppString();

                if (refType->referredType && refType->referredType->kind() == sema::TypeKind::Struct)
                {
                    auto sType = refType->referredType.AsFast<sema::StructType>();
                    if (sType->isInterface)
                        return appendGenericArguments(Mangler::mangleInterface(sType->name, sType->scopePath), sType) + "*";
                }

                return buildReferenceCppType(refType);
            }
            else if (current->kind() == sema::TypeKind::Struct)
            {
                auto sType = current.AsFast<sema::StructType>();
                if (sType->isInterface)
                    return appendGenericArguments(Mangler::mangleInterface(sType->name, sType->scopePath), sType) + "*";
            }

            return current->toCppString();
        }

        Ref<sema::Type> instantiateGenericStructType(const Ref<sema::StructType>& structType,
                                                     const std::vector<Ref<sema::Type>>& explicitTypeArguments);
