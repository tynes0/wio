#include "wio/bytecode/disassembler.h"

#include <sstream>

namespace wio::bytecode
{
    std::string Disassembler::disassemble(const Module& module) const
    {
        std::ostringstream output;
        output << ".wiob " << FormatMajor << '.' << FormatMinor << "\nmodule \"" << module.string(module.name)
               << "\" stable=0x" << std::hex << module.stableId << std::dec << "\n";
        output << "strings " << module.strings.size() << "\nconstants " << module.constants.size() << "\ntypes "
               << module.types.size() << "\nglobals " << module.globals.size()
               << "\ncontract imports=" << module.contract.imports.size()
               << " exports=" << module.contract.exports.size() << " reflection=" << module.contract.reflection.size()
               << " attributes=" << module.contract.attributes.size() << " systems=" << module.contract.systems.size()
               << " application=" << (module.contract.hasApplication ? "yes" : "no") << "\n";
        for (const Import& import : module.contract.imports)
            output << "import 0x" << std::hex << import.stableId << std::dec << " \""
                   << module.string(import.logicalName) << "\"\n";
        for (const Export& exportRecord : module.contract.exports)
            output << "export[" << exportRecord.callTableSlot << "] 0x" << std::hex << exportRecord.stableId << std::dec
                   << " \"" << module.string(exportRecord.logicalName) << "\" fn=@" << exportRecord.function << "\n";
        for (const Function& function : module.functions)
        {
            output << "fn @" << function.id << " \"" << module.string(function.name) << "\" -> !t"
                   << function.returnType << " flags=0x" << std::hex << function.flags << std::dec << " {\n";
            if (function.hasNativeBinding)
                output << "  native \"" << module.string(function.nativeBinding.symbol) << "\" thunk=\""
                       << module.string(function.nativeBinding.thunkSymbol) << "\"\n";
            if (function.hasCoroutine)
                output << "  coroutine states=" << function.coroutine.states.size()
                       << " frame-slots=" << function.coroutine.frameSlots.size() << "\n";
            for (const Block& block : function.blocks)
            {
                output << "  ^b" << block.id << " \"" << module.string(block.name) << "\":\n";
                for (const Instruction& instruction : block.instructions)
                {
                    output << "    ";
                    if (instruction.result != InvalidIndex)
                        output << "%v" << instruction.result << " = ";
                    output << opcodeName(instruction.opcode);
                    for (const std::uint32_t operand : instruction.operands)
                        output << " %v" << operand;
                    for (const BranchTarget& target : instruction.targets)
                    {
                        output << " ^b" << target.block;
                        if (!target.arguments.empty())
                        {
                            output << '(';
                            for (std::size_t i = 0; i < target.arguments.size(); ++i)
                            {
                                if (i != 0)
                                    output << ", ";
                                output << "%v" << target.arguments[i];
                            }
                            output << ')';
                        }
                    }
                    if (instruction.callee != InvalidIndex)
                        output << " callee=@" << instruction.callee;
                    if (instruction.global != InvalidIndex)
                        output << " global=@g" << instruction.global;
                    if (!module.string(instruction.selector).empty())
                        output << " selector=\"" << module.string(instruction.selector) << '"';
                    if (instruction.resultType != InvalidIndex)
                        output << " : !t" << instruction.resultType;
                    if (instruction.source.begin.file != InvalidIndex)
                        output << "  # " << module.string(instruction.source.begin.file) << ':'
                               << instruction.source.begin.line << ':' << instruction.source.begin.column;
                    output << '\n';
                }
            }
            output << "}\n";
        }
        return output.str();
    }
} // namespace wio::bytecode
