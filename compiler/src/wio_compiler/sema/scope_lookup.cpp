#include "wio/sema/scope_lookup.h"

namespace wio::sema::scope_lookup
{
    Ref<Symbol> resolveQualifiedSymbol(const Ref<Scope>& startScope, const std::string_view qualifiedName)
    {
        if (!startScope || qualifiedName.empty())
            return nullptr;

        std::size_t segmentStart = 0;
        Ref<Scope> scope = startScope;
        Ref<Symbol> resolvedSymbol = nullptr;

        while (segmentStart < qualifiedName.size())
        {
            const std::size_t separator = qualifiedName.find("::", segmentStart);
            const std::string segment = separator == std::string_view::npos
                ? std::string(qualifiedName.substr(segmentStart))
                : std::string(qualifiedName.substr(segmentStart, separator - segmentStart));

            if (segment.empty())
                return nullptr;

            resolvedSymbol = scope->resolve(segment);
            if (!resolvedSymbol)
                return nullptr;

            if (separator == std::string_view::npos)
                return resolvedSymbol;
            if (!resolvedSymbol->innerScope)
                return nullptr;

            scope = resolvedSymbol->innerScope;
            segmentStart = separator + 2;
        }

        return resolvedSymbol;
    }
}
