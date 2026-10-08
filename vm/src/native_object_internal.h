#pragma once

#include "native_pod_internal.h"

namespace wio::vm::detail
{
    [[nodiscard]] bool mergeNativeObjects(const bytecode::Module& module, const WioNativeObjectApi& api,
                                          NativePodLayouts& layouts, std::string& error);
} // namespace wio::vm::detail
