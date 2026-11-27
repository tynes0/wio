#include "wio/vm/machine.h"

#include <iostream>

namespace
{
    unsigned calls = 0;

    WioNativeAbiStatus thunk(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*, WioNativeAbiFailure*) noexcept
    {
        ++calls;
        return WIO_NATIVE_ABI_OK;
    }

    wio::bytecode::Module fixture()
    {
        wio::bytecode::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {"Native", "preflight:fixture", "CppRecord"};
        module.types.push_back(wio::bytecode::Type{.kind = 1});
        wio::bytecode::Function function;
        function.id = 0;
        function.name = 0;
        function.returnType = 0;
        function.hasNativeBinding = true;
        function.nativeBinding.stableKey = 1;
        function.nativeBinding.result.type = 0;
        module.functions.push_back(function);
        return module;
    }
} // namespace

int main()
{
    auto module = fixture();
    wio::vm::Machine machine{module};
    std::string error;
    if (machine.nativeBindingsAvailable(&error) || error.find("preflight:fixture") == std::string::npos)
        return 1;
    const WioNativeAbiFunctionDescriptor descriptor{.stableId = 1, .stableKey = "preflight:fixture", .thunk = thunk};
    const WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, 1, &descriptor};
    if (!machine.registerNativeRegistry(registry, &error) || !machine.nativeBindingsAvailable(&error) || calls != 0)
        return 2;

    auto generic = fixture();
    generic.types.push_back(wio::bytecode::Type{.kind = 21});
    generic.functions.front().genericParameters = {1};
    wio::vm::Machine genericMachine{generic};
    if (!genericMachine.nativeBindingsAvailable(&error))
        return 3;
    auto ownerGeneric = generic;
    ownerGeneric.functions.front().genericParameters.clear();
    ownerGeneric.functions.front().ownerType = 1;
    wio::vm::Machine ownerGenericMachine{ownerGeneric};
    if (!ownerGenericMachine.nativeBindingsAvailable(&error))
        return 5;

    auto nativePod = fixture();
    nativePod.functions.clear();
    nativePod.types.push_back(wio::bytecode::Type{
        .kind = 28, .name = 2, .nominalKind = 1, .nominalRepresentation = 1, .hasNativeBinding = true});
    wio::vm::Machine podMachine{nativePod};
    if (podMachine.nativeBindingsAvailable(&error) || error.find("native POD") == std::string::npos)
        return 4;
    std::cout << "Native preflight checks bindings and layouts without invoking Wio/native functions\n";
    return 0;
}
