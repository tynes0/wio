#include "native_any_internal.h"
#include "native_library.h"
#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wir_any_abi.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

namespace
{
    namespace bridge = wio::vm::detail;
    namespace abi = wio::wir_backend::abi;
    using wio::vm::Value;
    using Any = wio::runtime::Any;
    using Integers = abi::AnyPayloadAdapter<std::int32_t, 100, 11>;
    using Strings = abi::AnyPayloadAdapter<std::string, 100, 12>;

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    int owners = 0;
    void retain(void*) noexcept
    {
        ++owners;
    }
    void release(void*) noexcept
    {
        --owners;
    }
    std::uint64_t ownerType(const void*) noexcept
    {
        return 100;
    }
    const WioNativeAbiHandleOps ownerOps{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &ownerType};

    WioNativeAbiStatus partial(const WioNativeAbiValue*, WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
    {
        ++owners;
        result->kind = WIO_NATIVE_ABI_BYTES;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->owner = {&owners, &ownerOps, 1};
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    WioNativeAbiStatus partialRead(const WioNativeAbiValue* value, WioNativeAbiValue* result, bool* matched,
                                   WioNativeAbiFailure* error) noexcept
    {
        *matched = true;
        return partial(value, result, error);
    }

    bool lifetime(const char* bytecodePath, const char* libraryPath)
    {
        std::ifstream input(std::filesystem::u8path(bytecodePath), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        if (!expect(decoded.succeeded(), "any lifetime bytecode decode failed"))
            return false;
        std::uint32_t escape = wio::bytecode::InvalidIndex;
        for (const auto& function : decoded.module.functions)
            if (decoded.module.string(function.name) == "Escape")
                escape = function.id;
        std::string error;
        Value escaped;
        std::weak_ptr<bridge::NativeLibrary> lease;
        {
            auto producer = bridge::NativeLibrary::open(std::filesystem::u8path(libraryPath), error);
            if (!expect(bool(producer), "any lifetime producer open failed"))
                return false;
            lease = producer;
            wio::vm::Machine machine{decoded.module};
            if (!expect(machine.loadNativeLibrary(std::filesystem::u8path(libraryPath), &error),
                        "any lifetime producer registration failed"))
                return false;
            producer.reset();
            const auto result = machine.invoke(escape);
            if (!expect(result.succeeded() && result.value().kind() == Value::Kind::Any,
                        "any lifetime invocation failed"))
                return false;
            escaped = result.value();
        }
        if (!expect(!lease.expired(), "escaped any failed to pin producer code"))
            return false;
        Value child = *escaped.anyPayload();
        escaped = {};
        if (!expect(!lease.expired(), "escaped any POD payload lost its producer lease"))
            return false;
        child = {};
        return expect(lease.expired(), "any producer lease leaked after final release");
    }
} // namespace

int main(int argc, char** argv)
{
    wio::bytecode::Module module;
    module.types = {{.kind = 19}, {.kind = 5}, {.kind = 17}, {.kind = 5}};
    module.contract.reflection = {{.stableTypeId = 100, .type = 0},
                                  {.stableTypeId = 11, .type = 1},
                                  {.stableTypeId = 12, .type = 2},
                                  {.stableTypeId = 13, .type = 3}};
    const std::array payloads{WioNativeAnyPayloadDescriptor{11, &Integers::pack, &Integers::read},
                              WioNativeAnyPayloadDescriptor{12, &Strings::pack, &Strings::read}};
    const WioNativeAnyApi api{
        1, 2, 100, payloads.data(), &abi::AnyAdapter<100>::packNull, &abi::AnyAdapter<100>::isNull};
    bridge::NativePodLayouts layouts;
    std::string error;
    bool passed = expect(bridge::mergeNativeAny(module, api, layouts, error), "any registration failed");
    const Value original = Value::any(1, Value::signedInteger(7));
    WioNativeAbiValue wire{};
    passed &= expect(bridge::encodeNativeValue(module, original, 0, 8, wire, error, &layouts), "any pack failed");
    const auto decoded = bridge::decodeNativeValue(module, wire, 0, 8, &layouts);
    passed &= expect(decoded.succeeded() && decoded.value.anyType() == 1 &&
                         decoded.value.anyPayload()->asSignedInteger() == 7,
                     "any integer round-trip failed");
    auto nativeCopy = abi::Codec<Any, 100>::read(wire);
    nativeCopy.AsBoxed<std::int32_t>() = 41;
    WioNativeAbiReleaseValue(&wire);
    passed &=
        expect(bridge::encodeNativeValue(module, original, 0, 8, wire, error, &layouts), "any backing reuse failed");
    const auto refreshed = bridge::decodeNativeValue(module, wire, 0, 8, &layouts);
    passed &= expect(refreshed.succeeded() && refreshed.value.anyPayload()->asSignedInteger() == 41,
                     "native mutation through a copied AnyCell lost identity");
    WioNativeAbiReleaseValue(&wire);

    const Value text = Value::any(2, Value::string(std::string{"a\0b", 3}));
    passed &= expect(bridge::encodeNativeValue(module, text, 0, 8, wire, error, &layouts), "any string encode failed");
    const auto string = bridge::decodeNativeValue(module, wire, 0, 8, &layouts);
    passed &= expect(string.succeeded() && string.value.anyPayload()->asString() == std::string_view{"a\0b", 3},
                     "boxed string lost embedded NUL");
    WioNativeAbiReleaseValue(&wire);
    passed &=
        expect(bridge::encodeNativeValue(module, Value::null(), 0, 8, wire, error, &layouts), "null any encode failed");
    const auto null = bridge::decodeNativeValue(module, wire, 0, 8, &layouts);
    passed &= expect(null.succeeded() && null.value.kind() == Value::Kind::Null, "null any round-trip failed");
    WioNativeAbiReleaseValue(&wire);

    auto invalid = api;
    invalid.version = 99;
    bridge::NativePodLayouts empty;
    passed &= expect(!bridge::mergeNativeAny(module, invalid, empty, error) && empty.any.empty(),
                     "invalid any version was published");
    invalid = api;
    invalid.typeId = 11;
    passed &= expect(!bridge::mergeNativeAny(module, invalid, empty, error), "non-any type id was accepted");
    auto broken = payloads;
    broken[1].read = nullptr;
    invalid = api;
    invalid.payloads = broken.data();
    passed &= expect(!bridge::mergeNativeAny(module, invalid, empty, error) && empty.any.empty(),
                     "partial registration leaked an any adapter");
    passed &= expect(!bridge::encodeNativeValue(module, original, 0, 8, wire, error, &empty),
                     "any value without adapter was accepted");

    wire = abi::Codec<Any, 100>::write(Any::Box(1.5));
    const auto unknown = bridge::decodeNativeValue(module, wire, 0, 8, &layouts);
    passed &= expect(unknown.succeeded() && unknown.value.kind() == Value::Kind::RuntimeValue,
                     "unknown producer payload was guessed or lost native forwarding");
    WioNativeAbiReleaseValue(&wire);
    auto ambiguous = layouts;
    ambiguous.any.at(0).payloads.emplace(3, WioNativeAnyPayloadDescriptor{13, &Integers::pack, &Integers::read});
    wire = abi::Codec<Any, 100>::write(Any::Box(std::int32_t{1}));
    passed &= expect(!bridge::decodeNativeValue(module, wire, 0, 8, &ambiguous).succeeded(),
                     "ambiguous producer aliases were silently guessed");
    WioNativeAbiReleaseValue(&wire);
    auto failing = layouts;
    failing.any.at(0).payloads.at(1).pack = &partial;
    const Value fresh = Value::any(1, Value::signedInteger(3));
    passed &= expect(!bridge::encodeNativeValue(module, fresh, 0, 8, wire, error, &failing) && owners == 0 &&
                         wire.kind == WIO_NATIVE_ABI_VOID && fresh.nativeAnyBacking().isEmpty(),
                     "failed any pack leaked ownership or published backing");
    failing = layouts;
    failing.any.at(0).payloads.at(1).read = &partialRead;
    wire = abi::Codec<Any, 100>::write(Any::Box(std::int32_t{3}));
    passed &= expect(!bridge::decodeNativeValue(module, wire, 0, 8, &failing).succeeded() && owners == 0,
                     "partially filled any payload leaked ownership");
    WioNativeAbiReleaseValue(&wire);
    wire.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
    wire.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
    wire.typeId = 100;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 0, 8, &layouts).succeeded(),
                     "any handle without producer operations was accepted");
    const Value shared = Value::any(1, Value::signedInteger(9));
    std::array<void*, 8> states{};
    std::array<std::thread, 8> threads;
    for (std::size_t index = 0; index < threads.size(); ++index)
        threads[index] = std::thread(
            [&, index]
            {
                WioNativeAbiValue concurrent{};
                std::string concurrentError;
                if (bridge::encodeNativeValue(module, shared, 0, 8, concurrent, concurrentError, &layouts))
                    states[index] = concurrent.payload.handle.state;
                WioNativeAbiReleaseValue(&concurrent);
            });
    for (auto& thread : threads)
        thread.join();
    for (auto* state : states)
        passed &= expect(state && state == states.front(), "concurrent any materialization split producer identity");
    if (argc == 3)
        passed &= lifetime(argv[1], argv[2]);
    return passed ? 0 : 1;
}
