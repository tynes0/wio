// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

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
            return;
        }

        if (node.packIndex)
        {
            if (auto resolvedType = node.refType.Lock();
                resolvedType && !containsGenericParameterTypeForCodegen(resolvedType))
            {
                emit(toCppType(resolvedType));
                return;
            }

            if (auto indexValue = tryEvaluateStaticPackIndex(node.packIndex, variableDeclarationsBySymbol_))
            {
                emit(common::formatString(
                    "typename wio::meta::TypePackView<{}...>::template At<{}>",
                    node.name.value,
                    *indexValue
                ));
                return;
            }
        }

        if (auto resolvedType = node.refType.Lock())
        {
            emit(toCppType(resolvedType));
            return;
        }

        emit(node.name.value);
    }

    void CppGenerator::visit(BinaryExpression& node)
    {
        auto emitAssignableLeftExpression = [&](const NodePtr<Expression>& expression)
        {
            Ref<sema::Type> expressionType = expression ? expression->refType.Lock() : nullptr;
            if (shouldTreatReferenceAsHandleForAssignment(expressionType))
            {
                expression->accept(*this);
                return;
            }

            emitReadableExpression(expression);
        };

        auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
        {
            std::string accessOperator = ".";
            std::size_t referenceDepth = 0;
            Ref<sema::Type> terminalType = nullptr;

            if (auto receiverType = receiver ? receiver->refType.Lock() : nullptr)
            {
                auto baseType = receiverType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;

                while (baseType && baseType->kind() == sema::TypeKind::Reference)
                {
                    referenceDepth++;
                    baseType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    while (baseType && baseType->kind() == sema::TypeKind::Alias)
                        baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                }

                terminalType = baseType;
                if (referenceDepth > 0)
                {
                    accessOperator = "->";
                }
                else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = terminalType.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        accessOperator = "->";
                }
            }

            if (referenceDepth <= 1)
            {
                receiver->accept(*this);
            }
            else
            {
                emit("(");
                for (std::size_t i = 0; i < referenceDepth - 1; ++i)
                    emit("*");
                emit("(");
                receiver->accept(*this);
                emit("))");
            }

            emit(accessOperator);
        };

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 1u : 2u
            );
            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.left);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.right);
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 1)
                    emitExpressionWithExpectedType(node.left, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.left);
                emit(", ");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 2)
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[1], true);
                else
                    emitReadableExpression(node.right);
                emit(")");
            }
            return;
        }

        if (node.op.type == TokenType::kwIn)
        {
            if (node.right->is<RangeExpression>())
            {
                auto range = node.right->as<RangeExpression>();
                // C++ Output: [&](){ auto _val = (x); return _val >= (1) && _val <= (5); }()
                emit("[&](){ auto _val = (");
                emitReadableExpression(node.left);
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
            if (isAnyTypeForCodegen(node.left->refType.Lock()))
            {
                Ref<sema::Type> targetType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
                emit("([&](){ wio::runtime::Any _wio_any = ");
                node.left->accept(*this);
                emit("; return ");

                if (targetType && targetType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = targetType.AsFast<sema::StructType>();
                    if (structType->isInterface)
                    {
                        emit("_wio_any.IsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">()");
                    }
                    else if (structType->isObject && !structType->isInterface)
                    {
                        emit("!_wio_any.IsNull() && _wio_any.CanCastObject<" + mangleStructTypeName(structType) + ">()");
                    }
                    else
                    {
                        emit("_wio_any.IsBoxed<" + toCppType(targetType) + ">()");
                    }
                }
                else if (isOpaqueTypeForCodegen(targetType))
                {
                    emit("_wio_any.IsOpaque()");
                }
                else
                {
                    emit("_wio_any.IsBoxed<" + toCppType(targetType) + ">()");
                }

                emit("; }())");
                return;
            }

            node.left->accept(*this);

            bool isFatPointer = false;
            if (const auto& lhsType = node.left->refType.Lock())
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

            Ref<sema::Type> rhsType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
            auto targetStruct = rhsType && rhsType->kind() == sema::TypeKind::Struct
                ? rhsType.AsFast<sema::StructType>()
                : nullptr;

            if (targetStruct)
            {
                if (targetStruct->isInterface)
                    emit(mangleInterfaceTypeName(targetStruct) + "::TYPE_ID");
                else
                    emit(mangleStructTypeName(targetStruct) + "::TYPE_ID");
            }
            else if (auto rightSym = node.right->referencedSymbol.Lock(); rightSym && rightSym->kind == sema::SymbolKind::Struct)
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
            emit(")");
            return;//
        }

        const Ref<sema::Type> arithmeticResultType = unwrapAliasTypeForCodegen(node.refType.Lock());
        const auto isIntegerPrimitive = [](const Ref<sema::Type>& type)
        {
            if (!type || type->kind() != sema::TypeKind::Primitive)
                return false;
            const std::string& name = type.AsFast<sema::PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "isize" || name == "u8" || name == "u16" || name == "u32" ||
                   name == "u64" || name == "usize" || name == "byte";
        };

        const auto isStringPrimitive = [](const Ref<sema::Type>& type)
        {
            return type && type->kind() == sema::TypeKind::Primitive &&
                   type.AsFast<sema::PrimitiveType>()->name == "string";
        };
        const Ref<sema::Type> textualLeftType = unwrapAliasTypeForCodegen(node.left->refType.Lock());
        const Ref<sema::Type> textualRightType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
        const bool isStringBinaryOperator =
            node.op.type == TokenType::opPlus ||
            node.op.type == TokenType::opEqual ||
            node.op.type == TokenType::opNotEqual ||
            node.op.type == TokenType::opLess ||
            node.op.type == TokenType::opLessEqual ||
            node.op.type == TokenType::opGreater ||
            node.op.type == TokenType::opGreaterEqual;
        if (isStringBinaryOperator &&
            isStringPrimitive(textualLeftType) &&
            isStringPrimitive(textualRightType))
        {
            emit("(wio::String(");
            emitReadableExpression(node.left);
            emit(") " + node.op.value + " ");
            emitReadableExpression(node.right);
            emit(")");
            return;
        }

        std::string_view integerHelper;
        if (isIntegerPrimitive(arithmeticResultType))
        {
            switch (node.op.type)
            {
            case TokenType::opPlus: integerHelper = "WrappingAdd"; break;
            case TokenType::opMinus: integerHelper = "WrappingSub"; break;
            case TokenType::opStar: integerHelper = "WrappingMul"; break;
            case TokenType::opSlash: integerHelper = "IntegerDivide"; break;
            case TokenType::opPercent: integerHelper = "IntegerRemainder"; break;
            default: break;
            }
        }

        if (!integerHelper.empty())
        {
            emit("wio::intrinsics::" + std::string(integerHelper) + "<" + toCppType(arithmeticResultType) + ">(");
            emitReadableExpression(node.left);
            emit(", ");
            emitReadableExpression(node.right);
            emit(")");
            return;
        }

        emit("(");
        const Ref<sema::Type> leftType = unwrapAliasTypeForCodegen(node.left->refType.Lock());
        const Ref<sema::Type> rightType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
        const bool comparesWithNull =
            (node.op.type == TokenType::opEqual || node.op.type == TokenType::opNotEqual) &&
            ((leftType && leftType->kind() == sema::TypeKind::Null) ||
             (rightType && rightType->kind() == sema::TypeKind::Null));

        if (node.op.type == TokenType::opAssign)
            emitAssignableLeftExpression(node.left);
        else if (comparesWithNull && leftType && leftType->kind() == sema::TypeKind::Reference)
            node.left->accept(*this);
        else
            emitReadableExpression(node.left);

        std::string opStr = node.op.value;
        if (node.op.type == TokenType::kwAnd)
            opStr = "&&";
        else if (node.op.type == TokenType::kwOr)
            opStr = "||";

        emit(" " + opStr + " ");
        if (comparesWithNull && rightType && rightType->kind() == sema::TypeKind::Reference)
            node.right->accept(*this);
        else
            emitReadableExpression(node.right);
        emit(")");
    }

    void CppGenerator::visit(ConditionalExpression& node)
    {
        emit("(");
        emitReadableExpression(node.condition);
        emit(" ? ");
        emitExpressionWithExpectedType(node.whenTrue, node.refType.Lock(), false);
        emit(" : ");
        emitExpressionWithExpectedType(node.whenFalse, node.refType.Lock(), false);
        emit(")");
    }

    void CppGenerator::visit(UnaryExpression& node)
    {
        if (node.isMainExecutorAwait)
        {
            emit("(co_await wio::runtime::AsyncMainAwaiter{})");
            return;
        }

        auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
        {
            std::string accessOperator = ".";
            std::size_t referenceDepth = 0;
            Ref<sema::Type> terminalType = nullptr;

            if (auto receiverType = receiver ? receiver->refType.Lock() : nullptr)
            {
                auto baseType = receiverType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;

                while (baseType && baseType->kind() == sema::TypeKind::Reference)
                {
                    referenceDepth++;
                    baseType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    while (baseType && baseType->kind() == sema::TypeKind::Alias)
                        baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                }

                terminalType = baseType;
                if (referenceDepth > 0)
                {
                    accessOperator = "->";
                }
                else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = terminalType.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        accessOperator = "->";
                }
            }

            if (referenceDepth <= 1)
            {
                receiver->accept(*this);
            }
            else
            {
                emit("(");
                for (std::size_t i = 0; i < referenceDepth - 1; ++i)
                    emit("*");
                emit("(");
                receiver->accept(*this);
                emit("))");
            }

            emit(accessOperator);
        };

        if (node.op.type == TokenType::kwAwait)
        {
            emit("co_await ");
            node.operand->accept(*this);
            return;
        }

        if (node.op.type == TokenType::kwDeref)
        {
            Ref<sema::Type> operandType = unwrapAliasTypeForCodegen(node.operand ? node.operand->refType.Lock() : nullptr);
            if (operandType && operandType->kind() == sema::TypeKind::Reference)
            {
                Ref<sema::Type> referredType = unwrapAliasTypeForCodegen(
                    operandType.AsFast<sema::ReferenceType>()->referredType
                );
                if (referredType && referredType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = referredType.AsFast<sema::StructType>();
                    if (structType->isObject)
                    {
                        emit("wio::runtime::OwnObjectReference<" + mangleStructTypeName(structType) + ">(");
                        node.operand->accept(*this);
                        emit(")");
                        return;
                    }

                    if (structType->isInterface)
                    {
                        node.operand->accept(*this);
                        return;
                    }
                }
            }

            emit("*(");
            node.operand->accept(*this);
            emit(")");
            return;
        }

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 0u : 1u
            );
            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.operand);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("())");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.operand, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.operand);
                emit(")");
            }
            return;
        }

        Ref<sema::Type> unaryResultType = unwrapAliasTypeForCodegen(node.refType.Lock());
        if (node.op.type == TokenType::opMinus &&
            unaryResultType && unaryResultType->kind() == sema::TypeKind::Primitive)
        {
            const std::string& name = unaryResultType.AsFast<sema::PrimitiveType>()->name;
            const bool isInteger =
                name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "isize" ||
                name == "u8" || name == "u16" || name == "u32" || name == "u64" || name == "usize" || name == "byte";
            if (isInteger)
            {
                emit("wio::intrinsics::WrappingNeg<" + toCppType(unaryResultType) + ">(");
                emitReadableExpression(node.operand);
                emit(")");
                return;
            }
        }

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
        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 1u : 2u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                std::string accessOperator = ".";
                std::size_t referenceDepth = 0;
                Ref<sema::Type> terminalType = nullptr;

                if (auto receiverType = receiver ? receiver->refType.Lock() : nullptr)
                {
                    auto baseType = receiverType;
                    while (baseType && baseType->kind() == sema::TypeKind::Alias)
                        baseType = baseType.AsFast<sema::AliasType>()->aliasedType;

                    while (baseType && baseType->kind() == sema::TypeKind::Reference)
                    {
                        referenceDepth++;
                        baseType = baseType.AsFast<sema::ReferenceType>()->referredType;
                        while (baseType && baseType->kind() == sema::TypeKind::Alias)
                            baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                    }

                    terminalType = baseType;
                    if (referenceDepth > 0)
                        accessOperator = "->";
                    else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                    {
                        auto structType = terminalType.AsFast<sema::StructType>();
                        if (structType->isObject || structType->isInterface)
                            accessOperator = "->";
                    }
                }

                if (referenceDepth <= 1)
                {
                    receiver->accept(*this);
                }
                else
                {
                    emit("(");
                    for (std::size_t i = 0; i < referenceDepth - 1; ++i)
                        emit("*");
                    emit("(");
                    receiver->accept(*this);
                    emit("))");
                }

                emit(accessOperator);
            };

            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.left);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.right);
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 1)
                    emitExpressionWithExpectedType(node.left, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.left);
                emit(", ");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 2)
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[1], true);
                else
                    emitReadableExpression(node.right);
                emit(")");
            }
            return;
        }

        auto lhsType = node.left->refType.Lock();
        auto rhsType = node.right->refType.Lock();
        auto emitAssignmentTarget = [&]()
        {
            if (shouldTreatReferenceAsHandleForAssignment(lhsType))
            {
                node.left->accept(*this);
            }
            else
            {
                int derefCount = 0;

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
            }
        };

        Ref<sema::Type> assignmentValueType = unwrapAliasTypeForCodegen(lhsType);
        while (assignmentValueType && assignmentValueType->kind() == sema::TypeKind::Reference)
            assignmentValueType = unwrapAliasTypeForCodegen(assignmentValueType.AsFast<sema::ReferenceType>()->referredType);

        bool isIntegerAssignment = false;
        if (assignmentValueType && assignmentValueType->kind() == sema::TypeKind::Primitive)
        {
            const std::string& name = assignmentValueType.AsFast<sema::PrimitiveType>()->name;
            isIntegerAssignment =
                name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "isize" ||
                name == "u8" || name == "u16" || name == "u32" || name == "u64" || name == "usize" || name == "byte";
        }

        std::string_view assignmentHelper;
        if (isIntegerAssignment)
        {
            switch (node.op.type)
            {
            case TokenType::opPlusAssign: assignmentHelper = "WrappingAddAssign"; break;
            case TokenType::opMinusAssign: assignmentHelper = "WrappingSubAssign"; break;
            case TokenType::opStarAssign: assignmentHelper = "WrappingMulAssign"; break;
            case TokenType::opSlashAssign: assignmentHelper = "IntegerDivideAssign"; break;
            case TokenType::opPercentAssign: assignmentHelper = "IntegerRemainderAssign"; break;
            default: break;
            }
        }

        if (!assignmentHelper.empty())
        {
            emit("wio::intrinsics::" + std::string(assignmentHelper) + "(");
            emitAssignmentTarget();
            emit(", ");
            emitExpressionWithExpectedType(node.right, assignmentValueType, false);
            emit(")");
            return;
        }

        emitAssignmentTarget();

        emit(" " + node.op.value + " "); // =, +=, -= ...

        emitExpressionWithExpectedType(node.right, lhsType, false);
    }

    void CppGenerator::visit(IntegerLiteral& node)
    {
        std::string valStr = common::stripIntegerLiteralTypeSuffix(node.token.value);

        auto type = node.refType.Lock();
        std::string tName = type ? type->toString() : "i32";

        if (tName == "i64" && valStr == "-9223372036854775808")
            emit("(std::numeric_limits<int64_t>::min)()");
        else if (tName == "isize" && valStr == "-9223372036854775808")
            emit("(std::numeric_limits<ptrdiff_t>::min)()");
        else
        {
            std::string literalExpression = valStr;
            if (tName == "u32")
                literalExpression += "u";
            else if (tName == "i64" || tName == "isize")
                literalExpression += "ll";
            else if (tName == "u64" || tName == "usize")
                literalExpression += "ull";

            // C++ fixed-width aliases differ across ABIs: int64_t/size_t are
            // long on LP64 Linux but long long literals remain long long.
            // Preserve the Wio type at the expression boundary so generic
            // deduction and overload resolution are platform-independent.
            const bool isIntegerType =
                tName == "i8" || tName == "i16" || tName == "i32" || tName == "i64" || tName == "isize" ||
                tName == "u8" || tName == "u16" || tName == "u32" || tName == "u64" || tName == "usize" ||
                tName == "byte";
            if (type && isIntegerType)
                emit("static_cast<" + type->toCppString() + ">(" + literalExpression + ")");
            else
                emit(literalExpression);
        }
    }

    void CppGenerator::visit(FloatLiteral& node)
    {
        std::string valStr = common::stripFloatLiteralTypeSuffix(node.token.value);
        if (valStr.find_first_of(".eE") == std::string::npos)
            valStr += ".0";

        auto type = node.refType.Lock();
        if (type && type->toString() == "f64")
            emit(valStr);
        else
            emit(valStr + "f");
    }

    void CppGenerator::visit(StringLiteral& node)
    {
        const std::string literal = "\"" + common::wioStringToEscapedCppString(node.token.value) + "\"";
        if (node.token.isUnicodeString)
            emit("wio::runtime::Text::FromUtf8(" + literal + ")");
        else
            emit(literal);
    }

    void CppGenerator::visit(InterpolatedStringLiteral& node)
    {
        if (node.parts.empty())
        {
            emit(node.isUnicode ? "wio::runtime::Text{}" : "\"\"");
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

        if (node.isUnicode)
            emit("wio::runtime::Text::FromUtf8(");
        emit("wio::runtime::Format(\"" + formatString + "\"");

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
        if (node.isUnicode)
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
            if (sym->kind == sema::SymbolKind::Function)
            {
                auto funcType = sym->type.AsFast<sema::FunctionType>();
                emit(Mangler::mangleFunction(
                    sym->name,
                    funcType->paramTypes,
                    isStructMemberFunctionSymbol(sym) ? "" : sym->scopePath
                ));
                return;
            }

            if (sym->kind == sema::SymbolKind::FunctionGroup)
            {
                auto selectedType = node.refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                {
                    if (!sym->overloads.empty())
                        selectedType = sym->overloads.front()->type;
                }

                std::string scopePath = sym->scopePath;
                if (scopePath.empty() && !sym->overloads.empty())
                    scopePath = sym->overloads.front()->scopePath;

                if (selectedType && selectedType->kind() == sema::TypeKind::Function)
                {
                    auto funcType = selectedType.AsFast<sema::FunctionType>();
                    emit(Mangler::mangleFunction(
                        sym->name,
                        funcType->paramTypes,
                        isStructMemberFunctionSymbol(sym) ? "" : scopePath
                    ));
                    return;
                }
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

        Ref<sema::Type> identifierType = unwrapAliasTypeForCodegen(node.refType.Lock());
        if (!node.referencedSymbol.Lock() &&
            identifierType && identifierType->kind() == sema::TypeKind::Primitive)
        {
            const std::string& name = identifierType.AsFast<sema::PrimitiveType>()->name;
            if (name == "string" || name == "text")
            {
                emit(sanitizeCppIdentifier(node.token.value) + ".RuntimeValue()");
                return;
            }
        }

        emit(sanitizeCppIdentifier(node.token.value));
    }

    void CppGenerator::visit(TypeExpression& node)
    {
        if (auto resolvedType = node.refType.Lock())
        {
            emit(toCppType(resolvedType));
            return;
        }

        if (node.type)
        {
            node.type->accept(*this);
            return;
        }

        emit("/* invalid type expression */");
    }

    void CppGenerator::visit(PackExpansionExpression& node)
    {
        if (node.operand)
            node.operand->accept(*this);
        emit("...");
    }

    void CppGenerator::visit(NullExpression& node)
    {
        auto lockedRefType = node.refType.Lock();
        if (lockedRefType && lockedRefType->kind() == sema::TypeKind::Null)
        {
            auto transformedType = lockedRefType.AsFast<sema::NullType>()->transformedType;
            auto resolvedTransformedType = unwrapAliasTypeForCodegen(transformedType);
            if (resolvedTransformedType && resolvedTransformedType->kind() == sema::TypeKind::Nullable)
                resolvedTransformedType = unwrapAliasTypeForCodegen(
                    resolvedTransformedType.AsFast<sema::NullableType>()->valueType
                );
            const bool isPointerLikeNull =
                !resolvedTransformedType ||
                resolvedTransformedType->kind() == sema::TypeKind::Null ||
                resolvedTransformedType->kind() == sema::TypeKind::Function ||
                resolvedTransformedType->kind() == sema::TypeKind::Reference ||
                (resolvedTransformedType->kind() == sema::TypeKind::Primitive &&
                 (resolvedTransformedType.AsFast<sema::PrimitiveType>()->name == "opaque" ||
                  resolvedTransformedType.AsFast<sema::PrimitiveType>()->name == "any")) ||
                (resolvedTransformedType->kind() == sema::TypeKind::Struct &&
                 ([&]()
                 {
                     auto structType = resolvedTransformedType.AsFast<sema::StructType>();
                     return structType && (structType->isObject || structType->isInterface);
                 })());

            if (isPointerLikeNull)
            {
                emit("nullptr");
                return;
            }

            emit(transformedType ? transformedType->toCppString() : std::string("void"));
            emit("{}");
        }
        else
        {
            emit("nullptr");
        }
    }

    void CppGenerator::visit(ArrayAccessExpression& node)
    {
        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 1u : 2u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                Ref<sema::Type> receiverType = receiver ? receiver->refType.Lock() : nullptr;
                Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);

                bool usePointerAccess =
                    resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Reference;
                emit("(");
                receiver->accept(*this);
                emit(")");
                emit(usePointerAccess ? "->" : ".");
            };

            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.object);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.index, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.index);
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 1)
                    emitExpressionWithExpectedType(node.object, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.object);
                emit(", ");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 2)
                    emitExpressionWithExpectedType(node.index, operatorFunctionType->paramTypes[1], true);
                else
                    emitReadableExpression(node.index);
                emit(")");
            }
            return;
        }

        Ref<sema::Type> objectType = unwrapAliasType(node.object ? node.object->refType.Lock() : nullptr);
        if (objectType)
        {
            while (objectType && objectType->kind() == sema::TypeKind::Reference)
                objectType = unwrapAliasType(objectType.AsFast<sema::ReferenceType>()->referredType);
        }

        const auto expectedPackName =
            (objectType && objectType->kind() == sema::TypeKind::GenericParameterPack) ? std::optional<std::string_view>(objectType.AsFast<sema::GenericParameterPackType>()->name) :
            (objectType && objectType->kind() == sema::TypeKind::ValuePackView) ? std::optional<std::string_view>(objectType.AsFast<sema::ValuePackViewType>()->packName) :
            (objectType && objectType->kind() == sema::TypeKind::PackStorage) ? std::optional<std::string_view>(objectType.AsFast<sema::PackStorageType>()->packName) :
            (objectType && objectType->kind() == sema::TypeKind::TypePackView) ? std::optional<std::string_view>(objectType.AsFast<sema::TypePackViewType>()->packName) :
            std::nullopt;
        const auto indexBinding = tryEvaluatePackIndexBinding(
            node.index,
            variableDeclarationsBySymbol_,
            expectedPackName,
            SymbolicPackNameMode::Direct);
        if (objectType && indexBinding.has_value())
        {
            auto buildTemplateIndexExpr = [&](const ParsedPackElementBinding& binding, std::string_view sizeExpr) -> std::string
            {
                if (binding.kind == PackElementBindingKind::FromEnd)
                    return common::formatString("({} - {})", sizeExpr, binding.value);

                return std::to_string(binding.value);
            };

            if (objectType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = objectType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = node.object ? node.object->referencedSymbol.Lock() : nullptr;
                const bool isRuntimePackValue =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                     (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable);
                const std::string packValueName = isRuntimePackValue
                    ? sanitizeCppIdentifier(packValueSymbol->name)
                    : packTypeName;
                const std::string sizeExpr = isRuntimePackValue
                    ? common::formatString("sizeof...({})", packValueName)
                    : common::formatString("wio::meta::TypePackView<{}...>::size", packTypeName);
                emit(common::formatString(
                    "std::get<{}>(std::forward_as_tuple({}...))",
                    buildTemplateIndexExpr(*indexBinding, sizeExpr),
                    packValueName
                ));
                return;
            }

            if (objectType->kind() == sema::TypeKind::ValuePackView || objectType->kind() == sema::TypeKind::PackStorage)
            {
                std::string sizeExpr;
                if (objectType->kind() == sema::TypeKind::ValuePackView)
                {
                    auto packViewType = objectType.AsFast<sema::ValuePackViewType>();
                    sizeExpr = packViewType->elementTypes.empty()
                        ? common::formatString("wio::meta::ValuePackView<{}...>::size", packViewType->packName)
                        : std::to_string(packViewType->elementTypes.size());
                }
                else
                {
                    auto storageType = objectType.AsFast<sema::PackStorageType>();
                    sizeExpr = storageType->elementTypes.empty()
                        ? common::formatString("wio::meta::PackStorage<{}...>::size", storageType->packName)
                        : std::to_string(storageType->elementTypes.size());
                }

                emit("(");
                node.object->accept(*this);
                emit(common::formatString(").template Get<{}>()", buildTemplateIndexExpr(*indexBinding, sizeExpr)));
                return;
            }
        }

        auto emitIndexableReceiver = [&]()
        {
            const std::size_t derefCount = getAutoReadableReferenceDepth(node.object ? node.object->refType.Lock() : nullptr);
            for (std::size_t i = 0; i < derefCount; ++i)
                emit("*(");

            node.object->accept(*this);

            for (std::size_t i = 0; i < derefCount; ++i)
                emit(")");
        };

        emit("wio::intrinsics::Index(");
        emitIndexableReceiver();
        emit(", ");
        node.index->accept(*this);
        emit(")");
    }

    bool CppGenerator::emitIntrinsicMemberAccess(MemberAccessExpression& node)
    {
        if (node.intrinsicMember == IntrinsicMember::None)
            return false;

        Ref<sema::Type> receiverType = unwrapAliasType(node.object ? node.object->refType.Lock() : nullptr);
        while (receiverType && receiverType->kind() == sema::TypeKind::Reference)
            receiverType = unwrapAliasType(receiverType.AsFast<sema::ReferenceType>()->referredType);

        auto emitPackReceiverArrayExpr = [&]()
        {
            if (!receiverType)
                return;

            if (receiverType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = receiverType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = node.object ? node.object->referencedSymbol.Lock() : nullptr;
                const bool isRuntimePackValue =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                    (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable);

                if (isRuntimePackValue)
                {
                    const std::string packValueName = sanitizeCppIdentifier(packValueSymbol->name);
                    emit(common::formatString("wio::meta::ValuePackView<{}...>({}...)", packTypeName, packValueName));
                }
                else
                {
                    emit(common::formatString("wio::meta::TypePackView<{}...>{{}}", packTypeName));
                }
                return;
            }

            if (receiverType->kind() == sema::TypeKind::TypePackView)
            {
                auto typePackView = receiverType.AsFast<sema::TypePackViewType>();
                if (!typePackView->elementTypes.empty())
                {
                    emit("wio::meta::TypePackView<");
                    for (size_t i = 0; i < typePackView->elementTypes.size(); ++i)
                    {
                        emit(toCppType(typePackView->elementTypes[i]));
                        if (i + 1 < typePackView->elementTypes.size())
                            emit(", ");
                    }
                    emit(">{}");
                }
                else
                {
                    emit(common::formatString("wio::meta::TypePackView<{}...>{{}}", typePackView->packName));
                }
                return;
            }

            if (receiverType->kind() == sema::TypeKind::ValuePackView)
            {
                node.object->accept(*this);
                return;
            }

            if (receiverType->kind() == sema::TypeKind::PackStorage)
            {
                emit("(");
                node.object->accept(*this);
                emit(").AsView()");
            }
        };

            if (node.intrinsicMember == IntrinsicMember::PackSize)
        {
            if (!receiverType)
                return false;

            if (receiverType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = receiverType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = node.object ? node.object->referencedSymbol.Lock() : nullptr;
                const bool isRuntimePackValue =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                    (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable);
                const std::string packSizeTarget = isRuntimePackValue
                    ? sanitizeCppIdentifier(packValueSymbol->name)
                    : packTypeName;
                emit(common::formatString("sizeof...({})", packSizeTarget));
                return true;
            }

            emit("(");
            emitPackReceiverArrayExpr();
            emit(").Size()");
            return true;
        }

        if (node.intrinsicMember == IntrinsicMember::PackArray)
        {
            emitPackReceiverArrayExpr();
            return true;
        }

        auto functionType = node.refType.Lock();
        if (!functionType || functionType->kind() != sema::TypeKind::Function)
            return false;

        std::size_t referenceDepth = 0;
        receiverType = node.object->refType.Lock();
        while (receiverType && receiverType->kind() == sema::TypeKind::Alias)
            receiverType = receiverType.AsFast<sema::AliasType>()->aliasedType;
        while (receiverType && receiverType->kind() == sema::TypeKind::Reference)
        {
            ++referenceDepth;
            receiverType = receiverType.AsFast<sema::ReferenceType>()->referredType;
            while (receiverType && receiverType->kind() == sema::TypeKind::Alias)
                receiverType = receiverType.AsFast<sema::AliasType>()->aliasedType;
        }

        auto emitReceiver = [&]()
        {
            if (referenceDepth == 0)
            {
                node.object->accept(*this);
                return;
            }

            emit("(");
            for (std::size_t i = 0; i < referenceDepth; ++i)
                emit("*");
            emit("(");
            node.object->accept(*this);
            emit("))");
        };

        auto emitArgumentName = [&](std::size_t index)
        {
            emit("_wio_arg" + std::to_string(index));
        };

        auto intrinsicFunctionType = functionType.AsFast<sema::FunctionType>();

        if (node.intrinsicMember == IntrinsicMember::TaskPoll ||
            node.intrinsicMember == IntrinsicMember::TaskWithin)
        {
            Ref<sema::Type> resolvedTaskType = unwrapAliasTypeForCodegen(receiverType);
            if (!resolvedTaskType || resolvedTaskType->kind() != sema::TypeKind::AsyncTask)
                return false;
            Ref<sema::Type> valueType = resolvedTaskType.AsFast<sema::AsyncTaskType>()->valueType;
            const bool isVoidTask = valueType && valueType->isVoid();

            emit("([&](");
            for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
            {
                emit(toCppType(intrinsicFunctionType->paramTypes[i]));
                emit(" _wio_arg" + std::to_string(i));
                if (i + 1 < intrinsicFunctionType->paramTypes.size())
                    emit(", ");
            }
            emit(") -> ");
            emit(toCppType(intrinsicFunctionType->returnType));
            emit(" { return ");

            auto& typeContext = Compiler::get().getTypeContext();
            Ref<sema::Type> declarationTaskType = resolvedTaskType;
            std::string functionName;
            std::vector<Ref<sema::Type>> declarationParams;
            if (node.intrinsicMember == IntrinsicMember::TaskPoll)
            {
                functionName = "Poll";
                if (!isVoidTask)
                {
                    Ref<sema::Type> genericT = typeContext.getOrCreateGenericParameterType("T");
                    declarationTaskType = typeContext.getOrCreateAsyncTaskType(genericT);
                }
                declarationParams = {declarationTaskType};
            }
            else
            {
                functionName = isVoidTask ? "TimeoutCompleted" : "TimeoutOption";
                if (!isVoidTask)
                {
                    Ref<sema::Type> genericT = typeContext.getOrCreateGenericParameterType("T");
                    declarationTaskType = typeContext.getOrCreateAsyncTaskType(genericT);
                }
                declarationParams = {declarationTaskType, typeContext.getU64()};
            }

            emit(Mangler::mangleFunction(functionName, declarationParams, "std_async"));
            if (!isVoidTask)
            {
                emit("<");
                emit(toCppType(valueType));
                emit(">");
            }
            emit("(");
            emitReceiver();
            for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
            {
                emit(", ");
                emitArgumentName(i);
            }
            emit("); })");
            return true;
        }

        const std::string_view helperName = getIntrinsicHelperName(node.intrinsicMember);
        if (helperName.empty())
            return false;

        emit("([&](");
        for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
        {
            emit(toCppType(intrinsicFunctionType->paramTypes[i]));
            emit(" _wio_arg" + std::to_string(i));
            if (i + 1 < intrinsicFunctionType->paramTypes.size())
                emit(", ");
        }
        emit(") -> ");
        emit(toCppType(intrinsicFunctionType->returnType));
        emit(" { ");

        Ref<sema::Type> resolvedReturnType = unwrapAliasTypeForCodegen(intrinsicFunctionType->returnType);
        const bool returnsStdOption = resolvedReturnType &&
            resolvedReturnType->kind() == sema::TypeKind::Struct &&
            resolvedReturnType.AsFast<sema::StructType>()->name == "Option" &&
            resolvedReturnType.AsFast<sema::StructType>()->scopePath == "std";

        if (returnsStdOption &&
            (node.intrinsicMember == IntrinsicMember::ArrayGet ||
             node.intrinsicMember == IntrinsicMember::DictGet ||
             node.intrinsicMember == IntrinsicMember::StringGet))
        {
            Ref<sema::Type> valueType = nullptr;
            Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);
            if (resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Array)
                valueType = resolvedReceiverType.AsFast<sema::ArrayType>()->elementType;
            else if (resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Dictionary)
                valueType = resolvedReceiverType.AsFast<sema::DictionaryType>()->valueType;
            else if (resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Primitive &&
                     resolvedReceiverType.AsFast<sema::PrimitiveType>()->name == "string")
                valueType = Compiler::get().getTypeContext().getChar();
            if (!valueType)
                return false;

            const std::string cppValueType = toCppType(valueType);
            if (node.intrinsicMember == IntrinsicMember::StringGet)
            {
                emit("wio::String _wio_values = wio::String(");
                emitReceiver();
                emit(")");
            }
            else
            {
                emit("auto&& _wio_values = ");
                emitReceiver();
            }
            if (node.intrinsicMember == IntrinsicMember::DictGet)
            {
                emit("; auto _wio_it = _wio_values.find(_wio_arg0); ");
                emit("if (_wio_it != _wio_values.end()) return _WF_std_Some_T<" + cppValueType + ">(_wio_it->second); ");
            }
            else
            {
                emit("; if (_wio_arg0 < _wio_values.size()) return _WF_std_Some_T<" + cppValueType + ">(wio::intrinsics::Index(_wio_values, _wio_arg0)); ");
            }
            emit("return _WF_std_None<" + cppValueType + ">(); })");
            return true;
        }

        if (node.intrinsicMember == IntrinsicMember::StringGet)
        {
            emit("return wio::intrinsics::Index(wio::String(");
            emitReceiver();
            emit("), _wio_arg0); })");
            return true;
        }

        if (intrinsicFunctionType->returnType && !intrinsicFunctionType->returnType->isVoid())
            emit("return ");

        emit("wio::intrinsics::");
        emit(std::string(helperName));
        emit("(");
        emitReceiver();

        for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
        {
            emit(", ");
            emitArgumentName(i);
        }
        emit(")");
        emit("; })");
        return true;
    }

    void CppGenerator::visit(MemberAccessExpression& node)
    {
        if (emitIntrinsicMemberAccess(node))
            return;

        auto getSelectedMemberSymbol = [&]() -> Ref<sema::Symbol>
        {
            if (auto selectedMember = node.referencedSymbol.Lock())
                return selectedMember;

            return node.member->referencedSymbol.Lock();
        };

        auto emitMemberName = [&]()
        {
            if (auto memberSym = getSelectedMemberSymbol();
                memberSym && memberSym->kind == sema::SymbolKind::Function)
            {
                auto selectedType = node.refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                    selectedType = node.member->refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                    selectedType = memberSym->type;

                auto funcType = selectedType ? selectedType.AsFast<sema::FunctionType>() : nullptr;
                if (!funcType)
                    funcType = memberSym->type.AsFast<sema::FunctionType>();

                if (funcType &&
                    memberSym->type && memberSym->type->kind() == sema::TypeKind::Function)
                {
                    auto declarationFunctionType = memberSym->type.AsFast<sema::FunctionType>();
                    if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                    {
                        funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            declarationFunctionType->returnType,
                            getLeadingParameterTypes(declarationFunctionType, funcType->paramTypes.size())
                        ).AsFast<sema::FunctionType>();
                    }
                }

                emit(Mangler::mangleFunction(memberSym->name, funcType->paramTypes));
                return;
            }

            if (auto memberSym = getSelectedMemberSymbol();
                memberSym && memberSym->kind == sema::SymbolKind::FunctionGroup)
            {
                auto selectedType = node.refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                    selectedType = node.member->refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                {
                    if (!memberSym->overloads.empty())
                        selectedType = memberSym->overloads.front()->type;
                }

                std::string scopePath = memberSym->scopePath;
                if (scopePath.empty() && !memberSym->overloads.empty())
                    scopePath = memberSym->overloads.front()->scopePath;

                if (selectedType && selectedType->kind() == sema::TypeKind::Function)
                {
                    auto funcType = selectedType.AsFast<sema::FunctionType>();
                    if (!memberSym->overloads.empty() &&
                        memberSym->overloads.front()->type &&
                        memberSym->overloads.front()->type->kind() == sema::TypeKind::Function)
                    {
                        auto declarationFunctionType = memberSym->overloads.front()->type.AsFast<sema::FunctionType>();
                        if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                        {
                            funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                                declarationFunctionType->returnType,
                                getLeadingParameterTypes(declarationFunctionType, funcType->paramTypes.size())
                            ).AsFast<sema::FunctionType>();
                        }
                    }
                    emit(Mangler::mangleFunction(memberSym->name, funcType->paramTypes));
                    return;
                }
            }

            if (auto memberSym = node.member->referencedSymbol.Lock();
                memberSym && memberSym->kind == sema::SymbolKind::Struct)
            {
                emit(Mangler::mangleStruct(memberSym->name, memberSym->scopePath));
                return;
            }

            node.member->accept(*this);
        };

        if (auto objSym = node.object->referencedSymbol.Lock();
            objSym && objSym->kind == sema::SymbolKind::Namespace)
        {
            if (auto memberSym = getSelectedMemberSymbol())
            {
                if (memberSym->kind == sema::SymbolKind::Function)
                {
                    auto funcType = node.refType.Lock();
                    if (!funcType || funcType->kind() != sema::TypeKind::Function)
                        funcType = node.member->refType.Lock();
                    if (!funcType || funcType->kind() != sema::TypeKind::Function)
                        funcType = memberSym->type;

                    if (funcType && funcType->kind() == sema::TypeKind::Function)
                    {
                        auto mangledFunctionType = funcType.AsFast<sema::FunctionType>();
                        if (memberSym->type &&
                            memberSym->type->kind() == sema::TypeKind::Function)
                        {
                            auto declarationFunctionType = memberSym->type.AsFast<sema::FunctionType>();
                            if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                            {
                                mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                                    declarationFunctionType->returnType,
                                    getLeadingParameterTypes(declarationFunctionType, mangledFunctionType->paramTypes.size())
                                ).AsFast<sema::FunctionType>();
                            }
                        }

                        emit(Mangler::mangleFunction(memberSym->name, mangledFunctionType->paramTypes, memberSym->scopePath));
                        return;
                    }
                }
                else if (memberSym->kind == sema::SymbolKind::FunctionGroup)
                {
                    auto selectedType = node.refType.Lock();
                    if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                        selectedType = node.member->refType.Lock();
                    if ((!selectedType || selectedType->kind() != sema::TypeKind::Function) && !memberSym->overloads.empty())
                        selectedType = memberSym->overloads.front()->type;

                    std::string scopePath = memberSym->scopePath;
                    if (scopePath.empty() && !memberSym->overloads.empty())
                        scopePath = memberSym->overloads.front()->scopePath;

                    if (selectedType && selectedType->kind() == sema::TypeKind::Function)
                    {
                        auto mangledFunctionType = selectedType.AsFast<sema::FunctionType>();
                        if (!memberSym->overloads.empty() &&
                            memberSym->overloads.front()->type &&
                            memberSym->overloads.front()->type->kind() == sema::TypeKind::Function)
                        {
                            auto declarationFunctionType = memberSym->overloads.front()->type.AsFast<sema::FunctionType>();
                            if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                            {
                                mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                                    declarationFunctionType->returnType,
                                    getLeadingParameterTypes(declarationFunctionType, mangledFunctionType->paramTypes.size())
                                ).AsFast<sema::FunctionType>();
                            }
                        }

                        emit(Mangler::mangleFunction(memberSym->name, mangledFunctionType->paramTypes, scopePath));
                        return;
                    }
                }
                else if (memberSym->kind == sema::SymbolKind::Struct)
                {
                    emit(Mangler::mangleStruct(memberSym->name, memberSym->scopePath));
                    return;
                }
            }

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

                emit(mangleStructTypeName(baseStruct) + "::");
                emitMemberName();
                return;
            }
        }

        std::string op = ".";
        std::size_t referenceDepth = 0;
        Ref<sema::Type> terminalType = nullptr;

        if (auto objSym = node.object->referencedSymbol.Lock())
        {
            if (objSym->kind == sema::SymbolKind::Namespace ||
                objSym->kind == sema::SymbolKind::Struct ||
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

                while (baseType && baseType->kind() == sema::TypeKind::Reference)
                {
                    referenceDepth++;
                    baseType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    while (baseType && baseType->kind() == sema::TypeKind::Alias)
                        baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                }

                terminalType = baseType;

                if (referenceDepth > 0)
                {
                    op = "->";
                }
                else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = terminalType.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        op = "->";
                }
            }
        }

        auto emitObjectWithReferenceDepth = [&](std::size_t extraReferenceDepth)
        {
            if (op == "::")
            {
                node.object->accept(*this);
                return;
            }

            if (extraReferenceDepth <= 1)
            {
                node.object->accept(*this);
                return;
            }

            emit("(");
            for (std::size_t i = 0; i < extraReferenceDepth - 1; ++i)
                emit("*");
            emit("(");
            node.object->accept(*this);
            emit("))");
        };

        emitObjectWithReferenceDepth(referenceDepth);

        emit(op);

        emitMemberName();
    }

    void CppGenerator::visit(FunctionCallExpression& node)
    {
        if (node.propagateResult)
        {
            const auto payloadType = node.refType.Lock();
            const std::string payloadCppType = payloadType ? toCppType(payloadType) : "void";

            emit("([&]() -> " + payloadCppType + " ");
            emitLine("{");
            indent();

            const bool previousPropagateResult = node.propagateResult;
            node.propagateResult = false;

            EMIT_TABS();
            emit("auto _wio_result = ");
            visit(node);
            emit(";\n");

            node.propagateResult = previousPropagateResult;

            emitLine("if (_wio_result->_WF_IsError())");
            indent();
            emitLine("throw _wio_result->_WF_ErrorValue();");
            dedent();

            EMIT_TABS();
            emit("return _wio_result->_WF_Unwrap();\n");

            dedent();
            EMIT_TABS();
            emit("}())");
            return;
        }

        auto beginResultUnwrap = [&]()
        {
            if (node.unwrapResult)
                emit("(");
        };

        auto endResultUnwrap = [&]()
        {
            if (node.unwrapResult)
                emit(")->_WF_Unwrap()");
        };

        if (const auto* memberAccess = node.callee ? node.callee->as<MemberAccessExpression>() : nullptr;
            memberAccess && memberAccess->intrinsicMember == IntrinsicMember::PackToStaticArray)
        {
            Ref<sema::Type> receiverType = unwrapAliasType(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
            while (receiverType && receiverType->kind() == sema::TypeKind::Reference)
                receiverType = unwrapAliasType(receiverType.AsFast<sema::ReferenceType>()->referredType);

            if (node.explicitTypeArguments.empty())
            {
                emit("/* invalid PackToStaticArray call */");
                return;
            }

            if (receiverType && receiverType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = receiverType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = memberAccess->object ? memberAccess->object->referencedSymbol.Lock() : nullptr;
                const std::string packValueName =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                    (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable)
                        ? sanitizeCppIdentifier(packValueSymbol->name)
                        : packTypeName;
                beginResultUnwrap();
                emit("wio::meta::PackToStaticArray<");
                node.explicitTypeArguments.front()->accept(*this);
                emit(common::formatString(">({}...)", packValueName));
                endResultUnwrap();
                return;
            }

            beginResultUnwrap();
            emit("(");
            memberAccess->object->accept(*this);
            emit(").template ToStaticArray<");
            node.explicitTypeArguments.front()->accept(*this);
            emit(">()");
            endResultUnwrap();
            return;
        }

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isCallOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member
                    ? node.arguments.size()
                    : node.arguments.size() + 1u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                Ref<sema::Type> receiverType = receiver ? receiver->refType.Lock() : nullptr;
                Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);

                bool usePointerAccess =
                    resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Reference;
                emit("(");
                receiver->accept(*this);
                emit(")");
                emit(usePointerAccess ? "->" : ".");
            };

            beginResultUnwrap();
            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.callee);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    if (operatorFunctionType && i < operatorFunctionType->paramTypes.size())
                        emitExpressionWithExpectedType(node.arguments[i], operatorFunctionType->paramTypes[i], true);
                    else
                        emitReadableExpression(node.arguments[i]);

                    if (i + 1 < node.arguments.size())
                        emit(", ");
                }
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.callee, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.callee);

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    emit(", ");
                    if (operatorFunctionType && i + 1 < operatorFunctionType->paramTypes.size())
                        emitExpressionWithExpectedType(node.arguments[i], operatorFunctionType->paramTypes[i + 1], true);
                    else
                        emitReadableExpression(node.arguments[i]);
                }
                emit(")");
            }
            endResultUnwrap();
            return;
        }

        auto calleeType = node.callee->refType.Lock();
        auto calleeSym = node.callee->referencedSymbol.Lock();

        Ref<sema::Type> resolvedConstructorType = unwrapAliasType(calleeType);
        if (resolvedConstructorType && resolvedConstructorType->kind() == sema::TypeKind::Struct)
        {
            auto structType = resolvedConstructorType.AsFast<sema::StructType>();
            beginResultUnwrap();
            if (structType->isObject)
            {
                emit("wio::runtime::Ref<" + mangleStructTypeName(structType) + ">::Create(");
            }
            else
            {
                emit(mangleStructTypeName(structType) + "(");
            }

            for (size_t i = 0; i < node.arguments.size(); ++i)
            {
                node.arguments[i]->accept(*this);
                if (i < node.arguments.size() - 1)
                    emit(", ");
            }
            emit(")");
            endResultUnwrap();
            return;
        }

        Ref<sema::FunctionType> functionType = nullptr;
        if (calleeType && calleeType->kind() == sema::TypeKind::Function)
        {
            functionType = calleeType.AsFast<sema::FunctionType>();
        }
        else if (calleeSym && calleeSym->type && calleeSym->type->kind() == sema::TypeKind::Function)
        {
            functionType = calleeSym->type.AsFast<sema::FunctionType>();
        }

        bool shouldEmitDirectFunctionCallee = false;
        if (calleeSym &&
            (calleeSym->kind == sema::SymbolKind::Function || calleeSym->kind == sema::SymbolKind::FunctionGroup) &&
            functionType)
        {
            if (node.callee->is<Identifier>())
            {
                shouldEmitDirectFunctionCallee = true;
            }
            else if (const auto* memberAccess = node.callee->as<MemberAccessExpression>())
            {
                if (auto objectSym = memberAccess->object->referencedSymbol.Lock();
                    objectSym && objectSym->kind == sema::SymbolKind::Namespace)
                {
                    shouldEmitDirectFunctionCallee = true;
                }
                else if (calleeSym->flags.get_isExtension())
                {
                    shouldEmitDirectFunctionCallee = true;
                }
            }
        }

        beginResultUnwrap();
        if (shouldEmitDirectFunctionCallee)
        {
            std::string scopePath = calleeSym->scopePath;
            if (scopePath.empty() && calleeSym->kind == sema::SymbolKind::FunctionGroup && !calleeSym->overloads.empty())
                scopePath = calleeSym->overloads.front()->scopePath;
            if (isStructMemberFunctionSymbol(calleeSym))
                scopePath.clear();

            Ref<sema::Symbol> extensionImplementation =
                calleeSym->extensionImplementation ? calleeSym->extensionImplementation : calleeSym;
            Ref<sema::FunctionType> mangledFunctionType =
                calleeSym->flags.get_isExtension() && extensionImplementation->type &&
                extensionImplementation->type->kind() == sema::TypeKind::Function
                    ? extensionImplementation->type.AsFast<sema::FunctionType>()
                    : functionType;
            if (calleeSym->flags.get_isExtension() && mangledFunctionType && !mangledFunctionType->hasParameterPack)
            {
                const size_t emittedParameterCount = std::min(
                    mangledFunctionType->paramTypes.size(),
                    node.arguments.size() + 1);
                mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                    mangledFunctionType->returnType,
                    getLeadingParameterTypes(mangledFunctionType, emittedParameterCount)
                ).AsFast<sema::FunctionType>();
            }
            if (!calleeSym->genericParameterNames.empty())
            {
                Ref<sema::Type> declarationType = calleeSym->flags.get_isExtension()
                    ? extensionImplementation->type
                    : calleeSym->type;
                if ((!declarationType || declarationType->kind() != sema::TypeKind::Function) &&
                    calleeSym->kind == sema::SymbolKind::FunctionGroup && !calleeSym->overloads.empty())
                {
                    declarationType = calleeSym->overloads.front()->type;
                }

                if (declarationType && declarationType->kind() == sema::TypeKind::Function)
                {
                    auto declarationFunctionType = declarationType.AsFast<sema::FunctionType>();
                    if (declarationFunctionType->hasParameterPack)
                    {
                        mangledFunctionType = declarationFunctionType;
                    }
                    else
                    {
                        const size_t visibleArgumentOffset = calleeSym->flags.get_isExtension() ? 1 : 0;
                        mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            declarationFunctionType->returnType,
                            getLeadingParameterTypes(
                                declarationFunctionType,
                                node.arguments.size() + visibleArgumentOffset)
                        ).AsFast<sema::FunctionType>();
                    }
                }
            }

            if (calleeSym->flags.get_isDerived())
            {
                emit("wio::runtime::Ref<" + calleeSym->derivedProcessorCppType + ">::Create()->");
                emit(Mangler::mangleFunction(
                    extensionImplementation->name,
                    mangledFunctionType->paramTypes));
            }
            else
            {
                emit(Mangler::mangleFunction(calleeSym->name, mangledFunctionType->paramTypes, scopePath));
            }
        }
        else
        {
            node.callee->accept(*this);
        }
        if (!node.resolvedGenericArguments.empty())
        {
            emit("<");
            for (size_t i = 0; i < node.resolvedGenericArguments.size(); ++i)
            {
                emit(toCppType(node.resolvedGenericArguments[i].Lock()));
                if (i + 1 < node.resolvedGenericArguments.size())
                    emit(", ");
            }
            emit(">");
        }
        else if (!node.explicitTypeArguments.empty())
        {
            emit("<");
            for (size_t i = 0; i < node.explicitTypeArguments.size(); ++i)
            {
                node.explicitTypeArguments[i]->accept(*this);
                if (i < node.explicitTypeArguments.size() - 1)
                    emit(", ");
            }
            emit(">");
        }
        emit("(");
        bool emittedExtensionReceiver = false;
        if (calleeSym && calleeSym->flags.get_isExtension())
        {
            if (const auto* memberAccess = node.callee->as<MemberAccessExpression>())
            {
                auto implementation = calleeSym->extensionImplementation
                    ? calleeSym->extensionImplementation
                    : calleeSym;
                auto fullType = implementation->type.AsFast<sema::FunctionType>();
                Ref<sema::Type> receiverType =
                    fullType && !fullType->paramTypes.empty() ? fullType->paramTypes.front() : nullptr;
                emitExpressionWithExpectedType(memberAccess->object, receiverType, true);
                emittedExtensionReceiver = true;
            }
        }
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            if (emittedExtensionReceiver || i > 0)
                emit(", ");
            Ref<sema::Type> expectedType = nullptr;
            if (functionType && i < functionType->paramTypes.size())
                expectedType = functionType->paramTypes[i];

            if (node.arguments[i] && node.arguments[i]->is<PackExpansionExpression>())
                node.arguments[i]->accept(*this);
            else
                emitExpressionWithExpectedType(node.arguments[i], expectedType, false);
        }
        emit(")");
        endResultUnwrap();
    }

    void CppGenerator::visit(LambdaExpression& node)
    {
        auto functionType = node.refType.Lock().AsFast<sema::FunctionType>();
        emit("[=");
        if (currentClassIsObject_ && !currentClassName_.empty())
        {
            // C++ captures an implicit `this` as a raw pointer, even with a
            // value-default capture. Keep the Wio object alive for as long as
            // an escaping closure can evaluate `self`.
            emit(", this, _wio_lambda_self_guard = wio::runtime::Ref<" + currentClassName_ + ">(this)");
        }
        emit("](");

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            Ref<sema::Type> parameterType = nullptr;
            if (functionType && i < functionType->paramTypes.size())
                parameterType = functionType->paramTypes[i];
            else if (node.parameters[i].type)
                parameterType = node.parameters[i].type->refType.Lock();
            else if (node.parameters[i].name)
                parameterType = node.parameters[i].name->refType.Lock();

            if (parameterType && !parameterType->isUnknown())
                emit(toCppType(parameterType));
            else
                emit("auto");
            emit(" ");
            emit(node.parameters[i].name->token.value);
            if (i < node.parameters.size() - 1)
                emit(", ");
        }
        emit(") mutable");

        if (functionType && functionType->returnType && !functionType->returnType->isUnknown())
        {
            emit(" -> ");
            emit(toCppType(functionType->returnType));
        }

        emit(" {\n");
        indent();

        const auto enclosingPostProcessors = currentBehavioralPostProcessors_;
        const auto enclosingFinallyProcessors = currentBehavioralFinallyProcessors_;
        currentBehavioralPostProcessors_.clear();
        currentBehavioralFinallyProcessors_.clear();

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

        currentBehavioralPostProcessors_ = enclosingPostProcessors;
        currentBehavioralFinallyProcessors_ = enclosingFinallyProcessors;

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

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 0u : 1u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                Ref<sema::Type> receiverType = receiver ? receiver->refType.Lock() : nullptr;
                Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);

                bool usePointerAccess =
                    resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Reference;
                emit("(");
                receiver->accept(*this);
                emit(")");
                emit(usePointerAccess ? "->" : ".");
            };

            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.operand);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("())");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.operand, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.operand);
                emit(")");
            }
            return;
        }

        if (isAnyTypeForCodegen(srcType))
        {
            Ref<sema::Type> resolvedDestType = unwrapAliasTypeForCodegen(destType);
            if (isOpaqueTypeForCodegen(resolvedDestType))
            {
                emit("(");
                node.operand->accept(*this);
                emit(").AsOpaque()");
                return;
            }

            if (resolvedDestType && resolvedDestType->kind() == sema::TypeKind::Struct)
            {
                auto structType = resolvedDestType.AsFast<sema::StructType>();
                if (structType->isInterface)
                {
                    emit("(");
                    node.operand->accept(*this);
                    emit(").AsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">()");
                    return;
                }

                if (structType->isObject && !structType->isInterface)
                {
                    emit("(");
                    node.operand->accept(*this);
                    emit(").CastObject<" + mangleStructTypeName(structType) + ">()");
                    return;
                }
            }

            emit("(");
            node.operand->accept(*this);
            emit(").AsBoxed<" + toCppType(destType) + ">()");
            return;
        }

        Ref<sema::Type> resolvedSrcType = unwrapAliasTypeForCodegen(srcType);
        Ref<sema::Type> resolvedDestType = unwrapAliasTypeForCodegen(destType);
        const bool useNumericFit =
            resolvedSrcType &&
            resolvedSrcType->isNumeric() &&
            resolvedDestType &&
            (resolvedDestType->isNumeric() || resolvedDestType->kind() == sema::TypeKind::GenericParameter);

        if (useNumericFit)
        {
            std::string cppDestType = toCppType(destType);
            emit("wio::FitNumeric<" + cppDestType + ">(");
            emitReadableExpression(node.operand);
            emit(")");
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
        emit(currentExtensionMethod_ ? "_wio_self" : "this");
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
        std::string matchReturnType = "void";
        if (auto t = node.refType.Lock(); t)
        {
            producesValue = !t->isVoid() && !t->isUnknown();
            if (producesValue)
                matchReturnType = toCppType(t);
        }

        emit("[&]()");
        if (producesValue)
            emit(" -> " + matchReturnType);
        emit(" {\n");
        indent();
        EMIT_TABS();
        emit("auto _match_val = (");
        node.value->accept(*this);
        emit(");\n");

        bool first = true;
        for (auto& matchCase : node.cases)
        {
            EMIT_TABS();
            if (matchCase.matchValues.empty() && matchCase.variantName.empty()) // assumed
            {
                emit("else {\n");
            }
            else if (!matchCase.variantName.empty())
            {
                if (!first) emit("else ");
                emit("if (");
                if (matchCase.variantName == "__array")
                    emit("_match_val.size() == " + std::to_string(matchCase.bindings.size()));
                else if (matchCase.variantName == "Some")
                    emit("_match_val->_WF_IsSome()");
                else if (matchCase.variantName == "None")
                    emit("_match_val->_WF_IsNone()");
                else if (matchCase.variantName == "Ok")
                    emit("_match_val->_WF_IsOk()");
                else
                    emit("_match_val->_WF_IsError()");
                if (matchCase.guard)
                {
                    emit(" && [&]() { ");
                    for (size_t bindingIndex = 0; bindingIndex < matchCase.bindings.size(); ++bindingIndex)
                    {
                        auto& binding = matchCase.bindings[bindingIndex];
                        emit("auto " + sanitizeCppIdentifier(binding->token.value) + " = ");
                        if (matchCase.variantName == "__array")
                        {
                            emit("wio::intrinsics::Index(_match_val, " + std::to_string(bindingIndex) + "); ");
                        }
                        else
                        {
                            emit("_match_val->");
                            emit(matchCase.variantName == "Err" ? "_WF_ErrorValue(); " : "_WF_Value(); ");
                        }
                    }
                    emit("return ");
                    matchCase.guard->accept(*this);
                    emit("; }()");
                }
                emit(") {\n");
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
                if (matchCase.guard)
                {
                    emit(" && (");
                    matchCase.guard->accept(*this);
                    emit(")");
                }
                emit(") {\n");
            }
            first = false;

            indent();

            for (size_t bindingIndex = 0; bindingIndex < matchCase.bindings.size(); ++bindingIndex)
            {
                auto& binding = matchCase.bindings[bindingIndex];
                EMIT_TABS();
                emit("auto " + sanitizeCppIdentifier(binding->token.value) + " = ");
                if (matchCase.variantName == "__array")
                    emit("wio::intrinsics::Index(_match_val, " + std::to_string(bindingIndex) + ");\n");
                else
                {
                    emit("_match_val->");
                    emit(matchCase.variantName == "Err" ? "_WF_ErrorValue();\n" : "_WF_Value();\n");
                }
            }

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
