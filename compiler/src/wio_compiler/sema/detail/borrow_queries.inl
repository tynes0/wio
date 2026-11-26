// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        bool sameNativeBorrowCellType(const Ref<Type>& actual, const Ref<Type>& expected, unsigned depth = 0)
        {
            const auto lhs = unwrapAliasType(actual);
            const auto rhs = unwrapAliasType(expected);
            if (!lhs || !rhs || depth >= 64 || lhs->kind() != rhs->kind())
                return false;
            if (lhs == rhs)
                return true;
            const auto same = [&](const auto& a, const auto& b)
            {
                return sameNativeBorrowCellType(a, b, depth + 1);
            };
            switch (lhs->kind())
            {
            case TypeKind::Primitive:
                return lhs.AsFast<PrimitiveType>()->name == rhs.AsFast<PrimitiveType>()->name;
            case TypeKind::Nullable:
                return same(lhs.AsFast<NullableType>()->valueType, rhs.AsFast<NullableType>()->valueType);
            case TypeKind::Reference:
            {
                const auto a = lhs.AsFast<ReferenceType>();
                const auto b = rhs.AsFast<ReferenceType>();
                return a->isMutable == b->isMutable && same(a->referredType, b->referredType);
            }
            case TypeKind::Array:
            {
                const auto a = lhs.AsFast<ArrayType>();
                const auto b = rhs.AsFast<ArrayType>();
                return a->arrayKind == b->arrayKind && a->size == b->size &&
                       ((!a->extentType && !b->extentType) || same(a->extentType, b->extentType)) &&
                       same(a->elementType, b->elementType);
            }
            case TypeKind::Dictionary:
            {
                const auto a = lhs.AsFast<DictionaryType>();
                const auto b = rhs.AsFast<DictionaryType>();
                return a->isOrdered == b->isOrdered && same(a->keyType, b->keyType) && same(a->valueType, b->valueType);
            }
            case TypeKind::Struct:
            {
                const auto a = lhs.AsFast<StructType>();
                const auto b = rhs.AsFast<StructType>();
                if (a->name != b->name || a->scopePath != b->scopePath ||
                    a->genericArguments.size() != b->genericArguments.size())
                    return false;
                for (size_t i = 0; i < a->genericArguments.size(); ++i)
                    if (!same(a->genericArguments[i], b->genericArguments[i]))
                        return false;
                return true;
            }
            default:
                return lhs->toString() == rhs->toString() && lhs->toCppString() == rhs->toCppString();
            }
        }

        Ref<Type> nativeBorrowStorageType(const NodePtr<Expression>& expression, unsigned depth = 0)
        {
            // Flow narrowing changes the readable value, not the physical
            // caller cell. Its declared nullable shape must still be visible.
            if (!expression || depth >= 64)
                return nullptr;
            if (const auto* borrow = expression->as<RefExpression>())
                return nativeBorrowStorageType(borrow->operand, depth + 1);
            Ref<Type> type = expression->refType.Lock();
            if (expression->is<Identifier>() || expression->is<MemberAccessExpression>())
            {
                if (const auto symbol = expression->referencedSymbol.Lock(); symbol && isVariableLikeSymbol(symbol))
                    type = symbol->type;
            }
            else if (const auto* element = expression->as<ArrayAccessExpression>())
            {
                const auto owner = nativeBorrowStorageType(element->object, depth + 1);
                if (owner && owner->kind() == TypeKind::Array)
                    type = owner.AsFast<ArrayType>()->elementType;
                else if (owner && owner->kind() == TypeKind::Dictionary)
                    type = owner.AsFast<DictionaryType>()->valueType;
            }
            type = unwrapAliasType(type);
            if (type && type->kind() == TypeKind::Reference)
                type = unwrapAliasType(type.AsFast<ReferenceType>()->referredType);
            return type;
        }

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
