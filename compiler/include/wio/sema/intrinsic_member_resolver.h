#pragma once

#include <optional>
#include <string_view>

#include "type_context.h"
#include "wio/ast/ast.h"

namespace wio::sema
{
    struct IntrinsicMemberResolution
    {
        IntrinsicMember member = IntrinsicMember::None;
        Ref<Type> memberType = nullptr;
        bool requiresMutableReceiver = false;
    };

    std::optional<IntrinsicMemberResolution> resolveIntrinsicMember(
        TypeContext& typeContext,
        const Ref<Type>& ownerType,
        std::string_view memberName
    );

    bool isDynamicArrayOnlyIntrinsicMemberName(std::string_view memberName) noexcept;
    bool isMutatingIntrinsicMember(IntrinsicMember member) noexcept;
}
