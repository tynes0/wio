#include "wio/runtime/std_loaders/io_loader.h"

namespace wio::runtime
{
    bool IOLoader::load(const Ref<sema::Scope>& scope, std::vector<Ref<sema::Symbol>>& symbols)
    {
        auto& typeContext = Compiler::get().getTypeContext();
        
        sema::SymbolFlags flags = sema::SymbolFlags::createAllFalse();
        flags.set_isStd(true);

        auto addPrintOverload = [&](const Ref<sema::Type>& paramType) 
        {
            auto funcType = typeContext.getOrCreateFunctionType(typeContext.getI32(), {paramType});
            auto symbol = Ref<sema::Symbol>::Create("Print", funcType, sema::SymbolKind::Function, flags, common::Location::invalid());
            
            symbols.push_back(symbol);
            
            scope->define("Print", symbol); 
        };

        addPrintOverload(typeContext.getString()); // Print(string)
        addPrintOverload(typeContext.getI64());    // Print(i32)
        addPrintOverload(typeContext.getF64());    // Print(f32)
        addPrintOverload(typeContext.getBool());   // Print(bool)
        addPrintOverload(typeContext.getU32());    // Print(u32)/
        
        return true;
    }
}
