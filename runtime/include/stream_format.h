#pragma once

#include <sstream>
#include <string>

namespace wio::runtime::detail
{
    inline void AppendStreamBuffer(std::string& target, const std::ostringstream& stream)
    {
        const auto bytes = stream.view();
        // A native facet can expose its stream to an allocation hook. Keep the
        // old owning snapshot if append can grow the target, rather than carry
        // a live buffer view through that reentrant allocation boundary.
        if (bytes.size() <= target.capacity() - target.size())
            target.append(bytes);
        else
            target.append(stream.str());
    }
} // namespace wio::runtime::detail
