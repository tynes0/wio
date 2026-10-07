#include "wio/vm/machine.h"

#include <iostream>

namespace
{
    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    wio::bytecode::Module makeModule()
    {
        namespace bc = wio::bytecode;
        bc::Module module;
        module.abiDescriptorVersion = 1;
        module.contract.callTableStableId = 1;
        module.strings = {
            "test:vm-pod-pointer-lifetime", "make", "Entry", "pointer", "slots", "Pod", "opaque", "slots-type"};
        module.constants = {bc::Constant{}};
        module.types = {bc::Type{.kind = 20}, bc::Type{.kind = 31, .arguments = {0}, .staticExtent = 2},
                        bc::Type{.kind = 28,
                                 .name = 5,
                                 .nominalKind = 1,
                                 .nominalRepresentation = 1,
                                 .fields = {{.name = 3, .type = 0}, {.name = 4, .type = 1}}}};
        module.types[2].hasNativeBinding = true;
        module.contract.reflection = {{.stableTypeId = 501, .logicalName = 6, .type = 0},
                                      {.stableTypeId = 503, .logicalName = 7, .type = 1},
                                      {.stableTypeId = 500, .logicalName = 5, .type = 2, .nominalKind = 1}};
        bc::Function native;
        native.id = 0;
        native.name = 1;
        native.returnType = 2;
        native.flags = 0x0042u;
        native.hasNativeBinding = true;
        native.nativeBinding.stableKey = 0;
        native.nativeBinding.result = {.type = 2, .passing = 4, .marshalling = 4};
        module.functions.push_back(std::move(native));
        bc::Instruction invoke;
        invoke.opcode = bc::Opcode::NativeInvoke;
        invoke.result = 0;
        invoke.resultType = 2;
        invoke.callee = 0;
        invoke.constant = 0;
        bc::Instruction returnValue;
        returnValue.opcode = bc::Opcode::Return;
        returnValue.constant = 0;
        returnValue.operands = {0};
        bc::Function entry;
        entry.id = 1;
        entry.name = 2;
        entry.returnType = 2;
        entry.blocks.push_back(bc::Block{.id = 0, .instructions = {invoke, returnValue}});
        module.functions.push_back(std::move(entry));
        return module;
    }
} // namespace

int main()
{
    bool passed = true;
    const auto module = makeModule();
    wio::vm::Value escaped;
    {
        wio::vm::Machine machine{module};
        std::string error;
        passed &= expect(machine.loadNativeLibrary(WIO_TEST_POD_POINTER_LIBRARY, &error), error.c_str());
        const auto result = machine.invoke(1);
        if (!result.succeeded())
            std::cerr << result.error().code << ": " << result.error().message << '\n';
        passed &= expect(result.succeeded(), "POD pointer factory failed");
        if (result.succeeded())
            escaped = result.value();
    }
    if (escaped.fieldCount() != 2)
        return 1;
    // Only the aggregate's child opaque storage remains alive. Its DLL code
    // and static pointees must survive after the last machine lease ends.
    auto escapedPointer = *escaped.field(0);
    auto escapedSlots = *escaped.field(1);
    escaped = {};
    passed &= expect(*static_cast<int*>(escapedPointer.opaquePointer()) == 41,
                     "Owned opaque field died with its enclosing machine");
    // Release invokes producer code after machine teardown. After this, only
    // array elements can keep the DLL and its static pointees loaded.
    escapedPointer = {};
    passed &=
        expect(escapedSlots.elementCount() == 2 && *static_cast<int*>(escapedSlots.element(0)->opaquePointer()) == 17 &&
                   *static_cast<int*>(escapedSlots.element(1)->opaquePointer()) == 29,
               "Borrowed array pointers did not pin native static data");
    escapedSlots = {};
    return passed ? 0 : 1;
}
