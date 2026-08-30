// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        bool isNullableCapableType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->isUnknown() || current->kind() == TypeKind::Nullable)
                return false;

            if (current->kind() == TypeKind::Reference || current->kind() == TypeKind::Function)
                return true;

            if (current->kind() == TypeKind::Primitive)
                return current.AsFast<PrimitiveType>()->name == "opaque";

            if (current->kind() == TypeKind::Struct)
            {
                auto structType = current.AsFast<StructType>();
                return structType->isObject || structType->isInterface;
            }

            return false;
        }

        bool requiresExplicitNonNullInitialization(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->isUnknown() || current->kind() == TypeKind::Nullable)
                return false;
            if (current->kind() == TypeKind::Reference || current->kind() == TypeKind::Function)
                return true;
            if (current->kind() == TypeKind::Primitive)
                return current.AsFast<PrimitiveType>()->name == "opaque";
            if (current->kind() == TypeKind::Struct)
            {
                auto structType = current.AsFast<StructType>();
                return structType->isObject || structType->isInterface;
            }
            return false;
        }

        bool isMutableReferenceTypeChain(Ref<Type> type)
        {
            Ref<Type> current = unwrapAliasType(type);
            bool sawReference = false;

            while (current && current->kind() == TypeKind::Reference)
            {
                sawReference = true;

                auto referenceType = current.AsFast<ReferenceType>();
                if (!referenceType->isMutable)
                    return false;

                current = unwrapAliasType(referenceType->referredType);
            }

            return sawReference;
        }

        bool canMutateIntrinsicReceiver(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (auto memberAccess = expression->as<MemberAccessExpression>())
            {
                if (auto memberSymbol = memberAccess->referencedSymbol.Lock(); memberSymbol)
                {
                    if ((memberSymbol->kind == SymbolKind::Variable || memberSymbol->kind == SymbolKind::Parameter) &&
                        canMutateIntrinsicReceiver(memberAccess->object))
                    {
                        return true;
                    }
                }
            }

            if (auto receiverSymbol = expression->referencedSymbol.Lock(); receiverSymbol)
            {
                if (receiverSymbol->flags.get_isMutable())
                    return true;

                if (isMutableReferenceTypeChain(receiverSymbol->type))
                    return true;
            }

            return isMutableReferenceTypeChain(expression->refType.Lock());
        }

        bool isMutableAddressableOperand(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (auto receiverSymbol = expression->referencedSymbol.Lock(); receiverSymbol)
            {
                if (receiverSymbol->flags.get_isMutable())
                    return true;

                if (isMutableReferenceTypeChain(receiverSymbol->type))
                    return true;
            }

            if (expression->is<ArrayAccessExpression>())
            {
                auto* arrayAccess = expression->as<ArrayAccessExpression>();
                return isMutableAddressableOperand(arrayAccess->object) ||
                       isMutableReferenceTypeChain(arrayAccess->object ? arrayAccess->object->refType.Lock() : nullptr);
            }

            if (auto* memberAccess = expression->as<MemberAccessExpression>())
            {
                if (auto memberSymbol = memberAccess->referencedSymbol.Lock(); memberSymbol)
                {
                    if (memberSymbol->flags.get_isMutable())
                        return true;

                    if (memberSymbol->flags.get_isReadOnly())
                        return false;
                }

                return isMutableAddressableOperand(memberAccess->object) ||
                       isMutableReferenceTypeChain(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
            }

            return isMutableReferenceTypeChain(expression->refType.Lock());
        }

        bool isUnsupportedStaticArrayMember(const Ref<Type>& type, std::string_view memberName)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != TypeKind::Array)
                return false;

            auto arrayType = resolvedType.AsFast<ArrayType>();
            if (arrayType->arrayKind != ArrayType::ArrayKind::Static)
                return false;

            return isDynamicArrayOnlyIntrinsicMemberName(memberName);
        }

        bool isStringType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "string";
        }

        bool isTextType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "text";
        }

        bool isOpaqueType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "opaque";
        }

        bool isAnyType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "any";
        }

        bool isConcreteObjectTypeSupportedByAny(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != TypeKind::Struct)
                return false;

            auto structType = resolvedType.AsFast<StructType>();
            return structType && structType->isObject && !structType->isInterface;
        }

        bool isStorableInAny(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->isUnknown())
                return false;

            switch (resolvedType->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string& primitiveName = resolvedType.AsFast<PrimitiveType>()->name;
                return primitiveName != "void" &&
                       primitiveName != "<unknown>";
            }
            case TypeKind::Array:
            case TypeKind::Dictionary:
                return true;
            case TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<StructType>();
                if (!structType)
                    return false;

                return true;
            }
            default:
                return false;
            }
        }

        bool isSupportedAnyCastTargetType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->isUnknown())
                return false;

            if (isAnyType(resolvedType))
                return false;

            if (resolvedType->kind() == TypeKind::Struct)
            {
                auto structType = resolvedType.AsFast<StructType>();
                if (structType && structType->isInterface)
                    return true;
            }

            return isStorableInAny(resolvedType);
        }

        bool isIntrinsicReceiverType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return false;

            return resolvedType->kind() == TypeKind::Array ||
                   resolvedType->kind() == TypeKind::Dictionary ||
                   resolvedType->kind() == TypeKind::AsyncTask ||
                   (resolvedType->kind() == TypeKind::Struct &&
                    (resolvedType.AsFast<StructType>()->isEnum || resolvedType.AsFast<StructType>()->isFlagset)) ||
                   isStringType(resolvedType) ||
                   isTextType(resolvedType);
        }

        bool isIntegralLikeType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                   name == "isize" || name == "usize" || name == "byte" ||
                   name == "char" || name == "uchar";
        }

        Ref<Type> getCommonNumericType(const Ref<Type>& left, const Ref<Type>& right)
        {
            Ref<Type> lhs = unwrapAliasType(left);
            Ref<Type> rhs = unwrapAliasType(right);
            if (!lhs || !rhs || !lhs->isNumeric() || !rhs->isNumeric())
                return nullptr;

            if (lhs.Get() == rhs.Get())
                return lhs;

            struct NumericInfo
            {
                int bits = 0;
                bool isSigned = false;
                bool isFloat = false;
                bool isSize = false;
            };

            auto describe = [](const Ref<Type>& type) -> NumericInfo
            {
                const std::string& name = type.AsFast<PrimitiveType>()->name;
                if (name == "f32") return {32, true, true, false};
                if (name == "f64") return {64, true, true, false};
                if (name == "isize") return {64, true, false, true};
                if (name == "usize") return {64, false, false, true};
                const bool isSigned = name.starts_with('i');
                if (name.ends_with("8")) return {8, isSigned, false, false};
                if (name.ends_with("16")) return {16, isSigned, false, false};
                if (name.ends_with("32")) return {32, isSigned, false, false};
                return {64, isSigned, false, false};
            };

            const NumericInfo lhsInfo = describe(lhs);
            const NumericInfo rhsInfo = describe(rhs);
            auto& types = Compiler::get().getTypeContext();

            if (lhsInfo.isFloat || rhsInfo.isFloat)
            {
                const bool hasF64 =
                    (lhsInfo.isFloat && lhsInfo.bits == 64) ||
                    (rhsInfo.isFloat && rhsInfo.bits == 64);
                return hasF64 ? types.getF64() : types.getF32();
            }

            const NumericInfo* resultInfo = nullptr;
            if (lhsInfo.isSigned == rhsInfo.isSigned)
                resultInfo = lhsInfo.bits >= rhsInfo.bits ? &lhsInfo : &rhsInfo;
            else
            {
                const NumericInfo& signedInfo = lhsInfo.isSigned ? lhsInfo : rhsInfo;
                const NumericInfo& unsignedInfo = lhsInfo.isSigned ? rhsInfo : lhsInfo;
                resultInfo = signedInfo.bits > unsignedInfo.bits ? &signedInfo : &unsignedInfo;
            }

            const int bits = resultInfo->bits;
            const bool useSizeType = resultInfo->isSize && bits == 64;
            if (resultInfo->isSigned)
            {
                if (useSizeType) return types.getISize();
                if (bits <= 8) return types.getI8();
                if (bits <= 16) return types.getI16();
                if (bits <= 32) return types.getI32();
                return types.getI64();
            }

            if (useSizeType) return types.getUSize();
            if (bits <= 8) return types.getU8();
            if (bits <= 16) return types.getU16();
            if (bits <= 32) return types.getU32();
            return types.getU64();
        }

        std::optional<IntegerType> tryGetContextualIntegerLiteralType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return std::nullopt;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            if (name == "i8") return IntegerType::i8;
            if (name == "i16") return IntegerType::i16;
            if (name == "i32") return IntegerType::i32;
            if (name == "i64") return IntegerType::i64;
            if (name == "u8" || name == "byte") return IntegerType::u8;
            if (name == "u16") return IntegerType::u16;
            if (name == "u32") return IntegerType::u32;
            if (name == "u64") return IntegerType::u64;
            if (name == "isize") return IntegerType::isize;
            if (name == "usize") return IntegerType::usize;
            return std::nullopt;
        }

        std::optional<FloatType> tryGetContextualFloatLiteralType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return std::nullopt;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            if (name == "f32") return FloatType::f32;
            if (name == "f64") return FloatType::f64;
            return std::nullopt;
        }

        struct NumericConversionInfo
        {
            int bits = 0;
            bool isSigned = false;
            bool isFloat = false;
        };

        std::optional<NumericConversionInfo> describeNumericConversionType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive || !resolved->isNumeric())
                return std::nullopt;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            if (name == "f32") return NumericConversionInfo{32, true, true};
            if (name == "f64") return NumericConversionInfo{64, true, true};
            if (name == "isize") return NumericConversionInfo{64, true, false};
            if (name == "usize") return NumericConversionInfo{64, false, false};
            if (name == "byte") return NumericConversionInfo{8, false, false};
            const bool isSigned = name.starts_with('i');
            if (name.ends_with("8")) return NumericConversionInfo{8, isSigned, false};
            if (name.ends_with("16")) return NumericConversionInfo{16, isSigned, false};
            if (name.ends_with("32")) return NumericConversionInfo{32, isSigned, false};
            if (name.ends_with("64")) return NumericConversionInfo{64, isSigned, false};
            return std::nullopt;
        }

        bool isSafeImplicitNumericConversion(const Ref<Type>& destination, const Ref<Type>& source)
        {
            Ref<Type> resolvedDestination = unwrapAliasType(destination);
            Ref<Type> resolvedSource = unwrapAliasType(source);
            if (!resolvedDestination || !resolvedSource)
                return false;
            if (resolvedDestination.Get() == resolvedSource.Get())
                return true;

            const auto destinationInfo = describeNumericConversionType(resolvedDestination);
            const auto sourceInfo = describeNumericConversionType(resolvedSource);
            if (!destinationInfo || !sourceInfo)
                return false;

            if (destinationInfo->isFloat)
            {
                if (sourceInfo->isFloat)
                    return destinationInfo->bits >= sourceInfo->bits;

                // IEEE-754 f32/f64 exactly represent all integers with at most
                // 24/53 value bits respectively. Wider integer conversions must
                // be explicit because they may silently lose precision.
                const int exactIntegerBits = destinationInfo->bits == 32 ? 24 : 53;
                const int sourceValueBits = sourceInfo->bits - (sourceInfo->isSigned ? 1 : 0);
                return sourceValueBits <= exactIntegerBits;
            }

            if (sourceInfo->isFloat)
                return false;

            if (destinationInfo->isSigned == sourceInfo->isSigned)
                return destinationInfo->bits >= sourceInfo->bits;
            if (destinationInfo->isSigned && !sourceInfo->isSigned)
                return destinationInfo->bits > sourceInfo->bits;
            return false;
        }

        bool isRejectedImplicitNumericConversion(const Ref<Type>& destination, const Ref<Type>& source)
        {
            Ref<Type> resolvedDestination = unwrapAliasType(destination);
            Ref<Type> resolvedSource = unwrapAliasType(source);
            return resolvedDestination && resolvedSource &&
                   resolvedDestination->isNumeric() && resolvedSource->isNumeric() &&
                   !isSafeImplicitNumericConversion(resolvedDestination, resolvedSource);
        }

        bool isAssignmentLikeCompatible(const Ref<Type>& destination, const Ref<Type>& source)
        {
            if (!destination || !source || source->isUnknown())
                return false;

            Ref<Type> resolvedDestination = unwrapAliasType(destination);
            Ref<Type> resolvedSource = unwrapAliasType(source);
            if (resolvedDestination && resolvedSource)
            {
                auto getPackElementTypes = [](const Ref<Type>& type) -> const std::vector<Ref<Type>>*
                {
                    if (!type)
                        return nullptr;

                    auto nested = [&](const std::vector<Ref<Type>>& elementTypes) -> const std::vector<Ref<Type>>*
                    {
                        if (elementTypes.size() == 1)
                        {
                            Ref<Type> nestedType = unwrapAliasType(elementTypes.front());
                            if (!nestedType)
                                return &elementTypes;

                            switch (nestedType->kind())
                            {
                            case TypeKind::ValuePackView:
                                if (!nestedType.AsFast<ValuePackViewType>()->elementTypes.empty())
                                    return &nestedType.AsFast<ValuePackViewType>()->elementTypes;
                                break;
                            case TypeKind::TypePackView:
                                if (!nestedType.AsFast<TypePackViewType>()->elementTypes.empty())
                                    return &nestedType.AsFast<TypePackViewType>()->elementTypes;
                                break;
                            case TypeKind::PackStorage:
                                if (!nestedType.AsFast<PackStorageType>()->elementTypes.empty())
                                    return &nestedType.AsFast<PackStorageType>()->elementTypes;
                                break;
                            default:
                                break;
                            }
                        }

                        return &elementTypes;
                    };

                    switch (type->kind())
                    {
                    case TypeKind::ValuePackView:
                        return nested(type.AsFast<ValuePackViewType>()->elementTypes);
                    case TypeKind::TypePackView:
                        return nested(type.AsFast<TypePackViewType>()->elementTypes);
                    case TypeKind::PackStorage:
                        return nested(type.AsFast<PackStorageType>()->elementTypes);
                    default:
                        return nullptr;
                    }
                };

                auto formatConcreteTypePackName = [](const std::vector<Ref<Type>>& elementTypes) -> std::string
                {
                    std::string result = "type-pack<";
                    for (size_t i = 0; i < elementTypes.size(); ++i)
                    {
                        result += elementTypes[i] ? elementTypes[i]->toString() : "<unknown>";
                        if (i + 1 < elementTypes.size())
                            result += ", ";
                    }
                    result += ">";
                    return result;
                };

                const auto destinationPackName = tryGetNormalizedSymbolicPackName(resolvedDestination);
                const auto sourcePackName = tryGetNormalizedSymbolicPackName(resolvedSource);
                if (destinationPackName.has_value() && sourcePackName.has_value())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment && *destinationPackName == *sourcePackName)
                        return true;
                }

                const auto destinationElements = getPackElementTypes(resolvedDestination);
                const auto sourceElements = getPackElementTypes(resolvedSource);
                if (destinationPackName.has_value() && sourceElements && !sourceElements->empty())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment && *destinationPackName == formatConcreteTypePackName(*sourceElements))
                        return true;
                }

                if (sourcePackName.has_value() && destinationElements && !destinationElements->empty())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment && *sourcePackName == formatConcreteTypePackName(*destinationElements))
                        return true;
                }

                if (destinationElements && sourceElements &&
                    !destinationElements->empty() &&
                    destinationElements->size() == sourceElements->size())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment)
                    {
                        bool allElementsCompatible = true;
                        for (size_t i = 0; i < destinationElements->size(); ++i)
                        {
                            if (!isAssignmentLikeCompatible((*destinationElements)[i], (*sourceElements)[i]))
                            {
                                allElementsCompatible = false;
                                break;
                            }
                        }

                        if (allElementsCompatible)
                            return true;
                    }
                }
            }

            if (destination->isCompatibleWith(source) ||
                isSafeImplicitNumericConversion(destination, source))
            {
                return true;
            }

            if (shouldAutoReadReferenceType(source))
            {
                Ref<Type> readableSource = getAutoReadableType(source);
                if (readableSource &&
                    (destination->isCompatibleWith(readableSource) ||
                     isSafeImplicitNumericConversion(destination, readableSource)))
                {
                    return true;
                }
            }

            return false;
        }

        bool areMatchTypesCompatible(const Ref<Type>& lhs, const Ref<Type>& rhs)
        {
            if (!lhs || !rhs || lhs->isUnknown() || rhs->isUnknown())
                return true;

            return lhs->isCompatibleWith(rhs) ||
                   rhs->isCompatibleWith(lhs) ||
                   (lhs->isNumeric() && rhs->isNumeric());
        }

        bool isGuardConditionTypeAllowed(const Ref<Type>& type)
        {
            if (!type || type->isUnknown())
                return true;

            return type == Compiler::get().getTypeContext().getBool() ||
                   type->isNumeric() ||
                   type->kind() == TypeKind::Reference ||
                   type->kind() == TypeKind::Null;
        }

        bool isVariableLikeSymbol(const Ref<Symbol>& symbol)
        {
            return symbol &&
                   (symbol->kind == SymbolKind::Variable ||
                    symbol->kind == SymbolKind::Parameter);
        }

        bool isConstScalarType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved)
                return false;

            if (resolved->kind() == TypeKind::Struct)
            {
                auto structType = resolved.AsFast<StructType>();
                if (!structType || structType->isObject || structType->isInterface)
                    return false;

                if (auto structScope = structType->structScope.Lock())
                {
                    if (auto structSymbol = structScope->resolve(structType->name))
                        return structSymbol->flags.get_isEnum() || structSymbol->flags.get_isFlagset();
                }

                return false;
            }

            if (resolved->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "bool" ||
                   name == "char" ||
                   name == "uchar" ||
                   name == "byte" ||
                   name == "i8" ||
                   name == "i16" ||
                   name == "i32" ||
                   name == "i64" ||
                   name == "u8" ||
                   name == "u16" ||
                   name == "u32" ||
                   name == "u64" ||
                   name == "f32" ||
                   name == "f64" ||
                   name == "isize" ||
                   name == "usize" ||
                   name == "string" ||
                   name == "text";
        }

        bool isAllowedConstBinaryOperator(TokenType op)
        {
            return op == TokenType::opPlus ||
                   op == TokenType::opMinus ||
                   op == TokenType::opStar ||
                   op == TokenType::opSlash ||
                   op == TokenType::opPercent ||
                   op == TokenType::opBitAnd ||
                   op == TokenType::opBitOr ||
                   op == TokenType::opBitXor ||
                   op == TokenType::opShiftLeft ||
                   op == TokenType::opShiftRight ||
                   op == TokenType::opLogicalAnd ||
                   op == TokenType::opLogicalOr ||
                   op == TokenType::kwAnd ||
                   op == TokenType::kwOr ||
                   op == TokenType::opEqual ||
                   op == TokenType::opNotEqual ||
                   op == TokenType::opLess ||
                   op == TokenType::opLessEqual ||
                   op == TokenType::opGreater ||
                   op == TokenType::opGreaterEqual;
        }

        bool isAllowedConstUnaryOperator(TokenType op)
        {
            return op == TokenType::opPlus ||
                   op == TokenType::opMinus ||
                   op == TokenType::opBitNot ||
                   op == TokenType::opLogicalNot ||
                   op == TokenType::kwNot;
        }

        bool isConstEvaluableExpression(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (expression->is<IntegerLiteral>() ||
                expression->is<FloatLiteral>() ||
                expression->is<BoolLiteral>() ||
                expression->is<CharLiteral>() ||
                expression->is<ByteLiteral>() ||
                expression->is<StringLiteral>())
            {
                return true;
            }

            if (const auto* interpolated = expression->as<InterpolatedStringLiteral>())
            {
                return std::ranges::all_of(interpolated->parts, [](const NodePtr<Expression>& part)
                {
                    return isConstEvaluableExpression(part);
                });
            }

            if (expression->is<ArrayLiteral>() ||
                expression->is<DictionaryLiteral>() ||
                expression->is<NullExpression>() ||
                expression->is<LambdaExpression>() ||
                expression->is<RefExpression>() ||
                expression->is<SelfExpression>() ||
                expression->is<SuperExpression>() ||
                expression->is<RangeExpression>() ||
                expression->is<MatchExpression>() ||
                expression->is<AssignmentExpression>() ||
                expression->is<ArrayAccessExpression>() ||
                expression->is<FunctionCallExpression>())
            {
                return false;
            }

            if (const auto* identifier = expression->as<Identifier>())
            {
                auto symbol = identifier->referencedSymbol.Lock();
                return symbol && symbol->flags.get_isConst();
            }

            if (const auto* memberAccess = expression->as<MemberAccessExpression>())
            {
                auto memberSymbol = memberAccess->referencedSymbol.Lock();
                auto ownerSymbol = memberAccess->object ? memberAccess->object->referencedSymbol.Lock() : nullptr;
                if (!memberSymbol || !ownerSymbol)
                    return false;

                return memberSymbol->flags.get_isReadOnly() &&
                       memberSymbol->kind == SymbolKind::Variable &&
                       ownerSymbol->kind == SymbolKind::Struct &&
                       (ownerSymbol->flags.get_isEnum() || ownerSymbol->flags.get_isFlagset());
            }

            if (const auto* unary = expression->as<UnaryExpression>())
            {
                return isAllowedConstUnaryOperator(unary->op.type) &&
                       isConstEvaluableExpression(unary->operand);
            }

            if (const auto* binary = expression->as<BinaryExpression>())
            {
                return isAllowedConstBinaryOperator(binary->op.type) &&
                       isConstEvaluableExpression(binary->left) &&
                       isConstEvaluableExpression(binary->right);
            }

            if (const auto* fit = expression->as<FitExpression>())
            {
                Ref<Type> operandType = unwrapAliasType(fit->operand->refType.Lock());
                Ref<Type> targetType = fit->targetType ? unwrapAliasType(fit->targetType->refType.Lock()) : nullptr;
                return isConstEvaluableExpression(fit->operand) &&
                       operandType && targetType &&
                       operandType->isNumeric() &&
                       targetType->isNumeric();
            }

            return false;
        }
