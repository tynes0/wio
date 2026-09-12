// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        std::optional<std::string> getSdkDynamicBridgeCppType(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return std::nullopt;

            if (resolvedType->kind() == sema::TypeKind::Primitive)
            {
                const std::string& name = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (name == "void" || name == "object" || name == "any" || name == "opaque")
                    return std::nullopt;
                if (name == "text")
                    return "std::string";
                if (name == "uchar") return "wio::sdk::WioUChar";
                if (name == "byte") return "wio::sdk::WioByte";
                if (name == "u8") return "wio::sdk::WioU8";
                if (name == "i64") return "wio::sdk::WioI64";
                if (name == "u64") return "wio::sdk::WioU64";
                if (name == "isize") return "wio::sdk::WioISize";
                if (name == "usize") return "wio::sdk::WioUSize";
                return toCppType(resolvedType);
            }

            if (resolvedType->kind() == sema::TypeKind::Array)
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                auto elementType = arrayType ? getSdkDynamicBridgeCppType(arrayType->elementType) : std::nullopt;
                if (!arrayType || !elementType.has_value())
                    return std::nullopt;

                if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static)
                {
                    return "std::array<" + *elementType + ", " + std::to_string(arrayType->size) + ">";
                }
                return "std::vector<" + *elementType + ">";
            }

            if (resolvedType->kind() == sema::TypeKind::Dictionary)
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                auto keyType = dictionaryType ? getSdkDynamicBridgeCppType(dictionaryType->keyType) : std::nullopt;
                auto valueType = dictionaryType ? getSdkDynamicBridgeCppType(dictionaryType->valueType) : std::nullopt;
                if (!dictionaryType || !keyType.has_value() || !valueType.has_value())
                    return std::nullopt;

                return std::string(dictionaryType->isOrdered ? "std::map<" : "std::unordered_map<") +
                    *keyType + ", " + *valueType + ">";
            }

            if (auto optionType = getStdValueStructType(resolvedType, "Option");
                optionType && optionType->genericArguments.size() == 1)
            {
                auto valueType = getSdkDynamicBridgeCppType(optionType->genericArguments.front());
                if (!valueType.has_value())
                    return std::nullopt;
                return "wio::sdk::WioOption<" + *valueType + ">";
            }

            if (getStdValueStructType(resolvedType, "ResultUnit"))
                return "wio::sdk::WioUnit";

            if (getStdValueStructType(resolvedType, "Span"))
                return "wio::sdk::WioSpanRange";

            if (getStdValueStructType(resolvedType, "ByteBuffer"))
                return "wio::sdk::WioByteBuffer";

            if (auto tupleType = getStdValueStructType(resolvedType, "Tuple"); tupleType)
            {
                std::string hostType = "wio::sdk::WioTuple<";
                for (std::size_t index = 0; index < tupleType->genericArguments.size(); ++index)
                {
                    auto argumentType = getSdkDynamicBridgeCppType(tupleType->genericArguments[index]);
                    if (!argumentType.has_value())
                        return std::nullopt;
                    if (index > 0)
                        hostType += ", ";
                    hostType += *argumentType;
                }
                hostType += ">";
                return hostType;
            }

            if (auto resultType = getStdValueStructType(resolvedType, "Result");
                resultType && resultType->genericArguments.size() == 1)
            {
                auto valueType = getSdkDynamicBridgeCppType(resultType->genericArguments.front());
                if (!valueType.has_value())
                    return std::nullopt;
                return "wio::sdk::WioResult<" + *valueType + ">";
            }

            auto queueType = getStdValueStructType(resolvedType, "Queue");
            auto unorderedSetType = getStdValueStructType(resolvedType, "UnorderedSet");
            auto orderedSetType = getStdValueStructType(resolvedType, "OrderedSet");
            auto sequenceType = queueType ? queueType : (unorderedSetType ? unorderedSetType : orderedSetType);
            if (sequenceType && sequenceType->genericArguments.size() == 1)
            {
                auto valueType = getSdkDynamicBridgeCppType(sequenceType->genericArguments.front());
                if (!valueType.has_value())
                    return std::nullopt;

                const std::string wrapperName = queueType
                    ? "wio::sdk::WioQueue<"
                    : (unorderedSetType ? "wio::sdk::WioUnorderedSet<" : "wio::sdk::WioOrderedSet<");
                return wrapperName + *valueType + ">";
            }

            return std::nullopt;
        }

        std::optional<std::string> makeSdkDynamicToHostExpression(const std::string& expression,
                                                                  const Ref<sema::Type>& type,
                                                                  const std::size_t depth = 0)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return std::nullopt;

            if (resolvedType->kind() == sema::TypeKind::Primitive)
            {
                const std::string& name = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (name == "text")
                    return "(" + expression + ").Utf8()";
                if (name == "uchar") return "wio::sdk::WioUChar(" + expression + ")";
                if (name == "byte") return "wio::sdk::WioByte(" + expression + ")";
                if (name == "u8") return "wio::sdk::WioU8(" + expression + ")";
                if (name == "i64") return "wio::sdk::WioI64(" + expression + ")";
                if (name == "u64") return "wio::sdk::WioU64(" + expression + ")";
                if (name == "isize") return "wio::sdk::WioISize(" + expression + ")";
                if (name == "usize") return "wio::sdk::WioUSize(" + expression + ")";
                if (getSdkDynamicBridgeCppType(resolvedType).has_value())
                    return expression;
                return std::nullopt;
            }

            if (resolvedType->kind() == sema::TypeKind::Array)
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                if (!arrayType || !hostType.has_value())
                    return std::nullopt;

                const std::string sourceName = "_wio_array_" + std::to_string(depth);
                const std::string itemExpression = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? sourceName + "[_wio_index]"
                    : "_wio_item_" + std::to_string(depth);
                auto convertedItem = makeSdkDynamicToHostExpression(itemExpression, arrayType->elementType, depth + 1);
                if (!convertedItem.has_value())
                    return std::nullopt;

                if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static)
                {
                    std::string initializers;
                    for (std::size_t index = 0; index < arrayType->size; ++index)
                    {
                        auto indexedValue = makeSdkDynamicToHostExpression(
                            sourceName + "[" + std::to_string(index) + "]",
                            arrayType->elementType,
                            depth + 1
                        );
                        if (!indexedValue.has_value())
                            return std::nullopt;
                        if (!initializers.empty())
                            initializers += ", ";
                        initializers += *indexedValue;
                    }

                    return common::formatString(
                        "([&]() -> {} {{ const auto& {} = {}; return {}{{{}}}; }}())",
                        *hostType,
                        sourceName,
                        expression,
                        *hostType,
                        initializers
                    );
                }

                const std::string itemName = "_wio_item_" + std::to_string(depth);
                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "_wio_output.reserve({}.size()); for (const auto& {} : {}) "
                    "_wio_output.push_back({}); return _wio_output; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    *hostType,
                    sourceName,
                    itemName,
                    sourceName,
                    *convertedItem
                );
            }

            if (resolvedType->kind() == sema::TypeKind::Dictionary)
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                if (!dictionaryType || !hostType.has_value())
                    return std::nullopt;

                const std::string sourceName = "_wio_dictionary_" + std::to_string(depth);
                const std::string keyName = "_wio_key_" + std::to_string(depth);
                const std::string valueName = "_wio_value_" + std::to_string(depth);
                auto convertedKey = makeSdkDynamicToHostExpression(keyName, dictionaryType->keyType, depth + 1);
                auto convertedValue = makeSdkDynamicToHostExpression(valueName, dictionaryType->valueType, depth + 1);
                if (!convertedKey.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "for (const auto& [{}, {}] : {}) _wio_output.emplace({}, {}); "
                    "return _wio_output; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    *hostType,
                    keyName,
                    valueName,
                    sourceName,
                    *convertedKey,
                    *convertedValue
                );
            }

            if (auto optionType = getStdValueStructType(resolvedType, "Option");
                optionType && optionType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = optionType->genericArguments.front();
                auto hostType = getSdkDynamicBridgeCppType(valueType);
                const std::string variableName = "_wio_option_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicToHostExpression(
                    variableName + "->_WF_Value()",
                    valueType,
                    depth + 1
                );
                if (!hostType.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                return common::formatString(
                    "([&]() -> wio::sdk::WioOption<{}> {{ auto {} = {}; "
                    "if (!{} || !{}->_WF_IsSome()) return wio::sdk::WioOption<{}>::none(); "
                    "return wio::sdk::WioOption<{}>::some({}); }}())",
                    *hostType,
                    variableName,
                    expression,
                    variableName,
                    variableName,
                    *hostType,
                    *hostType,
                    *convertedValue
                );
            }

            if (getStdValueStructType(resolvedType, "ResultUnit"))
                return "wio::sdk::WioUnit{}";

            if (getStdValueStructType(resolvedType, "Span"))
            {
                return "wio::sdk::WioSpanRange{static_cast<std::size_t>((" + expression +
                    ").start), static_cast<std::size_t>((" + expression + ").count)}";
            }

            if (getStdValueStructType(resolvedType, "ByteBuffer"))
            {
                const std::string sourceName = "_wio_byte_buffer_" + std::to_string(depth);
                return common::formatString(
                    "([&]() -> wio::sdk::WioByteBuffer {{ auto {} = {}; wio::sdk::WioByteBuffer _wio_output; "
                    "if (!{}) return _wio_output; _wio_output.reserve(static_cast<std::size_t>({}->_WF_Capacity())); "
                    "for (const auto _wio_byte : {}->_WF_ToArray()) "
                    "_wio_output.write(static_cast<std::byte>(_wio_byte)); "
                    "(void)_wio_output.seek(static_cast<std::size_t>({}->_WF_Position())); return _wio_output; }}())",
                    sourceName,
                    expression,
                    sourceName,
                    sourceName,
                    sourceName,
                    sourceName
                );
            }

            if (auto tupleType = getStdValueStructType(resolvedType, "Tuple"); tupleType)
            {
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                if (!hostType.has_value())
                    return std::nullopt;

                const std::string sourceName = "_wio_tuple_" + std::to_string(depth);
                std::string values;
                for (std::size_t index = 0; index < tupleType->genericArguments.size(); ++index)
                {
                    auto convertedValue = makeSdkDynamicToHostExpression(
                        sourceName + "->data.template Get<" + std::to_string(index) + ">()",
                        tupleType->genericArguments[index],
                        depth + 1
                    );
                    if (!convertedValue.has_value())
                        return std::nullopt;
                    if (!values.empty())
                        values += ", ";
                    values += *convertedValue;
                }

                return common::formatString(
                    "([&]() -> {} {{ auto {} = {}; if (!{}) throw std::runtime_error(\"Wio SDK tuple field is null.\"); "
                    "return {}{{{}}}; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    sourceName,
                    *hostType,
                    values
                );
            }

            if (auto resultType = getStdValueStructType(resolvedType, "Result");
                resultType && resultType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = resultType->genericArguments.front();
                auto hostType = getSdkDynamicBridgeCppType(valueType);
                const std::string variableName = "_wio_result_" + std::to_string(depth);
                const std::string errorName = "_wio_error_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicToHostExpression(
                    variableName + "->_WF_Value()",
                    valueType,
                    depth + 1
                );
                if (!hostType.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                return common::formatString(
                    "([&]() -> wio::sdk::WioResult<{}> {{ auto {} = {}; "
                    "if (!{} || {}->_WF_IsError()) {{ auto {} = {}->_WF_ErrorValue(); "
                    "return wio::sdk::WioResult<{}>::error(wio::sdk::WioResultError{{"
                    "static_cast<wio::sdk::WioResultDomain>(static_cast<std::int32_t>({}.domain)), "
                    "{}.code, {}.nativeCode, {}.message}}); }} "
                    "return wio::sdk::WioResult<{}>::ok({}); }}())",
                    *hostType,
                    variableName,
                    expression,
                    variableName,
                    variableName,
                    errorName,
                    variableName,
                    *hostType,
                    errorName,
                    errorName,
                    errorName,
                    errorName,
                    *hostType,
                    *convertedValue
                );
            }

            auto queueType = getStdValueStructType(resolvedType, "Queue");
            auto unorderedSetType = getStdValueStructType(resolvedType, "UnorderedSet");
            auto orderedSetType = getStdValueStructType(resolvedType, "OrderedSet");
            auto sequenceType = queueType ? queueType : (unorderedSetType ? unorderedSetType : orderedSetType);
            if (sequenceType && sequenceType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = sequenceType->genericArguments.front();
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                const std::string sourceName = "_wio_sequence_" + std::to_string(depth);
                const std::string itemName = "_wio_sequence_item_" + std::to_string(depth);
                auto convertedItem = makeSdkDynamicToHostExpression(itemName, valueType, depth + 1);
                if (!hostType.has_value() || !convertedItem.has_value())
                    return std::nullopt;

                const std::string appendExpression = queueType
                    ? "_wio_output.push(" + *convertedItem + ");"
                    : "(void)_wio_output.add(" + *convertedItem + ");";
                return common::formatString(
                    "([&]() -> {} {{ auto {} = {}; {} _wio_output; if (!{}) return _wio_output; "
                    "for (const auto& {} : {}->_WF_ToArray()) {{ {} }} return _wio_output; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    *hostType,
                    sourceName,
                    itemName,
                    sourceName,
                    appendExpression
                );
            }

            return std::nullopt;
        }

        std::optional<std::string> makeSdkDynamicFromHostExpression(const std::string& expression,
                                                                    const Ref<sema::Type>& type,
                                                                    const std::size_t depth = 0)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return std::nullopt;

            if (resolvedType->kind() == sema::TypeKind::Primitive)
            {
                const std::string& name = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (name == "text")
                    return "wio::runtime::Text::FromUtf8(" + expression + ")";
                if (name == "uchar" || name == "byte" || name == "u8" ||
                    name == "i64" || name == "u64" || name == "isize" || name == "usize")
                {
                    return "static_cast<" + toCppType(resolvedType) + ">((" + expression + ").value())";
                }
                if (getSdkDynamicBridgeCppType(resolvedType).has_value())
                    return expression;
                return std::nullopt;
            }

            if (resolvedType->kind() == sema::TypeKind::Array)
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                if (!arrayType)
                    return std::nullopt;

                const std::string sourceName = "_wio_host_array_" + std::to_string(depth);
                const std::string itemName = "_wio_host_item_" + std::to_string(depth);
                const std::string itemExpression = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? sourceName + "[_wio_index]"
                    : itemName;
                auto convertedItem = makeSdkDynamicFromHostExpression(itemExpression, arrayType->elementType, depth + 1);
                if (!convertedItem.has_value())
                    return std::nullopt;

                const std::string cppType = toCppType(resolvedType);
                if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static)
                {
                    return common::formatString(
                        "([&]() -> {} {{ const auto& {} = {}; {} _wio_output{{}}; "
                        "for (std::size_t _wio_index = 0; _wio_index < {}; ++_wio_index) "
                        "_wio_output[_wio_index] = {}; return _wio_output; }}())",
                        cppType,
                        sourceName,
                        expression,
                        cppType,
                        arrayType->size,
                        *convertedItem
                    );
                }

                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "_wio_output.reserve({}.size()); for (const auto& {} : {}) "
                    "_wio_output.push_back({}); return _wio_output; }}())",
                    cppType,
                    sourceName,
                    expression,
                    cppType,
                    sourceName,
                    itemName,
                    sourceName,
                    *convertedItem
                );
            }

            if (resolvedType->kind() == sema::TypeKind::Dictionary)
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                if (!dictionaryType)
                    return std::nullopt;

                const std::string sourceName = "_wio_host_dictionary_" + std::to_string(depth);
                const std::string keyName = "_wio_host_key_" + std::to_string(depth);
                const std::string valueName = "_wio_host_value_" + std::to_string(depth);
                auto convertedKey = makeSdkDynamicFromHostExpression(keyName, dictionaryType->keyType, depth + 1);
                auto convertedValue = makeSdkDynamicFromHostExpression(valueName, dictionaryType->valueType, depth + 1);
                if (!convertedKey.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                const std::string cppType = toCppType(resolvedType);
                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "for (const auto& [{}, {}] : {}) _wio_output.emplace({}, {}); "
                    "return _wio_output; }}())",
                    cppType,
                    sourceName,
                    expression,
                    cppType,
                    keyName,
                    valueName,
                    sourceName,
                    *convertedKey,
                    *convertedValue
                );
            }

            if (auto optionType = getStdValueStructType(resolvedType, "Option");
                optionType && optionType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = optionType->genericArguments.front();
                const std::string variableName = "_wio_option_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicFromHostExpression(
                    variableName + ".value()",
                    valueType,
                    depth + 1
                );
                if (!convertedValue.has_value())
                    return std::nullopt;

                const std::string cppStructType = mangleStructTypeName(optionType);
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "if ({}.is_none()) return wio::runtime::Ref<{}>::Create(); "
                    "return wio::runtime::Ref<{}>::Create({}); }}())",
                    cppStructType,
                    variableName,
                    expression,
                    variableName,
                    cppStructType,
                    cppStructType,
                    *convertedValue
                );
            }

            if (auto unitType = getStdValueStructType(resolvedType, "ResultUnit"); unitType)
                return mangleStructTypeName(unitType) + "()";

            if (auto spanType = getStdValueStructType(resolvedType, "Span"); spanType)
            {
                return mangleStructTypeName(spanType) + "(static_cast<std::size_t>((" + expression +
                    ").start()), static_cast<std::size_t>((" + expression + ").count()))";
            }

            if (auto byteBufferType = getStdValueStructType(resolvedType, "ByteBuffer"); byteBufferType)
            {
                const std::string sourceName = "_wio_host_byte_buffer_" + std::to_string(depth);
                const std::string cppStructType = mangleStructTypeName(byteBufferType);
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; wio::DArray<uint8_t> _wio_bytes; "
                    "_wio_bytes.reserve({}.count()); for (const auto _wio_byte : {}.data()) "
                    "_wio_bytes.push_back(std::to_integer<uint8_t>(_wio_byte)); "
                    "auto _wio_output = wio::runtime::Ref<{}>::Create(_wio_bytes); "
                    "_wio_output->_WF_Reserve_usize(static_cast<std::size_t>({}.capacity())); "
                    "(void)_wio_output->_WF_Seek_usize(static_cast<std::size_t>({}.position())); return _wio_output; }}())",
                    cppStructType,
                    sourceName,
                    expression,
                    sourceName,
                    sourceName,
                    cppStructType,
                    sourceName,
                    sourceName
                );
            }

            if (auto tupleType = getStdValueStructType(resolvedType, "Tuple"); tupleType)
            {
                const std::string sourceName = "_wio_host_tuple_" + std::to_string(depth);
                std::string values;
                for (std::size_t index = 0; index < tupleType->genericArguments.size(); ++index)
                {
                    auto convertedValue = makeSdkDynamicFromHostExpression(
                        "std::get<" + std::to_string(index) + ">(" + sourceName + ")",
                        tupleType->genericArguments[index],
                        depth + 1
                    );
                    if (!convertedValue.has_value())
                        return std::nullopt;
                    if (!values.empty())
                        values += ", ";
                    values += *convertedValue;
                }

                const std::string cppStructType = mangleStructTypeName(tupleType);
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "return wio::runtime::Ref<{}>::Create({}); }}())",
                    cppStructType,
                    sourceName,
                    expression,
                    cppStructType,
                    values
                );
            }

            if (auto resultType = getStdValueStructType(resolvedType, "Result");
                resultType && resultType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = resultType->genericArguments.front();
                const std::string variableName = "_wio_result_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicFromHostExpression(
                    variableName + ".value()",
                    valueType,
                    depth + 1
                );
                if (!convertedValue.has_value())
                    return std::nullopt;

                const std::string cppStructType = mangleStructTypeName(resultType);
                const std::string resultErrorType = Mangler::mangleStruct("ResultError", "std");
                const std::string resultDomainType = Mangler::mangleStruct("ResultDomain", "std");
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "if ({}.is_error()) {{ const auto& _wio_host_error = {}.error_value(); "
                    "{} _wio_error{{}}; "
                    "_wio_error.domain = static_cast<{}>(static_cast<std::int32_t>(_wio_host_error.domain)); "
                    "_wio_error.code = _wio_host_error.code; _wio_error.nativeCode = _wio_host_error.native_code; "
                    "_wio_error.message = _wio_host_error.message; "
                    "return wio::runtime::Ref<{}>::Create(_wio_error); }} "
                    "return wio::runtime::Ref<{}>::Create({}); }}())",
                    cppStructType,
                    variableName,
                    expression,
                    variableName,
                    variableName,
                    resultErrorType,
                    resultDomainType,
                    cppStructType,
                    cppStructType,
                    *convertedValue
                );
            }

            auto queueType = getStdValueStructType(resolvedType, "Queue");
            auto unorderedSetType = getStdValueStructType(resolvedType, "UnorderedSet");
            auto orderedSetType = getStdValueStructType(resolvedType, "OrderedSet");
            auto sequenceType = queueType ? queueType : (unorderedSetType ? unorderedSetType : orderedSetType);
            if (sequenceType && sequenceType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = sequenceType->genericArguments.front();
                const std::string sourceName = "_wio_host_sequence_" + std::to_string(depth);
                const std::string itemName = "_wio_host_sequence_item_" + std::to_string(depth);
                auto convertedItem = makeSdkDynamicFromHostExpression(itemName, valueType, depth + 1);
                if (!convertedItem.has_value())
                    return std::nullopt;

                const std::string cppStructType = mangleStructTypeName(sequenceType);
                const std::string valuesExpression = queueType
                    ? sourceName + ".to_array()"
                    : sourceName + ".values()";
                const std::string appendExpression = queueType
                    ? "_wio_output->_WF_Enqueue_T(" + *convertedItem + ");"
                    : "(void)_wio_output->_WF_Add_T(" + *convertedItem + ");";
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "auto _wio_output = wio::runtime::Ref<{}>::Create(); "
                    "for (const auto& {} : {}) {{ {} }} return _wio_output; }}())",
                    cppStructType,
                    sourceName,
                    expression,
                    cppStructType,
                    itemName,
                    valuesExpression,
                    appendExpression
                );
            }

            return std::nullopt;
        }
