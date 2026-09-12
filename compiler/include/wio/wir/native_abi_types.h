#pragma once
#include "wio/wir/type.h"

namespace wio::wir
{
    NativeMarshallingKind nativeAbiMarshalling(const TypeTable& types, TypeId id);
    std::string nativeAbiTypeKey(const TypeTable& types, TypeId id);
    void refreshNativeAbiValue(const TypeTable& types, NativeAbiValue& value, bool result);
} // namespace wio::wir
