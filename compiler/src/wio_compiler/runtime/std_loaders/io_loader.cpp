#include "wio/runtime/std_loaders/io_loader.h"

namespace wio::runtime
{
    bool IOLoader::load(const Ref<sema::Scope>& scope, std::vector<Ref<sema::Symbol>>& symbols)
    {
        auto& typeContext = Compiler::get().getTypeContext();
        
        sema::SymbolFlags flags = sema::SymbolFlags::createAllFalse();
        flags.set_isStd(true); 

        auto symbol = Ref<sema::Symbol>::Create("Print", typeContext.getFunctionType(typeContext.getI32(), {typeContext.getString()}), sema::SymbolKind::Function, flags, common::Location::invalid());
        symbols.push_back(symbol);

        scope->define("Print", symbol);

        return true;
    }
}
