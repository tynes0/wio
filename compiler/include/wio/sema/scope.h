#pragma once

#include <map>
#include <string>
#include "wio/common/smart_ptr.h"

namespace wio::sema
{
    struct Symbol;
    enum class ScopeKind : uint8_t;
    
    class Scope : public RefCountedObject
    {
    public:
        Scope(WeakRef<Scope> parent, ScopeKind kind);

        void define(const std::string& name, const Ref<Symbol>& symbol);
        Ref<Symbol> resolve(const std::string& name);
        Ref<Symbol> resolveLocally(const std::string& name);
        
        WeakRef<Scope> getParent() const;
        ScopeKind getKind() const;
        
    private:
        WeakRef<Scope> parent_;
        ScopeKind kind_;
        std::map<std::string, Ref<Symbol>> symbols_;
    };
}
