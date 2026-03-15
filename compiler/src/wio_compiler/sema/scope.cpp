#include "wio/sema/scope.h"
#include "wio/sema/symbol.h"

#include "wio/common/exception.h"

namespace wio::sema
{
    Scope::Scope(WeakRef<Scope> parent, ScopeKind kind)
        : parent_(std::move(parent)), kind_(kind)
    {
    }

    void Scope::define(const std::string& name, const Ref<Symbol>& symbol)
    {
        if (auto it = symbols_.find(name); it != symbols_.end())
        {
            if (symbol)
                throw RedefinitionError(
                    ("Symbol already defined here: " + it->second->definitionLoc.toString()).c_str(),
                    symbol->definitionLoc
                    );
            throw RedefinitionError(("Symbol already defined here: " + it->second->definitionLoc.toString()).c_str());
        }
        
        symbols_[name] = symbol;
    }

    Ref<Symbol> Scope::resolve(const std::string& name)
    {
        if (auto it = symbols_.find(name); it != symbols_.end())
        {
            return it->second;
        }

        if (auto locked = parent_.Lock(); locked)
        {
            return locked->resolve(name);
        }

        return nullptr;
    }

    WeakRef<Scope> Scope::getParent() const
    {
        return parent_;
    }

    ScopeKind Scope::getKind() const
    {
        return kind_;
    }
}
