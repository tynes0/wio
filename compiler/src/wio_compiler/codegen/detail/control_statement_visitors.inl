// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

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
        emitSourceDirective(node.location());
        if (node.matchVar.isValid() && node.condition->is<BinaryExpression>())
        {
            auto binExpr = node.condition->as<BinaryExpression>();
            auto targetType = binExpr->right->refType.Lock();

            if (isAnyTypeForCodegen(binExpr->left->refType.Lock()))
            {
                Ref<sema::Type> resolvedTargetType = unwrapAliasTypeForCodegen(targetType);
                const std::string targetCppType = toCppType(targetType);

                EMIT_TABS();
                emit("if (wio::runtime::Any _wio_any_match = ");
                binExpr->left->accept(*this);
                emit("; ");

                if (resolvedTargetType && resolvedTargetType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedTargetType.AsFast<sema::StructType>();
                    if (structType->isInterface)
                    {
                        emit("_wio_any_match.IsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">()");
                        emit(")\n");
                        emitLine("{");
                        indent();
                        emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">();");
                    }
                    else if (structType->isObject && !structType->isInterface)
                    {
                        emit("_wio_any_match.CanCastObject<" + mangleStructTypeName(structType) + ">()");
                        emit(")\n");
                        emitLine("{");
                        indent();
                        emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.CastObject<" + mangleStructTypeName(structType) + ">();");
                    }
                    else
                    {
                        emit("_wio_any_match.IsBoxed<" + targetCppType + ">()");
                        emit(")\n");
                        emitLine("{");
                        indent();
                        emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsBoxed<" + targetCppType + ">();");
                    }
                }
                else if (isOpaqueTypeForCodegen(resolvedTargetType))
                {
                    emit("_wio_any_match.IsOpaque()");
                    emit(")\n");
                    emitLine("{");
                    indent();
                    emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsOpaque();");
                }
                else
                {
                    emit("_wio_any_match.IsBoxed<" + targetCppType + ">()");
                    emit(")\n");
                    emitLine("{");
                    indent();
                    emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsBoxed<" + targetCppType + ">();");
                }

                if (node.thenBranch) node.thenBranch->accept(*this);

                dedent();
                emitLine("}");

                if (node.elseBranch)
                {
                    if (node.elseBranch->is<IfStatement>())
                        emitLine("else");
                    else
                        emit("else ");
                    node.elseBranch->accept(*this);
                }
                return;
            }

            auto typeSym = binExpr->right->referencedSymbol.Lock();
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
                if (node.elseBranch->is<IfStatement>())
                    emitLine("else");
                else
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
            if (node.elseBranch->is<IfStatement>())
                emitLine("else");
            else
                emit("else ");
            node.elseBranch->accept(*this);
        }
    }

    void CppGenerator::visit(WhileStatement& node)
    {
        emitSourceDirective(node.location());
        EMIT_TABS();
        buffer_ << "while (";
        node.condition->accept(*this);
        buffer_ << ")\n";

        node.body->accept(*this);
    }

    void CppGenerator::visit(ForInStatement& node)
    {
        emitSourceDirective(node.location());
        auto emitLoopBodyStatements = [&](const NodePtr<Statement>& body)
        {
            if (!body)
                return;

            if (body->is<BlockStatement>())
            {
                auto block = body->as<BlockStatement>();
                for (auto& stmt : block->statements)
                    stmt->accept(*this);
                return;
            }

            body->accept(*this);
        };

        auto unwrapAliasType = [](Ref<sema::Type> type)
        {
            while (type && type->kind() == sema::TypeKind::Alias)
                type = type.AsFast<sema::AliasType>()->aliasedType;
            return type;
        };

        auto buildReferenceInitializer = [&](const Ref<sema::Type>& bindingType, const std::string& sourceExpr)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(bindingType);
            if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                return sourceExpr;

            auto refType = resolvedType.AsFast<sema::ReferenceType>();
            Ref<sema::Type> referredType = unwrapAliasType(refType->referredType);

            if (referredType && referredType->kind() == sema::TypeKind::Struct)
            {
                auto structType = referredType.AsFast<sema::StructType>();
                if (structType->isObject || structType->isInterface)
                    return sourceExpr;
            }

            return "&(" + sourceExpr + ")";
        };

        auto getBindingMode = [&](size_t index) -> ForBindingMode
        {
            if (index < node.bindingModes.size())
                return node.bindingModes[index];

            return ForBindingMode::ValueImmutable;
        };

        if (node.iterable->is<RangeExpression>())
        {
            const auto* range = node.iterable->as<RangeExpression>();
            const auto& binding = node.bindings.front();
            const ForBindingMode bindingMode = getBindingMode(0);
            auto bindingSym = binding->referencedSymbol.Lock();
            Ref<sema::Type> bindingType = (bindingSym && bindingSym->type) ? bindingSym->type : binding->refType.Lock();

            const std::string loopVarName = bindingMode == ForBindingMode::ValueImmutable
                ? "_wio_range_value"
                : binding->token.value;

            emitLine("{");
            indent();

            EMIT_TABS();
            emit("auto _wio_range_start = (");
            range->start->accept(*this);
            emit(");\n");

            EMIT_TABS();
            emit("auto _wio_range_end = (");
            range->end->accept(*this);
            emit(");\n");

            EMIT_TABS();
            emit("auto _wio_range_step = static_cast<" + toCppType(bindingType) + ">(");
            if (node.step)
                node.step->accept(*this);
            else
                emit("1");
            emit(");\n");

            emitLine("if (_wio_range_step == 0) throw wio::runtime::RuntimeException(\"Range step cannot be zero.\");");

            emitLine("if (_wio_range_step > 0)");
            emitLine("{");
            indent();

            EMIT_TABS();
            emit("for (" + toCppType(bindingType) + " " + loopVarName + " = _wio_range_start; ");
            emit(loopVarName);
            emit(range->isInclusive ? " <= " : " < ");
            emit("_wio_range_end; " + loopVarName + " = static_cast<" + toCppType(bindingType) + ">(" + loopVarName + " + _wio_range_step))\n");

            emitLine("{");
            indent();

            if (bindingMode == ForBindingMode::ValueImmutable)
                emitLine("const auto& " + binding->token.value + " = " + loopVarName + ";");

            emitLoopBodyStatements(node.body);
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");

            emitLine("else");
            emitLine("{");
            indent();

            EMIT_TABS();
            emit("for (" + toCppType(bindingType) + " " + loopVarName + " = _wio_range_start; ");
            emit(loopVarName);
            emit(range->isInclusive ? " >= " : " > ");
            emit("_wio_range_end; " + loopVarName + " = static_cast<" + toCppType(bindingType) + ">(" + loopVarName + " + _wio_range_step))\n");

            emitLine("{");
            indent();

            if (bindingMode == ForBindingMode::ValueImmutable)
                emitLine("const auto& " + binding->token.value + " = " + loopVarName + ";");

            emitLoopBodyStatements(node.body);
            dedent();
            emitLine("}");

            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            return;
        }

        auto emitBoundDeclaration = [&](size_t index, const std::string& sourceExpr)
        {
            const auto& binding = node.bindings[index];
            const ForBindingMode bindingMode = getBindingMode(index);
            auto bindingSym = binding->referencedSymbol.Lock();
            Ref<sema::Type> bindingType = (bindingSym && bindingSym->type) ? bindingSym->type : binding->refType.Lock();

            switch (bindingMode)
            {
            case ForBindingMode::ValueMutable:
                emitLine("auto " + binding->token.value + " = " + sourceExpr + ";");
                return;
            case ForBindingMode::ValueImmutable:
                emitLine("const auto& " + binding->token.value + " = " + sourceExpr + ";");
                return;
            case ForBindingMode::ReferenceMutable:
            case ForBindingMode::ReferenceView:
                  emitLine(toCppType(bindingType) + " " + binding->token.value + " = " + buildReferenceInitializer(bindingType, sourceExpr) + ";");
                  return;
              }
          };

        auto isArrayIterable = [&]() -> bool
        {
            Ref<sema::Type> iterableType = node.iterable ? node.iterable->refType.Lock() : nullptr;
            iterableType = unwrapAliasType(iterableType);

            while (iterableType && iterableType->kind() == sema::TypeKind::Reference)
                iterableType = unwrapAliasType(iterableType.AsFast<sema::ReferenceType>()->referredType);

            return iterableType && iterableType->kind() == sema::TypeKind::Array;
        };

        const bool explicitIndexBinding = !node.bindingAccessors.empty() && node.bindingAccessors.front() == "__index__";
        if (isArrayIterable() && (explicitIndexBinding || node.step != nullptr))
        {
            emitLine("{");
            indent();

            EMIT_TABS();
            emit("auto&& _wio_range = (");
            node.iterable->accept(*this);
            emit(");\n");

            EMIT_TABS();
            emit("auto _wio_array_step = static_cast<std::int64_t>(");
            if (node.step)
                node.step->accept(*this);
            else
                emit("1");
            emit(");\n");

            emitLine("if (_wio_array_step <= 0) throw wio::runtime::RuntimeException(\"Array step must be positive.\");");

            EMIT_TABS();
            emit("for (std::size_t _wio_index = 0; _wio_index < _wio_range.size(); _wio_index += static_cast<std::size_t>(_wio_array_step))\n");
            emitLine("{");
            indent();

            if (explicitIndexBinding)
            {
                const auto& indexBinding = node.bindings.front();
                const ForBindingMode indexBindingMode = getBindingMode(0);
                auto indexBindingSym = indexBinding->referencedSymbol.Lock();
                Ref<sema::Type> indexBindingType = (indexBindingSym && indexBindingSym->type) ? indexBindingSym->type : indexBinding->refType.Lock();
                std::string indexCastExpr = "static_cast<" + toCppType(indexBindingType) + ">(_wio_index)";

                switch (indexBindingMode)
                {
                case ForBindingMode::ValueMutable:
                    emitLine("auto " + indexBinding->token.value + " = " + indexCastExpr + ";");
                    break;
                case ForBindingMode::ValueImmutable:
                    emitLine("const auto " + indexBinding->token.value + " = " + indexCastExpr + ";");
                    break;
                case ForBindingMode::ReferenceMutable:
                case ForBindingMode::ReferenceView:
                    break;
                }
            }

            const std::string itemExpr = "_wio_range[_wio_index]";
            if (node.bindingAccessors.empty())
            {
                emitBoundDeclaration(0, itemExpr);
            }
            else
            {
                const size_t bindingStartIndex = explicitIndexBinding ? 1 : 0;
                if (explicitIndexBinding && node.bindingAccessors.size() >= 2 && node.bindingAccessors[1] == "__value__")
                {
                    emitBoundDeclaration(1, itemExpr);
                }
                else
                {
                    for (size_t i = bindingStartIndex; i < node.bindings.size() && i < node.bindingAccessors.size(); ++i)
                    {
                        if (node.bindingAccessors[i] == "__index__")
                            continue;

                        if (node.bindingAccessors[i] == "__value__")
                            emitBoundDeclaration(i, itemExpr);
                        else
                            emitBoundDeclaration(i, itemExpr + "." + node.bindingAccessors[i]);
                    }
                }
            }

            emitLoopBodyStatements(node.body);
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            return;
        }

        const bool hasMutableReferenceBinding = std::ranges::any_of(node.bindingModes, [](ForBindingMode mode)
        {
            return mode == ForBindingMode::ReferenceMutable;
        });

        const bool needsPrelude = node.bindings.size() > 1 ||
            getBindingMode(0) == ForBindingMode::ReferenceMutable ||
            getBindingMode(0) == ForBindingMode::ReferenceView;

        std::string rangeVarName = needsPrelude ? "_wio_loop_item" : node.bindings.front()->token.value;
        std::string rangeDecl = "const auto& " + rangeVarName;
        if (!needsPrelude)
        {
            switch (getBindingMode(0))
            {
            case ForBindingMode::ValueMutable:
                rangeDecl = "auto " + rangeVarName;
                break;
            case ForBindingMode::ValueImmutable:
                rangeDecl = "const auto& " + rangeVarName;
                break;
            case ForBindingMode::ReferenceMutable:
                rangeDecl = "auto& " + rangeVarName;
                break;
            case ForBindingMode::ReferenceView:
                rangeDecl = "const auto& " + rangeVarName;
                break;
            }
        }
        else if (hasMutableReferenceBinding)
        {
            rangeDecl = "auto& " + rangeVarName;
        }

        EMIT_TABS();
        buffer_ << "for (" << rangeDecl << " : ";
        node.iterable->accept(*this);
        buffer_ << ")\n";

        emitLine("{");
        indent();

        if (needsPrelude)
        {
            if (node.bindingAccessors.empty())
            {
                emitBoundDeclaration(0, rangeVarName);
            }
            else
            {
                for (size_t i = 0; i < node.bindings.size() && i < node.bindingAccessors.size(); ++i)
                {
                    emitBoundDeclaration(i, rangeVarName + "." + node.bindingAccessors[i]);
                }
            }
        }

        emitLoopBodyStatements(node.body);
        dedent();
        emitLine("}");
    }

    void CppGenerator::visit(CForStatement& node)
    {
        emitSourceDirective(node.location());
        emitLine("{");
        indent();

        if (node.initializer)
            node.initializer->accept(*this);

        EMIT_TABS();
        emit("for (; ");
        if (node.condition)
            node.condition->accept(*this);
        else
            emit("true");
        emit("; ");
        if (node.increment)
            node.increment->accept(*this);
        emit(")\n");

        if (node.body)
            node.body->accept(*this);

        dedent();
        emitLine("}");
    }

    void CppGenerator::visit(BreakStatement& node)
    {
        emitSourceDirective(node.location());
        emitLine("break;");
    }

    void CppGenerator::visit(ContinueStatement& node)
    {
        emitSourceDirective(node.location());
        emitLine("continue;");
    }

    void CppGenerator::visit(ReturnStatement& node)
    {
        emitSourceDirective(node.location());
        const bool hasBehavioralExit = !currentBehavioralPostProcessors_.empty() ||
                                       !currentBehavioralFinallyProcessors_.empty();
        if (node.value && hasBehavioralExit)
        {
            const std::string resultName = "_wio_attribute_return_" + std::to_string(behavioralReturnCounter_++);
            EMIT_TABS();
            buffer_ << "auto " << resultName << " = ";
            emitExpressionWithExpectedType(node.value, currentFunctionReturnType_, false);
            buffer_ << ";\n";
            for (std::string invocation : currentBehavioralPostProcessors_)
            {
                if (const size_t marker = invocation.find("{result}"); marker != std::string::npos)
                    invocation.replace(marker, std::string("{result}").size(), resultName);
                emitLine(invocation);
            }
            for (const std::string& invocation : currentBehavioralFinallyProcessors_)
                emitLine(invocation);
            EMIT_TABS();
            buffer_ << (currentFunctionIsAsync_ ? "co_return " : "return ") << resultName << ";\n";
            return;
        }

        for (const std::string& invocation : currentBehavioralPostProcessors_)
            emitLine(invocation);
        for (const std::string& invocation : currentBehavioralFinallyProcessors_)
            emitLine(invocation);

        EMIT_TABS();
        buffer_ << (currentFunctionIsAsync_ ? "co_return" : "return");
        if (node.value)
        {
            buffer_ << " ";
            emitExpressionWithExpectedType(node.value, currentFunctionReturnType_, false);
        }
        buffer_ << ";\n";
    }

    void CppGenerator::visit(UseStatement& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(UsingAttributeStatement& node)
    {
        if (node.body)
            node.body->accept(*this);
    }
