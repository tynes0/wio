#pragma once

#include "wio/common/location.h"

#include <utility>

namespace wio::wir
{
    struct SourceSpan
    {
        common::Location begin = common::Location::invalid();
        common::Location end = common::Location::invalid();

        [[nodiscard]] bool hasSourceContext() const
        {
            return begin.hasSourceContext();
        }

        [[nodiscard]] static SourceSpan at(common::Location location)
        {
            return SourceSpan{.begin = location, .end = std::move(location)};
        }
    };
}
