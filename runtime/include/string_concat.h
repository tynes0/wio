#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace wio::runtime
{
    namespace detail
    {
        // Transactional append for compiler-proven local concat chains. Capture
        // alias lengths before reserve; repeated self operands mean the original
        // value, not the progressively growing destination. No views into a
        // possibly reallocated buffer survive reserve.
        template <std::size_t Count>
        void AppendStringParts(std::string& destination, const std::array<const std::string*, Count>& parts)
        {
            const auto originalSize = destination.size();
            auto required = originalSize;
            for (const auto* part : parts)
            {
                const auto size = part == &destination ? originalSize : part->size();
                if (size > destination.max_size() - required)
                    throw std::length_error("Wio string concatenation is too large.");

                required += size;
            }

            if (required > destination.capacity())
            {
                // Do not turn repeated append into exact-size growth on STL
                // implementations whose reserve does not grow geometrically.
                const auto capacity = destination.capacity();
                const auto growth = capacity / 2;
                const auto geometric =
                    growth <= destination.max_size() - capacity ? capacity + growth : destination.max_size();
                destination.reserve(required > geometric ? required : geometric);
            }

            // All length/allocation checks have succeeded. std::string with
            // standard char traits cannot throw while copying within capacity.
            for (const auto* part : parts)
            {
                if (part == &destination)
                    destination.append(destination.data(), originalSize);
                else
                    destination.append(*part);
            }
        }
    } // namespace detail

    template <class... Parts> void AppendConcat(std::string& destination, const Parts&... parts)
    {
        detail::AppendStringParts(destination, std::array<const std::string*, sizeof...(Parts)>{&parts...});
    }

    template <class... Parts> [[nodiscard]] std::string Concat(const std::string& first, const Parts&... parts)
    {
        std::string result;
        AppendConcat(result, first, parts...);
        return result;
    }
} // namespace wio::runtime
