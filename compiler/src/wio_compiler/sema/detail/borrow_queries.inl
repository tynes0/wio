// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        std::string formatAccessContextType(const Ref<Type>& type)
        {
            if (!type)
                return "<non-object context>";

            return type->toString();
        }

        bool isAddressableRefOperand(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (expression->is<ArrayAccessExpression>())
            {
                auto* arrayAccess = expression->as<ArrayAccessExpression>();
                if (isTextType(arrayAccess->object ? arrayAccess->object->refType.Lock() : nullptr))
                    return false;
                if (arrayAccess->operatorDispatchKind == OperatorDispatchKind::None)
                    return true;

                Ref<Type> indexedType = unwrapAliasType(arrayAccess->refType.Lock());
                return indexedType && indexedType->kind() == TypeKind::Reference;
            }

            if (expression->is<FunctionCallExpression>())
            {
                Ref<Type> callType = unwrapAliasType(expression->refType.Lock());
                return callType && callType->kind() == TypeKind::Reference;
            }

            if (expression->is<Identifier>() || expression->is<MemberAccessExpression>())
                return isVariableLikeSymbol(expression->referencedSymbol.Lock());

            return false;
        }

        BorrowOrigin classifyBorrowOrigin(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return BorrowOrigin::Temporary;

            if (expression->borrowOrigin != BorrowOrigin::None)
                return expression->borrowOrigin;

            if (expression->is<SelfExpression>() || expression->is<SuperExpression>())
                return BorrowOrigin::Caller;

            // A selected member or indexed element is owned by its receiver,
            // not by the field symbol recorded on the access expression.
            if (const auto* memberAccess = expression->as<MemberAccessExpression>())
                return classifyBorrowOrigin(memberAccess->object);
            if (const auto* arrayAccess = expression->as<ArrayAccessExpression>())
                return classifyBorrowOrigin(arrayAccess->object);
            if (const auto* refExpression = expression->as<RefExpression>())
                return classifyBorrowOrigin(refExpression->operand);

            if (const auto symbol = expression->referencedSymbol.Lock(); symbol)
            {
                if (symbol->flags.get_isGlobal())
                    return BorrowOrigin::Static;
                if (symbol->kind == SymbolKind::Parameter && symbol->type && symbol->type->kind() == TypeKind::Reference)
                    return BorrowOrigin::Caller;
                if (symbol->kind == SymbolKind::Variable || symbol->kind == SymbolKind::Parameter)
                    return BorrowOrigin::Local;
            }

            return BorrowOrigin::Temporary;
        }

        std::string_view borrowOriginName(BorrowOrigin origin)
        {
            switch (origin)
            {
            case BorrowOrigin::Static: return "static storage";
            case BorrowOrigin::Caller: return "caller-owned storage";
            case BorrowOrigin::Local: return "a local value";
            case BorrowOrigin::Temporary: return "a temporary value";
            case BorrowOrigin::None: return "an untracked value";
            }
            return "an unknown value";
        }
