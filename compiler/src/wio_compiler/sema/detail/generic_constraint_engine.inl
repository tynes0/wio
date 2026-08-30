// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

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
            if (name == "isize") return ctx.getISize();
            if (name == "usize") return ctx.getUSize();
            if (name == "byte") return ctx.getU8();
            if (name == "bit") return ctx.getBool();
            if (name == "f32") return ctx.getF32();
            if (name == "f64") return ctx.getF64();
            if (name == "bool") return ctx.getBool();
            if (name == "char") return ctx.getChar();
            if (name == "string") return ctx.getString();
            if (name == "text") return ctx.getText();
            if (name == "any") return ctx.getAny();
            if (name == "opaque") return ctx.getOpaque();
            if (name == "void") return ctx.getVoid();

            if (name == "object") return ctx.getObject();

            return nullptr;
        }

        bool isTypeDerivedFrom(const Ref<Type>& derived, const Ref<Type>& base)
        {
            if (!derived || !base) return false;
            if (derived->isCompatibleWith(base)) return true;

            if (derived->kind() == sema::TypeKind::Struct)
            {
                auto sType = derived.AsFast<sema::StructType>();
                for (auto& bType : sType->baseTypes)
                {
                    if (isTypeDerivedFrom(bType, base)) return true;
                }
            }
            return false;
        }

        std::string getStructIdentityKey(const Ref<StructType>& structType)
        {
            if (!structType)
                return {};

            if (structType->scopePath.empty())
                return structType->name;

            return structType->scopePath + "::" + structType->name;
        }

        Ref<Symbol> findStructMemberInHierarchy(const Ref<Type>& type, const std::string& name, Ref<Type>* ownerType)
        {
            if (!type || type->kind() != TypeKind::Struct)
                return nullptr;

            auto structType = type.AsFast<StructType>();
            if (!structType)
                return nullptr;

            if (auto lockedScope = structType->structScope.Lock(); lockedScope)
            {
                if (auto found = lockedScope->resolveLocally(name); found)
                {
                    if (ownerType)
                        *ownerType = type;
                    return found;
                }
            }

            for (const auto& baseType : structType->baseTypes)
            {
                if (auto found = findStructMemberInHierarchy(baseType, name, ownerType); found)
                    return found;
            }

            return nullptr;
        }

        bool validateStructMemberAccess(const Ref<Type>& currentStructType,
                                        const Ref<Type>& ownerType,
                                        const Ref<Symbol>& member,
                                        const common::Location& location)
        {
            if (!member)
                return false;

            bool isInsideHierarchy = false;
            bool isInsideSameObject = false;
            bool isTrustedAccess = false;

            if (currentStructType && ownerType)
            {
                if (currentStructType == ownerType ||
                    isTypeDerivedFrom(currentStructType, ownerType) ||
                    isTypeDerivedFrom(ownerType, currentStructType))
                {
                    isInsideHierarchy = true;
                }

                isInsideSameObject = currentStructType == ownerType;

                if (auto ownerStruct = ownerType.AsFast<StructType>(); ownerStruct)
                {
                    const std::string trustedKey = getStructIdentityKey(currentStructType.AsFast<StructType>());
                    isTrustedAccess =
                        !trustedKey.empty() &&
                        std::ranges::find(ownerStruct->trustedTypeKeys, trustedKey) != ownerStruct->trustedTypeKeys.end();
                }
            }

            const std::string ownerTypeName = formatAccessContextType(ownerType);
            const std::string currentContextTypeName = formatAccessContextType(currentStructType);
            bool isAccessible = true;

            if (member->flags.get_isPrivate() && !isInsideSameObject && !isTrustedAccess)
            {
                WIO_LOG_ADD_ERROR(
                    location,
                    "Cannot access private member '{}' declared on '{}' from '{}'.",
                    member->name,
                    ownerTypeName,
                    currentContextTypeName
                );
                isAccessible = false;
            }

            if (member->flags.get_isProtected() && !isInsideHierarchy && !isTrustedAccess)
            {
                WIO_LOG_ADD_ERROR(
                    location,
                    "Cannot access protected member '{}' declared on '{}' from '{}'.",
                    member->name,
                    ownerTypeName,
                    currentContextTypeName
                );
                isAccessible = false;
            }

            return isAccessible;
        }

        AccessModifier getDefaultAccessModifier(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                AccessModifier fallbackAccess)
        {
            AccessModifier resolvedAccess = fallbackAccess;
            const auto defaultAttributes = getAttributeStatements(attributes, Attribute::Default);
            if (defaultAttributes.empty())
                return resolvedAccess;

            if (defaultAttributes.size() > 1)
            {
                WIO_LOG_ADD_ERROR(defaultAttributes[1]->location(), "Only one @Default(...) attribute is allowed per declaration.");
            }

            for (const auto* defaultAttribute : defaultAttributes)
            {
                const bool hasExactlyOneRawArg = defaultAttribute->args.size() == 1;
                const bool hasTypeArg =
                    !defaultAttribute->typeArgs.empty() &&
                    defaultAttribute->typeArgs.front() != nullptr;

                if (!hasExactlyOneRawArg || hasTypeArg)
                {
                    WIO_LOG_ADD_ERROR(defaultAttribute->location(), "@Default expects exactly one access modifier: public, private, or protected.");
                    continue;
                }

                const Token& accessToken = defaultAttribute->args.front();
                if (accessToken.type == TokenType::kwPublic)
                {
                    resolvedAccess = AccessModifier::Public;
                }
                else if (accessToken.type == TokenType::kwPrivate)
                {
                    resolvedAccess = AccessModifier::Private;
                }
                else if (accessToken.type == TokenType::kwProtected)
                {
                    resolvedAccess = AccessModifier::Protected;
                }
                else
                {
                    WIO_LOG_ADD_ERROR(defaultAttribute->location(), "@Default expects exactly one access modifier: public, private, or protected.");
                }
            }

            return resolvedAccess;
        }

        struct AttributeTypeArgument
        {
            Token token;
            NodePtr<TypeSpecifier> typeSpecifier;
        };

        std::vector<AttributeTypeArgument> getAttributeTypeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            std::vector<AttributeTypeArgument> allArgs;
            for (const auto& attr : attributes)
            {
                if (attr->attribute != targetAttr)
                    continue;

                for (size_t i = 0; i < attr->args.size(); ++i)
                {
                    NodePtr<TypeSpecifier> typeSpecifier = nullptr;
                    if (i < attr->typeArgs.size())
                        typeSpecifier = attr->typeArgs[i];

                    allArgs.push_back(AttributeTypeArgument{
                        .token = attr->args[i],
                        .typeSpecifier = typeSpecifier
                    });
                }
            }
            return allArgs;
        }

        std::vector<Ref<Type>> resolveExplicitSpecializationArguments(
            SemanticAnalyzer& analyzer,
            const std::vector<NodePtr<AttributeStatement>>& attributes,
            const common::Location& declarationLocation,
            const bool allowGenericPatterns = false)
        {
            const auto specializationAttributes = getAttributeStatements(attributes, Attribute::Specialize);
            if (specializationAttributes.empty())
                return {};

            if (specializationAttributes.size() > 1)
            {
                WIO_LOG_ADD_ERROR(
                    specializationAttributes[1]->location(),
                    "Only one @Specialize(...) attribute is allowed per declaration."
                );
            }

            const auto* specializationAttribute = specializationAttributes.front();
            if (!specializationAttribute || specializationAttribute->args.empty())
            {
                WIO_LOG_ADD_ERROR(declarationLocation, "@Specialize expects at least one concrete generic argument.");
                return {};
            }

            std::vector<Ref<Type>> arguments;
            arguments.reserve(specializationAttribute->args.size());
            for (size_t argumentIndex = 0; argumentIndex < specializationAttribute->args.size(); ++argumentIndex)
            {
                NodePtr<TypeSpecifier> typeSpecifier =
                    argumentIndex < specializationAttribute->typeArgs.size()
                        ? specializationAttribute->typeArgs[argumentIndex]
                        : nullptr;

                if (!typeSpecifier)
                {
                    WIO_LOG_ADD_ERROR(
                        specializationAttribute->location(),
                        "@Specialize arguments must be concrete types or compile-time integer values."
                    );
                    arguments.push_back(Compiler::get().getTypeContext().getUnknown());
                    continue;
                }

                typeSpecifier->accept(analyzer);
                Ref<Type> argumentType = typeSpecifier->refType.Lock();
                if (!argumentType || argumentType->isUnknown() ||
                    (!allowGenericPatterns && containsGenericParameterType(argumentType)))
                {
                    WIO_LOG_ADD_ERROR(
                        typeSpecifier->location(),
                        "@Specialize arguments must be fully concrete types or compile-time integer values."
                    );
                    argumentType = Compiler::get().getTypeContext().getUnknown();
                }
                arguments.push_back(argumentType);
            }

            return arguments;
        }

        AttributeTypeArgument getAttributeTypeArgument(const AttributeStatement& attribute, size_t index)
        {
            NodePtr<TypeSpecifier> typeSpecifier = nullptr;
            if (index < attribute.typeArgs.size())
                typeSpecifier = attribute.typeArgs[index];

            return AttributeTypeArgument{
                .token = index < attribute.args.size() ? attribute.args[index] : Token::invalid(),
                .typeSpecifier = typeSpecifier
            };
        }

        bool usesSingleGenericApplyConstraintList(const AttributeStatement& attribute,
                                                  const std::vector<std::string>& genericParameterNames)
        {
            return attribute.constraintGroupOffsets.empty() &&
                   genericParameterNames.size() == 1 &&
                   !attribute.args.empty();
        }

        bool hasValidGroupedApplyConstraintShape(const AttributeStatement& attribute,
                                                 const std::vector<std::string>& genericParameterNames)
        {
            return !attribute.constraintGroupOffsets.empty() &&
                   attribute.constraintGroupOffsets.size() == genericParameterNames.size() + 1 &&
                   attribute.constraintGroupOffsets.front() == 0 &&
                   attribute.constraintGroupOffsets.back() == attribute.args.size();
        }

        std::vector<AttributeTypeArgument> getApplyConstraintArguments(
            const AttributeStatement& attribute,
            const std::vector<std::string>& genericParameterNames,
            const size_t genericParameterIndex)
        {
            if (hasValidGroupedApplyConstraintShape(attribute, genericParameterNames))
            {
                if (genericParameterIndex >= genericParameterNames.size())
                    return {};

                const size_t begin = attribute.constraintGroupOffsets[genericParameterIndex];
                const size_t end = attribute.constraintGroupOffsets[genericParameterIndex + 1];
                std::vector<AttributeTypeArgument> constraintArguments;
                constraintArguments.reserve(end - begin);
                for (size_t argumentIndex = begin; argumentIndex < end; ++argumentIndex)
                    constraintArguments.push_back(getAttributeTypeArgument(attribute, argumentIndex));
                return constraintArguments;
            }

            if (usesSingleGenericApplyConstraintList(attribute, genericParameterNames))
            {
                std::vector<AttributeTypeArgument> constraintArguments;
                constraintArguments.reserve(attribute.args.size());
                for (size_t argumentIndex = 0; argumentIndex < attribute.args.size(); ++argumentIndex)
                    constraintArguments.push_back(getAttributeTypeArgument(attribute, argumentIndex));
                return constraintArguments;
            }

            if (genericParameterIndex >= attribute.args.size())
                return {};

            return { getAttributeTypeArgument(attribute, genericParameterIndex) };
        }

        bool isExactConstraintTypeMatch(const Ref<Type>& actual, const Ref<Type>& expected)
        {
            Ref<Type> lhs = unwrapAliasType(actual);
            Ref<Type> rhs = unwrapAliasType(expected);
            return lhs && rhs && lhs->isCompatibleWith(rhs) && rhs->isCompatibleWith(lhs);
        }

        bool isIntegerConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                   name == "isize" || name == "usize";
        }

        bool isNumericConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->isNumeric();
        }

        bool isFloatingConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "f32" || name == "f64";
        }

        bool isSignedConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "isize" || name == "f32" || name == "f64";
        }

        bool isUnsignedConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                   name == "usize";
        }

        bool isObjectConstraintType(const Ref<Type>& type)
        {
            auto objectType = getObjectOrInterfaceStructType(type);
            return objectType && objectType->isObject && !objectType->isInterface;
        }

        bool isInterfaceConstraintType(const Ref<Type>& type)
        {
            auto interfaceType = getObjectOrInterfaceStructType(type);
            return interfaceType && interfaceType->isInterface;
        }

        bool isComponentConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;
            auto structType = resolved.AsFast<StructType>();
            return structType && !structType->isObject && !structType->isInterface &&
                   !structType->isEnum && !structType->isFlagset;
        }

        bool isArrayConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->kind() == TypeKind::Array;
        }

        bool isReferenceConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->kind() == TypeKind::Reference;
        }

        bool isEnumConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;

            auto structType = resolved.AsFast<StructType>();
            if (!structType || structType->isObject || structType->isInterface)
                return false;

            if (auto structScope = structType->structScope.Lock())
            {
                if (auto structSymbol = structScope->resolve(structType->name))
                    return structSymbol->flags.get_isEnum();
            }

            return false;
        }

        bool isFlagsetConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;

            auto structType = resolved.AsFast<StructType>();
            if (!structType || structType->isObject || structType->isInterface)
                return false;

            if (auto structScope = structType->structScope.Lock())
            {
                if (auto structSymbol = structScope->resolve(structType->name))
                    return structSymbol->flags.get_isFlagset();
            }

            return false;
        }

        std::vector<Ref<Type>> getIntegerConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return {
                ctx.getI8(), ctx.getI16(), ctx.getI32(), ctx.getI64(),
                ctx.getU8(), ctx.getU16(), ctx.getU32(), ctx.getU64(),
                ctx.getISize(), ctx.getUSize()
            };
        }

        std::vector<Ref<Type>> getNumericConstraintCandidateTypes()
        {
            auto candidates = getIntegerConstraintCandidateTypes();
            auto& ctx = Compiler::get().getTypeContext();
            candidates.push_back(ctx.getF32());
            candidates.push_back(ctx.getF64());
            return candidates;
        }

        std::vector<Ref<Type>> getFloatingConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return { ctx.getF32(), ctx.getF64() };
        }

        std::vector<Ref<Type>> getSignedConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return {
                ctx.getI8(), ctx.getI16(), ctx.getI32(), ctx.getI64(),
                ctx.getISize(), ctx.getF32(), ctx.getF64()
            };
        }

        std::vector<Ref<Type>> getUnsignedConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return {
                ctx.getU8(), ctx.getU16(), ctx.getU32(), ctx.getU64(), ctx.getUSize()
            };
        }

        std::vector<Ref<Type>> getNoConcreteConstraintCandidateTypes()
        {
            return {};
        }

        enum class GenericConstraintTraitKind : uint8_t
        {
            IsInteger,
            IsNumeric,
            IsFloating,
            IsSigned,
            IsUnsigned,
            IsEnum,
            IsFlagset,
            IsObject,
            IsComponent,
            IsInterface,
            IsArray,
            IsReference
        };

        using GenericConstraintPredicateFn = bool (*)(const Ref<Type>&);
        using GenericConstraintCandidatesFn = std::vector<Ref<Type>> (*)();

        struct GenericConstraintTraitDescriptor
        {
            GenericConstraintTraitKind kind;
            std::string_view canonicalQualifiedName;
            std::string_view shortName;
            GenericConstraintPredicateFn predicate;
            GenericConstraintCandidatesFn candidateTypes;
        };

        const std::array<GenericConstraintTraitDescriptor, 12>& getGenericConstraintTraitDescriptors()
        {
            static const std::array<GenericConstraintTraitDescriptor, 12> descriptors = {{
                {
                    .kind = GenericConstraintTraitKind::IsInteger,
                    .canonicalQualifiedName = "std::traits::IsInteger",
                    .shortName = "IsInteger",
                    .predicate = isIntegerConstraintType,
                    .candidateTypes = getIntegerConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsNumeric,
                    .canonicalQualifiedName = "std::traits::IsNumeric",
                    .shortName = "IsNumeric",
                    .predicate = isNumericConstraintType,
                    .candidateTypes = getNumericConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsFloating,
                    .canonicalQualifiedName = "std::traits::IsFloating",
                    .shortName = "IsFloating",
                    .predicate = isFloatingConstraintType,
                    .candidateTypes = getFloatingConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsSigned,
                    .canonicalQualifiedName = "std::traits::IsSigned",
                    .shortName = "IsSigned",
                    .predicate = isSignedConstraintType,
                    .candidateTypes = getSignedConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsUnsigned,
                    .canonicalQualifiedName = "std::traits::IsUnsigned",
                    .shortName = "IsUnsigned",
                    .predicate = isUnsignedConstraintType,
                    .candidateTypes = getUnsignedConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsEnum,
                    .canonicalQualifiedName = "std::traits::IsEnum",
                    .shortName = "IsEnum",
                    .predicate = isEnumConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsFlagset,
                    .canonicalQualifiedName = "std::traits::IsFlagset",
                    .shortName = "IsFlagset",
                    .predicate = isFlagsetConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsObject,
                    .canonicalQualifiedName = "std::traits::IsObject",
                    .shortName = "IsObject",
                    .predicate = isObjectConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsComponent,
                    .canonicalQualifiedName = "std::traits::IsComponent",
                    .shortName = "IsComponent",
                    .predicate = isComponentConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsInterface,
                    .canonicalQualifiedName = "std::traits::IsInterface",
                    .shortName = "IsInterface",
                    .predicate = isInterfaceConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsArray,
                    .canonicalQualifiedName = "std::traits::IsArray",
                    .shortName = "IsArray",
                    .predicate = isArrayConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsReference,
                    .canonicalQualifiedName = "std::traits::IsReference",
                    .shortName = "IsReference",
                    .predicate = isReferenceConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                }
            }};
            return descriptors;
        }

        std::string_view getLastQualifiedSegment(std::string_view name)
        {
            const size_t separator = name.rfind("::");
            if (separator == std::string_view::npos)
                return name;

            return name.substr(separator + 2);
        }

        const GenericConstraintTraitDescriptor* findGenericConstraintTraitDescriptor(std::string_view rawName,
                                                                                     const Ref<Type>& resolvedType = nullptr)
        {
            const std::string_view lastSegment = getLastQualifiedSegment(rawName);
            for (const auto& descriptor : getGenericConstraintTraitDescriptors())
            {
                if (rawName == descriptor.shortName ||
                    rawName == descriptor.canonicalQualifiedName ||
                    lastSegment == descriptor.shortName)
                    return &descriptor;

                Ref<Type> unwrapped = unwrapAliasType(resolvedType);
                if (!unwrapped || unwrapped->kind() != TypeKind::Struct)
                    continue;

                auto structType = unwrapped.AsFast<StructType>();
                if (structType && structType->name == descriptor.shortName && structType->scopePath == "std::traits")
                    return &descriptor;
            }

            return nullptr;
        }

        std::string_view getGenericConstraintTraitDisplayName(const GenericConstraintTraitDescriptor& descriptor)
        {
            return descriptor.canonicalQualifiedName;
        }

        bool isUserDefinedTraitConstraint(const Ref<Type>& constraintType, std::string_view parameterName)
        {
            Ref<Type> resolved = unwrapAliasType(constraintType);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;

            auto traitType = resolved.AsFast<StructType>();
            if (!traitType || !traitType->isInterface || traitType->genericArguments.empty())
                return false;

            return std::ranges::any_of(traitType->genericArguments, [&](const Ref<Type>& argument)
            {
                Ref<Type> unwrappedArgument = unwrapAliasType(argument);
                if (!unwrappedArgument || unwrappedArgument->kind() != TypeKind::GenericParameter)
                    return false;
                return unwrappedArgument.AsFast<GenericParameterType>()->name == parameterName;
            });
        }

        bool matchesUserDefinedTraitConstraint(
            const Ref<Type>& bindingType,
            const Ref<Type>& constraintType,
            std::string_view parameterName)
        {
            Ref<Type> resolvedBinding = unwrapAliasType(bindingType);
            Ref<Type> resolvedConstraint = unwrapAliasType(constraintType);
            if (!resolvedBinding || !resolvedConstraint ||
                resolvedBinding->kind() != TypeKind::Struct ||
                resolvedConstraint->kind() != TypeKind::Struct)
                return false;

            auto constraintStruct = resolvedConstraint.AsFast<StructType>();
            if (!constraintStruct || !constraintStruct->isInterface)
                return false;

            std::function<bool(const Ref<Type>&)> matchesCandidate = [&](const Ref<Type>& candidateType) -> bool
            {
                Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                if (!resolvedCandidate || resolvedCandidate->kind() != TypeKind::Struct)
                    return false;

                auto candidateStruct = resolvedCandidate.AsFast<StructType>();
                if (!candidateStruct)
                    return false;

                if (candidateStruct->name == constraintStruct->name &&
                    candidateStruct->scopePath == constraintStruct->scopePath &&
                    candidateStruct->genericArguments.size() == constraintStruct->genericArguments.size())
                {
                    bool argumentsMatch = true;
                    for (size_t i = 0; i < constraintStruct->genericArguments.size(); ++i)
                    {
                        Ref<Type> expected = unwrapAliasType(constraintStruct->genericArguments[i]);
                        Ref<Type> actual = unwrapAliasType(candidateStruct->genericArguments[i]);
                        if (expected && expected->kind() == TypeKind::GenericParameter &&
                            expected.AsFast<GenericParameterType>()->name == parameterName)
                        {
                            if (!isExactConstraintTypeMatch(actual, resolvedBinding))
                                argumentsMatch = false;
                        }
                        else if (!isExactConstraintTypeMatch(actual, expected))
                        {
                            argumentsMatch = false;
                        }
                    }
                    if (argumentsMatch)
                        return true;
                }

                return std::ranges::any_of(candidateStruct->baseTypes, [&](const Ref<Type>& baseType)
                {
                    return matchesCandidate(baseType);
                });
            };

            return matchesCandidate(resolvedBinding);
        }

        bool isOpenNativeTemplateIntrinsic(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            const Token* cppNameArg = getFirstAttributeArg(attributes, Attribute::CppName);
            if (!cppNameArg)
                return false;

            return cppNameArg->value == "wio::runtime::EnumCount" ||
                   cppNameArg->value == "wio::runtime::EnumName" ||
                   cppNameArg->value == "wio::runtime::EnumValue" ||
                   cppNameArg->value == "wio::runtime::EnumIndex" ||
                   cppNameArg->value == "wio::runtime::EnumUnderlyingTypeName" ||
                   cppNameArg->value == "wio::runtime::EnumSize" ||
                   cppNameArg->value == "wio::runtime::EnumIsValid" ||
                   cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                   cppNameArg->value == "wio::runtime::EnumFromRaw" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeName" ||
                   cppNameArg->value == "wio::runtime::ReflectedKind" ||
                   cppNameArg->value == "wio::runtime::ReflectedSize" ||
                   cppNameArg->value == "wio::runtime::ReflectedAlignment" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldTypes" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldAccess" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodSignatures" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodAccess" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodBehaviorAttributeNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodBehaviorProcessorTypes" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodBehaviorPhases" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodBehaviorHooks" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodBehaviorModes" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodBehaviorOffsets" ||
                   cppNameArg->value == "wio::runtime::ReflectedBaseTypes" ||
                   cppNameArg->value == "wio::runtime::ReflectedGenericParameterNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedGenericArguments" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributes" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeStableIds" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeRetentions" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeOrigins" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeArgumentNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeArgumentTypes" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeArgumentValues" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeArgumentUsedDefaults" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeAttributeArgumentOffsets" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldAttributeNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldAttributeOffsets" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldCount" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodCount" ||
                   cppNameArg->value == "wio::runtime::traits::IsIntegerValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsNumericValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsFloatingValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsSignedValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsUnsignedValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsArrayValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsPrimitiveValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsStringValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsTextValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsDictionaryValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsEnumValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsFlagsetValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsObjectValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsComponentValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsInterfaceValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsSameValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsDefaultConstructibleValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsCopyConstructibleValue" ||
                   cppNameArg->value == "wio::runtime::BlockOn" ||
                   cppNameArg->value == "wio::runtime::StartAsync" ||
                   cppNameArg->value == "wio::runtime::AsyncReady" ||
                   cppNameArg->value == "wio::runtime::AsyncCancelledStatus" ||
                   cppNameArg->value == "wio::runtime::AsyncFaulted" ||
                   cppNameArg->value == "wio::runtime::AsyncFailureMessage" ||
                   cppNameArg->value == "wio::runtime::AsyncWaitFor" ||
                   cppNameArg->value == "wio::runtime::CancelAsync" ||
                   cppNameArg->value == "wio::runtime::CancelAfter" ||
                   cppNameArg->value == "wio::runtime::DetachAsync" ||
                   cppNameArg->value == "wio::runtime::RunAsync" ||
                   cppNameArg->value == "wio::runtime::RunBlockingAsync" ||
                   cppNameArg->value == "wio::runtime::WhenAll" ||
                   cppNameArg->value == "wio::runtime::WhenAny" ||
                   cppNameArg->value == "wio::runtime::Race" ||
                   cppNameArg->value == "wio::runtime::WithTimeout" ||
                   cppNameArg->value == "wio::runtime::AsyncScopeSpawn" ||
                   cppNameArg->value == "wio::runtime::AsyncScopeRun" ||
                   cppNameArg->value == "wio::runtime::AsyncScopeRunBlocking";
        }

        bool matchesOpenNativeTemplateIntrinsicConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                           const std::vector<Ref<Type>>& bindingTypes)
        {
            const Token* cppNameArg = getFirstAttributeArg(attributes, Attribute::CppName);
            if (!cppNameArg)
                return true;

            if (cppNameArg->value == "wio::runtime::EnumCount" ||
                cppNameArg->value == "wio::runtime::EnumName" ||
                cppNameArg->value == "wio::runtime::EnumValue" ||
                cppNameArg->value == "wio::runtime::EnumIndex" ||
                cppNameArg->value == "wio::runtime::EnumUnderlyingTypeName" ||
                cppNameArg->value == "wio::runtime::EnumSize" ||
                cppNameArg->value == "wio::runtime::EnumIsValid" ||
                cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                cppNameArg->value == "wio::runtime::EnumFromRaw")
            {
                return bindingTypes.size() == 1 &&
                       (cppNameArg->value == "wio::runtime::EnumIsValid" ||
                        cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                        cppNameArg->value == "wio::runtime::EnumFromRaw"
                            ? isEnumConstraintType(bindingTypes.front())
                            : (isEnumConstraintType(bindingTypes.front()) || isFlagsetConstraintType(bindingTypes.front())));
            }

            return true;
        }

        Ref<Type> getPreparedConstraintType(SemanticAnalyzer& analyzer, const NodePtr<TypeSpecifier>& typeSpecifier)
        {
            if (!typeSpecifier)
                return nullptr;

            if (auto lockedType = typeSpecifier->refType.Lock(); lockedType && !lockedType->isUnknown())
                return lockedType;

            typeSpecifier->accept(analyzer);
            return typeSpecifier->refType.Lock();
        }

        bool validateGenericConstraintArgument(SemanticAnalyzer& analyzer,
                                               const AttributeTypeArgument& argument,
                                               std::string_view expectedParameterName,
                                               std::string_view attributeName,
                                               common::Location errorLocation,
                                               bool allowBoolLiteral)
        {
            if (argument.typeSpecifier)
            {
                Ref<Type> preparedConstraintType = getPreparedConstraintType(analyzer, argument.typeSpecifier);
                if (const auto* predicateTrait = findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value, preparedConstraintType))
                {
                    if (argument.typeSpecifier->generics.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(
                            errorLocation,
                            "{} predicate '{}' expects exactly one generic parameter operand.",
                            attributeName,
                            getGenericConstraintTraitDisplayName(*predicateTrait)
                        );
                        return false;
                    }

                    const auto& predicateOperand = argument.typeSpecifier->generics.front();
                    if (!predicateOperand ||
                        predicateOperand->name.type != TokenType::identifier ||
                        !predicateOperand->generics.empty() ||
                        predicateOperand->name.value != expectedParameterName)
                    {
                        WIO_LOG_ADD_ERROR(
                            errorLocation,
                            "{} predicate '{}' must target the matching generic parameter '{}'.",
                            attributeName,
                            getGenericConstraintTraitDisplayName(*predicateTrait),
                            expectedParameterName
                        );
                        return false;
                    }

                    return true;
                }

                Ref<Type> exactType = preparedConstraintType;
                if (!exactType || exactType->isUnknown())
                {
                    WIO_LOG_ADD_ERROR(
                        errorLocation,
                        "{} contains an unresolved type constraint.",
                        attributeName
                    );
                    return false;
                }

                if (containsGenericParameterType(exactType))
                {
                    if (isUserDefinedTraitConstraint(exactType, expectedParameterName))
                        return true;

                    WIO_LOG_ADD_ERROR(
                        errorLocation,
                        "{} must use fully concrete type constraints or supported predicates like std::traits::IsInteger<{}>.",
                        attributeName,
                        expectedParameterName
                    );
                    return false;
                }

                return true;
            }

            if (allowBoolLiteral && (argument.token.type == TokenType::kwTrue || argument.token.type == TokenType::kwFalse))
                return true;

            WIO_LOG_ADD_ERROR(
                errorLocation,
                "{} supports concrete types{} or supported predicates like std::traits::IsInteger<{}> and std::traits::IsNumeric<{}>.",
                attributeName,
                allowBoolLiteral ? ", boolean constants" : "",
                expectedParameterName,
                expectedParameterName
            );
            return false;
        }

        bool validateApplyAttributes(SemanticAnalyzer& analyzer,
                                     const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     std::string_view declarationKind,
                                     common::Location errorLocation)
        {
            const auto applyAttributes = getAttributeStatements(attributes, Attribute::Apply);
            if (applyAttributes.empty())
                return true;

            if (genericParameterNames.empty())
            {
                WIO_LOG_ADD_ERROR(errorLocation, "@Apply can only be used on generic {} declarations.", declarationKind);
                return false;
            }

            bool allValid = true;
            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute)
                    continue;

                if (genericParameterNames.size() == 1)
                {
                    if (applyAttribute->args.empty())
                    {
                        WIO_LOG_ADD_ERROR(
                            applyAttribute->location(),
                            "@Apply on single-parameter generic {} declarations expects at least one type or predicate argument.",
                            declarationKind
                        );
                        allValid = false;
                        continue;
                    }
                }
                else if (!hasValidGroupedApplyConstraintShape(*applyAttribute, genericParameterNames) &&
                         applyAttribute->args.size() != genericParameterNames.size())
                {
                    WIO_LOG_ADD_ERROR(
                        applyAttribute->location(),
                        "@Apply expects exactly {} arguments.",
                        genericParameterNames.size()
                    );
                    allValid = false;
                    continue;
                }

                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    const auto constraintArguments =
                        getApplyConstraintArguments(*applyAttribute, genericParameterNames, i);
                    if (constraintArguments.empty())
                    {
                        allValid = false;
                        continue;
                    }

                    for (const auto& constraintArgument : constraintArguments)
                    {
                        if (!validateGenericConstraintArgument(
                                analyzer,
                                constraintArgument,
                                genericParameterNames[i],
                                "@Apply",
                                applyAttribute->location(),
                                true))
                        {
                            allValid = false;
                        }
                    }
                }
            }

            return allValid;
        }

        bool matchesApplyConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     const bool hasGenericParameterPack,
                                     const GenericBindingSet& bindings)
        {
            const auto applyAttributes = getAttributeStatements(attributes, Attribute::Apply);
            if (applyAttributes.empty())
                return true;

            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute)
                    continue;

                if (!hasValidGroupedApplyConstraintShape(*applyAttribute, genericParameterNames) &&
                    genericParameterNames.size() != 1 &&
                    applyAttribute->args.size() != genericParameterNames.size())
                    continue;

                bool matches = true;
                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    const auto constraintArguments =
                        getApplyConstraintArguments(*applyAttribute, genericParameterNames, i);
                    const bool isPackParameter = hasGenericParameterPack && i + 1 == genericParameterNames.size();
                    const std::string& genericParameterName = genericParameterNames[i];

                    if (constraintArguments.empty())
                    {
                        matches = false;
                        break;
                    }

                    auto evaluateConstraintAgainstSingleType = [&](const AttributeTypeArgument& argument,
                                                                   const Ref<Type>& bindingType) -> bool
                    {
                        if (!bindingType || bindingType->isUnknown() || containsGenericParameterType(bindingType))
                            return true;

                        if (argument.typeSpecifier)
                        {
                            if (const auto* predicateTrait = findGenericConstraintTraitDescriptor(
                                    argument.typeSpecifier->name.value,
                                    argument.typeSpecifier->refType.Lock()))
                            {
                                return predicateTrait->predicate(bindingType);
                            }

                            Ref<Type> exactType = argument.typeSpecifier->refType.Lock();
                            if (isUserDefinedTraitConstraint(exactType, genericParameterName))
                            {
                                return matchesUserDefinedTraitConstraint(
                                    bindingType,
                                    exactType,
                                    genericParameterName);
                            }
                            return exactType && isExactConstraintTypeMatch(bindingType, exactType);
                        }

                        if (argument.token.type == TokenType::kwTrue)
                            return true;

                        if (argument.token.type == TokenType::kwFalse)
                            return false;

                        return false;
                    };

                    auto evaluateConstraintSetAgainstSingleType = [&](const Ref<Type>& bindingType) -> bool
                    {
                        if (applyAttribute->conjunctiveConstraintGroups)
                        {
                            return std::ranges::all_of(constraintArguments, [&](const AttributeTypeArgument& argument)
                            {
                                return evaluateConstraintAgainstSingleType(argument, bindingType);
                            });
                        }

                        return std::ranges::any_of(constraintArguments, [&](const AttributeTypeArgument& argument)
                        {
                            return evaluateConstraintAgainstSingleType(argument, bindingType);
                        });
                    };

                    if (!isPackParameter)
                    {
                        auto bindingIt = bindings.directBindings.find(genericParameterName);
                        if (bindingIt == bindings.directBindings.end())
                        {
                            matches = false;
                            break;
                        }

                        if (!evaluateConstraintSetAgainstSingleType(bindingIt->second))
                        {
                            matches = false;
                            break;
                        }

                        continue;
                    }

                    if (bindings.packAliases.contains(genericParameterName))
                        continue;

                    auto packIt = bindings.packBindings.find(genericParameterName);
                    if (packIt == bindings.packBindings.end())
                    {
                        matches = false;
                        break;
                    }

                    for (const auto& packElementType : packIt->second)
                    {
                        if (!evaluateConstraintSetAgainstSingleType(packElementType))
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (!matches)
                        break;
                }

                if (matches)
                    return true;
            }

            return false;
        }

        bool matchesApplyConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     const bool hasGenericParameterPack,
                                     const std::vector<Ref<Type>>& bindingTypes)
        {
            const size_t minimumBindingCount = getMinimumGenericArgumentCount(genericParameterNames, hasGenericParameterPack);
            if ((!hasGenericParameterPack && bindingTypes.size() != genericParameterNames.size()) ||
                (hasGenericParameterPack && bindingTypes.size() < minimumBindingCount))
            {
                return false;
            }

            return matchesApplyConstraints(
                attributes,
                genericParameterNames,
                hasGenericParameterPack,
                buildExtendedGenericBindings(genericParameterNames, hasGenericParameterPack, bindingTypes)
            );
        }

        std::string formatConcreteInstantiationSignature(const std::vector<Ref<Type>>& instantiationTypes)
        {
            std::string signature = "<";
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                signature += instantiationTypes[i] ? instantiationTypes[i]->toString() : "<unknown>";
                if (i + 1 < instantiationTypes.size())
                    signature += ", ";
            }
            signature += ">";
            return signature;
        }

        std::vector<std::vector<Ref<Type>>> resolveInstantiateAttributes(SemanticAnalyzer& analyzer,
                                                                         const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                                         const std::vector<std::string>& genericParameterNames,
                                                                         const std::vector<Ref<Type>>& genericParameterDefaults,
                                                                         const bool hasGenericParameterPack)
        {
            std::vector<std::vector<Ref<Type>>> instantiations;
            const auto instantiateAttributes = getAttributeStatements(attributes, Attribute::Instantiate);
            if (instantiateAttributes.empty())
                return instantiations;

            const size_t fixedCount = getMinimumGenericArgumentCount(genericParameterNames, hasGenericParameterPack);
            const size_t requiredCount = getRequiredGenericArgumentCount(genericParameterDefaults, fixedCount);
            std::unordered_set<std::string> seenInstantiationSignatures;
            for (const auto* instantiateAttribute : instantiateAttributes)
            {
                if (!instantiateAttribute)
                    continue;

                if (instantiateAttribute->args.size() < requiredCount ||
                    (!hasGenericParameterPack && instantiateAttribute->args.size() > fixedCount))
                {
                    WIO_LOG_ADD_ERROR(
                        instantiateAttribute->location(),
                        hasGenericParameterPack
                            ? common::formatString("@Instantiate expects at least {} arguments for this generic pack declaration.", requiredCount)
                            : requiredCount == fixedCount
                                ? common::formatString("@Instantiate expects exactly {} arguments.", fixedCount)
                                : common::formatString("@Instantiate expects {} to {} arguments.", requiredCount, fixedCount)
                    );
                    continue;
                }

                std::vector<std::vector<Ref<Type>>> candidateLists;
                candidateLists.reserve(fixedCount);
                std::vector<Ref<Type>> concretePackTypes;
                bool isValidInstantiation = true;
                const size_t providedFixedCount = std::min(instantiateAttribute->args.size(), fixedCount);

                for (size_t i = 0; i < providedFixedCount; ++i)
                {
                    const auto argument = getAttributeTypeArgument(*instantiateAttribute, i);
                    if (!validateGenericConstraintArgument(
                            analyzer,
                            argument,
                            genericParameterNames[i],
                            "@Instantiate",
                            instantiateAttribute->location(),
                            false))
                    {
                        isValidInstantiation = false;
                        break;
                    }

                    if (argument.typeSpecifier)
                    {
                        if (const auto* predicateTrait = findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value,
                                                                                              argument.typeSpecifier->refType.Lock()))
                        {
                            candidateLists.push_back(predicateTrait->candidateTypes());
                            continue;
                        }

                        Ref<Type> exactType = argument.typeSpecifier->refType.Lock();
                        if (!exactType)
                        {
                            isValidInstantiation = false;
                            break;
                        }

                        candidateLists.push_back({ exactType });
                        continue;
                    }

                    isValidInstantiation = false;
                    break;
                }

                if (!isValidInstantiation)
                    continue;

                if (hasGenericParameterPack)
                {
                    for (size_t i = fixedCount; i < instantiateAttribute->args.size(); ++i)
                    {
                        const auto argument = getAttributeTypeArgument(*instantiateAttribute, i);
                        if (!argument.typeSpecifier)
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "@Instantiate on generic pack declarations must provide concrete pack element types for '{}...'. Predicates belong in @Apply.",
                                genericParameterNames.back()
                            );
                            isValidInstantiation = false;
                            break;
                        }

                        Ref<Type> exactType = getPreparedConstraintType(analyzer, argument.typeSpecifier);
                        if (findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value, exactType))
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "@Instantiate on generic pack declarations must provide concrete pack element types for '{}...'. Predicates belong in @Apply.",
                                genericParameterNames.back()
                            );
                            isValidInstantiation = false;
                            break;
                        }

                        if (!exactType || exactType->isUnknown() || containsGenericParameterType(exactType))
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "@Instantiate pack element types must be fully concrete."
                            );
                            isValidInstantiation = false;
                            break;
                        }

                        concretePackTypes.push_back(exactType);
                    }
                }

                if (!isValidInstantiation)
                    continue;

                std::vector<Ref<Type>> currentInstantiation;
                currentInstantiation.reserve(candidateLists.size() + concretePackTypes.size());

                std::function<void(size_t)> expandCandidates = [&](size_t index)
                {
                    if (index == fixedCount)
                    {
                        std::vector<Ref<Type>> fullInstantiation = currentInstantiation;
                        fullInstantiation.insert(fullInstantiation.end(), concretePackTypes.begin(), concretePackTypes.end());

                        std::string signatureKey;
                        for (size_t i = 0; i < fullInstantiation.size(); ++i)
                        {
                            if (i > 0)
                                signatureKey += "|";
                            signatureKey += fullInstantiation[i] ? fullInstantiation[i]->toString() : "<unknown>";
                        }

                        if (!seenInstantiationSignatures.insert(signatureKey).second)
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "Duplicate @Instantiate declaration for '{}'.",
                                formatConcreteInstantiationSignature(fullInstantiation)
                            );
                            return;
                        }

                        instantiations.push_back(std::move(fullInstantiation));
                        return;
                    }

                    if (index >= candidateLists.size())
                    {
                        GenericBindingSet bindings;
                        for (size_t bindingIndex = 0; bindingIndex < currentInstantiation.size(); ++bindingIndex)
                            bindings.directBindings[genericParameterNames[bindingIndex]] = currentInstantiation[bindingIndex];
                        Ref<Type> defaultType = index < genericParameterDefaults.size()
                            ? instantiateGenericType(genericParameterDefaults[index], bindings)
                            : nullptr;
                        if (!defaultType || defaultType->isUnknown() || containsGenericParameterType(defaultType))
                            return;
                        currentInstantiation.push_back(defaultType);
                        expandCandidates(index + 1);
                        currentInstantiation.pop_back();
                        return;
                    }

                    for (const auto& candidateType : candidateLists[index])
                    {
                        currentInstantiation.push_back(candidateType);
                        expandCandidates(index + 1);
                        currentInstantiation.pop_back();
                    }
                };

                expandCandidates(0);
            }

            return instantiations;
        }
