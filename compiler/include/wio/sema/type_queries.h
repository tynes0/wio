#pragma once

#include "wio/sema/type.h"

#include <cstddef>
#include <string_view>

namespace wio::sema::type_queries
{
    Ref<Type> unwrapAliasType(Ref<Type> type);
    bool isPrimitiveNamed(const Ref<Type>& type, std::string_view name);
    bool isStdLibraryScopePath(std::string_view scopePath);
    Ref<StructType> getStdValueStructType(const Ref<Type>& type, std::string_view expectedName);
    bool isSdkValueBridgeType(const Ref<Type>& type);
    bool shouldAutoReadReferenceType(const Ref<Type>& type);
    Ref<Type> getAutoReadableType(const Ref<Type>& type);
    std::size_t getAutoReadableReferenceDepth(const Ref<Type>& type);
}
