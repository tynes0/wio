#include "../vm/src/string_intrinsics.h"

#include <iostream>
#include <stdexcept>

namespace
{
    void expect(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    using namespace wio::vm;
    try
    {
        auto value = Value::string("snapshot");
        const auto copy = value;
        value.mutableString()->reserve(4096);
        const auto capacity = value.mutableString()->capacity();
        const auto address = value.asString().data();
        const auto suffix = Value::string("0123456789abcdef");
        const Value* arguments[] = {&suffix};
        for (int index = 0; index < 100; ++index)
        {
            const auto result = detail::executeStringIntrinsic("Append", value, &value, arguments, 0);
            expect(result.succeeded, "append failed");
            expect(value.asString().data() == address, "append copied the reserved prefix");
        }
        expect(copy.asString() == "snapshot", "value snapshot was mutated");
        auto result = detail::executeStringIntrinsic("Clear", value, &value, {}, 0);
        expect(result.succeeded && value.asString().empty(), "clear failed");
        expect(value.mutableString()->capacity() == capacity, "clear discarded capacity");
        value = Value::string(std::string(80, 'x'));
        const Value* self[] = {&value};
        result = detail::executeStringIntrinsic("Append", value, &value, self, 0);
        expect(result.succeeded && value.asString() == std::string(160, 'x'), "self append failed");
        const auto before = value.asString().size();
        const auto invalid = Value::signedInteger(5);
        const Value* bad[] = {&invalid};
        result = detail::executeStringIntrinsic("Append", value, &value, bad, 0);
        expect(!result.succeeded && result.code == "WVM1131" && value.asString().size() == before,
               "failed append mutated the receiver");
        result = detail::executeStringIntrinsic("Clear", value, nullptr, {}, 0);
        expect(!result.succeeded && result.code == "WVM1135", "immutable receiver was accepted");
        auto scalar = Value::boolean(true);
        expect(scalar.mutableString() == nullptr, "non-string exposes mutable string storage");
        std::cout << "string-capacity-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
