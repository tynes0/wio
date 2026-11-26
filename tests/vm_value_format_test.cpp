#include "value_format_internal.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void expect(const bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    using wio::vm::Value;
    using wio::vm::detail::ValueFormatter;
    try
    {
        wio::bytecode::Module module;
        module.types.resize(10);
        module.types[0].kind = 14; // f64
        module.types[1].kind = 17; // string
        module.types[2].kind = 31; // f64[]
        module.types[2].arguments = {0};
        module.types[3].kind = 16; // char
        module.types[4].kind = 31; // char[]
        module.types[4].arguments = {3};
        module.types[5].kind = 32; // Dict<string, f64[]>
        module.types[5].arguments = {1, 2};
        module.types[6].kind = 29; // cyclic forged reference (bounded rejection)
        module.types[6].arguments = {6};
        module.types[7].kind = 28; // unsupported component
        module.types[8].kind = 18; // text
        module.types[9].kind = 13; // f32
        std::string output;
        expect(ValueFormatter::append(module, 0, Value::floating(1.23456789), output) && output == "1.23457",
               "VM retained shortest-format divergence");
        output.clear();
        const auto values = Value::array({Value::floating(-0.0), Value::floating(12500000.0)});
        expect(ValueFormatter::append(module, 2, values, output) && output == "[-0, 1.25e+07]",
               "VM array precision/signed-zero mismatch");
        output.clear();
        const auto dictionary = Value::dictionary(5, {Value::string("samples"), values}, false);
        expect(ValueFormatter::append(module, 5, dictionary, output) && output == "{samples: [-0, 1.25e+07]}",
               "VM dictionary formatting mismatch");
        output.clear();
        const auto characters = Value::array({Value::unsignedInteger('A'), Value::unsignedInteger(0)});
        expect(ValueFormatter::append(module, 4, characters, output) && output == std::string("[A, \0]", 6),
               "VM typed char/NUL was printed as integer");
        output.clear();
        expect(ValueFormatter::append(module, 8, Value::text(U"\U0001f30de\u0301\u65e5\u672c\u8a9e"), output) &&
                   output == "\xf0\x9f\x8c\x8d"
                             "e\xcc\x81\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
               "VM Unicode formatting mismatch");
        output.clear();
        expect(!ValueFormatter::append(module, 6, Value::floating(1), output), "VM formatter recursion is unbounded");
        expect(!ValueFormatter::append(module, 7, Value::component(7, {}), output),
               "VM formatter invented a representation for unsupported components");
        expect(!ValueFormatter::append(module, 99, values, output), "VM formatter accepted missing type metadata");
        expect(!ValueFormatter::append(module, 2, values, output, 129), "VM formatter ignored explicit depth limit");
        output.clear();
        expect(ValueFormatter::append(module, 9, Value::floating(static_cast<float>(1.23456789)), output) &&
                   output == "1.23457",
               "VM f32 formatting mismatch");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
