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
               << module.types.size() << "\nglobals " << module.globals.size() << "\n";
        for (const Function& function : module.functions)
        {
            output << "fn @" << function.id << " \"" << module.string(function.name) << "\" -> !t"
                   << function.returnType << " flags=0x" << std::hex << function.flags << std::dec << " {\n";
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
                        output << " ^b" << target.block;
                    if (instruction.resultType != InvalidIndex)
                        output << " : !t" << instruction.resultType;
                    output << '\n';
                }
            }
            output << "}\n";
        }
        return output.str();
    }
} // namespace wio::bytecode
