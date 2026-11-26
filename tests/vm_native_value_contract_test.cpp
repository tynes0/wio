#include "native_bridge_internal.h"
#include "wio/bytecode/verifier.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
    namespace bridge = wio::vm::detail;
    using wio::vm::Value;

    bool expect(const bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }
} // namespace

int main()
{
    wio::bytecode::Module module;
    // Type indexes intentionally match primitive kind ids in this fixture only.
    for (std::uint8_t kind = 0; kind <= 18; ++kind)
        module.types.push_back(wio::bytecode::Type{.kind = kind});
    module.types.push_back(wio::bytecode::Type{.kind = 20});                                // 19 opaque
    module.types.push_back(wio::bytecode::Type{.kind = 30, .arguments = {19}});             // 20 opaque?
    module.types.push_back(wio::bytecode::Type{.kind = 29, .arguments = {20}, .flags = 1}); // 21 ref opaque?
    module.types.push_back(wio::bytecode::Type{.kind = 29, .arguments = {19}});             // 22 view opaque
    module.types.push_back(wio::bytecode::Type{.kind = 29, .arguments = {5}});              // 23 view i32
    module.types.push_back(wio::bytecode::Type{.kind = 29, .arguments = {19}, .flags = 1}); // 24 ref opaque

    bool passed = true;
    std::string error;
    WioNativeAbiValue wire;
    for (const auto [kind, bits] :
         std::array<std::pair<std::uint8_t, unsigned>, 4>{{{3, 8}, {4, 16}, {5, 32}, {6, 64}}})
    {
        const auto maximum =
            bits == 64 ? (std::numeric_limits<std::int64_t>::max)() : (std::int64_t{1} << (bits - 1)) - 1;
        const auto minimum = -maximum - 1;
        for (const auto value : {minimum, maximum})
        {
            passed &= expect(bridge::encodeNativeValue(module, Value::signedInteger(value), kind, 1, wire, error),
                             "signed boundary encode");
            const auto decoded = bridge::decodeNativeValue(module, wire, kind, 1);
            passed &= expect(decoded.succeeded() && decoded.value.asSignedInteger() == value, "signed boundary decode");
        }
        if (bits != 64)
        {
            for (const auto value : {minimum - 1, maximum + 1})
            {
                passed &= expect(!bridge::encodeNativeValue(module, Value::signedInteger(value), kind, 1, wire, error),
                                 "signed overflow encode accepted");
                wire.kind = WIO_NATIVE_ABI_I64;
                wire.payload.signedInteger = value;
                passed &= expect(!bridge::decodeNativeValue(module, wire, kind, 1).succeeded(),
                                 "signed overflow decode accepted");
            }
        }
    }
    for (const auto [kind, bits] :
         std::array<std::pair<std::uint8_t, unsigned>, 6>{{{8, 8}, {9, 16}, {10, 32}, {11, 64}, {15, 8}, {16, 32}}})
    {
        const auto maximum = bits == 64 ? (std::numeric_limits<std::uint64_t>::max)() : (std::uint64_t{1} << bits) - 1;
        passed &= expect(bridge::encodeNativeValue(module, Value::unsignedInteger(maximum), kind, 1, wire, error),
                         "unsigned boundary encode");
        const auto decoded = bridge::decodeNativeValue(module, wire, kind, 1);
        passed &=
            expect(decoded.succeeded() && decoded.value.asUnsignedInteger() == maximum, "unsigned boundary decode");
        if (bits != 64)
        {
            passed &=
                expect(!bridge::encodeNativeValue(module, Value::unsignedInteger(maximum + 1), kind, 1, wire, error),
                       "unsigned overflow encode accepted");
            wire.kind = WIO_NATIVE_ABI_U64;
            wire.payload.unsignedInteger = maximum + 1;
            passed &= expect(!bridge::decodeNativeValue(module, wire, kind, 1).succeeded(),
                             "unsigned overflow decode accepted");
        }
    }

    const double original = 1.0 / 3.0;
    const double rounded = static_cast<float>(original);
    passed &= expect(bridge::encodeNativeValue(module, Value::floating(original), 13, 1, wire, error) &&
                         wire.payload.floatingPoint == rounded,
                     "f32 encode was not rounded");
    wire.payload.floatingPoint = original;
    const auto roundedResult = bridge::decodeNativeValue(module, wire, 13, 1);
    passed &=
        expect(roundedResult.succeeded() && roundedResult.value.asFloat64() == rounded, "f32 decode was not rounded");

    for (const bool unicode : {false, true})
    {
        for (const bool empty : {false, true})
        {
            const std::uint32_t type = unicode ? 18 : 17;
            const std::uint8_t marshalling = unicode ? 3 : 2;
            const auto makeValue = [&]()
            {
                return unicode ? Value::text(empty ? U"" : std::u32string{U'a', U'\0', U'\U0001f680'})
                               : Value::string(empty ? "" : std::string{"a\0bytes", 7});
            };
            {
                const Value temporary = makeValue();
                passed &= expect(bridge::encodeOwnedNativeValue(module, temporary, type, marshalling, wire, error),
                                 "owned callback encode");
            }
            passed &= expect(wire.flags == WIO_NATIVE_ABI_VALUE_OWNED && wire.owner.state && wire.owner.ops,
                             "slice did not transfer owned storage");
            WioNativeAbiValue retained = wire;
            WioNativeAbiRetain(retained.owner);
            WioNativeAbiReleaseValue(&wire);
            WioNativeAbiReleaseValue(&wire); // Idempotent, no double release.
            const auto decoded = bridge::decodeNativeValue(module, retained, type, marshalling);
            passed &= expect(decoded.succeeded() && decoded.value == makeValue(),
                             "callback storage did not survive frame destruction and release");
            WioNativeAbiReleaseValue(&retained);
        }
    }

    wire.kind = WIO_NATIVE_ABI_TEXT_UTF32;
    const char32_t invalid = 0xd800;
    passed &= expect(!bridge::encodeNativeValue(module, Value::text(std::u32string{invalid}), 18, 3, wire, error),
                     "invalid VM Unicode scalar encoded");
    wire.kind = WIO_NATIVE_ABI_TEXT_UTF32;
    wire.payload.slice = {&invalid, sizeof(invalid)};
    passed &= expect(!bridge::decodeNativeValue(module, wire, 18, 3).succeeded(), "invalid Unicode scalar accepted");
    wire.payload.slice = {nullptr, 4};
    passed &= expect(!bridge::decodeNativeValue(module, wire, 18, 3).succeeded(), "null nonempty slice accepted");
    wire.payload.slice = {&invalid, 3};
    passed &= expect(!bridge::decodeNativeValue(module, wire, 18, 3).succeeded(), "partial Unicode scalar accepted");
    std::uint8_t marshalling = 0;
    passed &= expect(bridge::encodeNativeValue(module, Value::null(), 21, 5, wire, error) &&
                         wire.kind == WIO_NATIVE_ABI_VOID && (wire.flags & WIO_NATIVE_ABI_VALUE_NULLABLE),
                     "nullable ref did not encode null");
    passed &= expect(bridge::decodeNativeValue(module, wire, 21, 5).value.kind() == Value::Kind::Null,
                     "nullable ref did not decode null");
    passed &=
        expect(!bridge::encodeNativeValue(module, Value::null(), 22, 5, wire, error), "nonnullable view accepted null");
    wire = {};
    wire.kind = WIO_NATIVE_ABI_OPAQUE;
    passed &= expect(bridge::decodeNativeValue(module, wire, 20, 5).value.kind() == Value::Kind::Null,
                     "nullable opaque pointer null not normalized");
    passed &= expect(!bridge::callbackValueMarshalling(module, 21, marshalling),
                     "borrowed callback silently classified by value");
    passed &= expect(!bridge::callbackValueMarshalling(module, 20, marshalling),
                     "nullable callback silently classified by value");
    passed &= expect(bridge::callbackValueMarshalling(module, 17, marshalling) && marshalling == 2,
                     "string callback classification");
    passed &= expect(bridge::callbackValueMarshalling(module, 18, marshalling) && marshalling == 3,
                     "text callback classification");
    passed &=
        expect(bridge::encodeOwnedNativeValue(module, Value::signedInteger(42), 5, 1, wire, error) && !wire.owner.state,
               "scalar callback allocated slice storage");

    module.abiDescriptorVersion = 1;
    module.contract.callTableStableId = 1;
    module.strings = {"fixture:view"};
    module.constants = {wio::bytecode::Constant{}};
    wio::bytecode::Function native;
    native.id = 0;
    native.flags = 0x0042;
    native.returnType = 1;
    native.parameters = {{.value = 0, .type = 22}};
    native.hasNativeBinding = true;
    native.nativeBinding.stableKey = 0;
    native.nativeBinding.parameters = {{.type = 22, .passing = 1, .marshalling = 5}};
    native.nativeBinding.result = {.type = 1, .marshalling = 0};
    wio::bytecode::Function caller;
    caller.id = 1;
    caller.returnType = 1;
    caller.parameters = {{.value = 0, .type = 21}};
    caller.blocks = {
        {.id = 0,
         .instructions = {
             {.opcode = wio::bytecode::Opcode::NativeInvoke, .resultType = 1, .operands = {0}, .callee = 0},
             {.opcode = wio::bytecode::Opcode::Return}}}};
    module.functions = {native, caller};
    for (auto& instruction : module.functions[1].blocks[0].instructions)
        instruction.constant = 0;
    const auto narrowed = wio::bytecode::Verifier{}.verify(module);
    if (!narrowed.succeeded())
        for (const auto& diagnostic : narrowed.diagnostics())
            std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    passed &= expect(narrowed.succeeded(), "flow-narrowed native view rejected");
    module.functions[1].parameters[0].type = 23;
    passed &=
        expect(!wio::bytecode::Verifier{}.verify(module).succeeded(), "unrelated view type admitted to native call");
    module.functions[1].parameters[0].type = 21;
    module.functions[0].parameters[0].type = 24;
    module.functions[0].nativeBinding.parameters[0] = {.type = 24, .passing = 2, .marshalling = 5};
    passed &= expect(!wio::bytecode::Verifier{}.verify(module).succeeded(),
                     "mutable nullable copy-back lost exact type identity");
    return passed ? 0 : 1;
}
