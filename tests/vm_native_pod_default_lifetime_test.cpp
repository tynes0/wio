#include "wio/vm/machine.h"

#include <cstring>
#include <iostream>

int main()
{
    namespace bc = wio::bytecode;
    bc::Module module;
    module.abiDescriptorVersion = 1;
    module.contract.callTableStableId = 1;
    module.strings = {"opaque", "Pod", "pointer", "Entry"};
    module.constants = {bc::Constant{}};
    module.types = {
        bc::Type{.kind = 20},
        bc::Type{
            .kind = 28, .name = 1, .nominalKind = 1, .nominalRepresentation = 1, .fields = {{.name = 2, .type = 0}}}};
    module.types[1].hasNativeBinding = true;
    module.contract.reflection = {{.stableTypeId = 911, .logicalName = 0, .type = 0},
                                  {.stableTypeId = 910, .logicalName = 1, .type = 1, .nominalKind = 1}};
    bc::Instruction construct;
    construct.opcode = bc::Opcode::ConstructComponent;
    construct.result = 0;
    construct.resultType = 1;
    construct.constant = 0;
    bc::Instruction finish;
    finish.opcode = bc::Opcode::Return;
    finish.constant = 0;
    finish.operands = {0};
    bc::Function entry;
    entry.id = 0;
    entry.name = 3;
    entry.returnType = 1;
    entry.blocks = {bc::Block{.id = 0, .instructions = {construct, finish}}};
    module.functions.push_back(std::move(entry));

    wio::vm::Value pointer;
    wio::vm::Value backing;
    {
        wio::vm::Machine machine{module};
        std::string error;
        if (!machine.loadNativeLibrary(WIO_TEST_POD_DEFAULT_LIBRARY, &error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        const auto result = machine.invoke(0);
        if (!result.succeeded())
        {
            std::cerr << result.error().code << ": " << result.error().message << '\n';
            return 1;
        }
        pointer = *result.value().field(0);
        backing = result.value().nativePodBacking();
    }
    // Only the escaped default field remains. Its native static data must stay
    // loaded after factory output, enclosing component and machine are gone.
    if (!pointer.opaquePointer() || *static_cast<const std::int32_t*>(pointer.opaquePointer()) != 47)
    {
        std::cerr << "Default POD pointer field lost its producer library lease\n";
        return 1;
    }
    pointer = {};
    void* storedPointer = nullptr;
    if (backing.nativePodBytes().size() != sizeof(void*))
        return 1;
    std::memcpy(&storedPointer, backing.nativePodBytes().data(), sizeof(storedPointer));
    // An immutable native prototype also pins its producer, even without any
    // visible opaque field left alive. Hidden fields may reference static data.
    if (!storedPointer || *static_cast<const std::int32_t*>(storedPointer) != 47)
    {
        std::cerr << "Default POD prototype lost its producer library lease\n";
        return 1;
    }
    return 0;
}
