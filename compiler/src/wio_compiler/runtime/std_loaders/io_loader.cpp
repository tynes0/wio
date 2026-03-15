#include "wio/runtime/std_loaders/io_loader.h"

#include "runtime/io.h"

namespace wio::runtime
{
    bool IOLoader::load(const Ref<sema::Scope>& scope, std::vector<Ref<sema::Symbol>>& symbols)
    {
        auto& typeContext = Compiler::get().getTypeContext();
        auto symbol = Ref<sema::Symbol>::Create("Print", typeContext.getFunctionType(typeContext.getI32(), {typeContext.getI32()}), sema::SymbolKind::Function, sema::SymbolFlags{}, common::Location::invalid());
        symbols.push_back(symbol);

        scope->define("Print", symbol);

        return true;
    }
}
