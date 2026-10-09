#include "native_any_internal.h"
#include "native_library.h"
#include "wio/bytecode/codec.h"
#include "wio/codegen/wir_native_container_emitter.h"
#include "wio/vm/machine.h"

#include <wir_any_abi.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace
{
    namespace bridge = wio::vm::detail;
    namespace abi = wio::wir_backend::abi;
    using Any = wio::runtime::Any;
    using Value = wio::vm::Value;
    using Integers = abi::AnyPayloadAdapter<std::int32_t, 100, 11>;
    using Strings = abi::AnyPayloadAdapter<std::string, 100, 12>;
    using Nested = abi::AnyPayloadAdapter<Any, 100, 100>;

    void check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    struct Wire final
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };

    int partialOwners = 0;
    void retain(void*) noexcept
    {
        ++partialOwners;
    }
    void release(void*) noexcept
    {
        --partialOwners;
    }
    std::uint64_t typeId(const void*) noexcept
    {
        return 100;
    }
    const WioNativeAbiHandleOps partialOps{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &typeId};

    WioNativeAbiStatus failPack(const WioNativeAbiValue*, WioNativeAbiValue* output, WioNativeAbiFailure*) noexcept
    {
        ++partialOwners;
        output->kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
        output->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        output->owner = {&partialOwners, &partialOps, 1};
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    WioNativeAbiStatus failRead(const WioNativeAbiValue* value, WioNativeAbiValue* output, bool* matched,
                                WioNativeAbiFailure* error) noexcept
    {
        *matched = true;
        return failPack(value, output, error);
    }

    void selection()
    {
        using namespace wio::wir;
        lowered::Module module;
        const auto any = module.types.intern(Type{.kind = TypeKind::Any});
        const auto array = module.types.intern(Type{.kind = TypeKind::Array, .arguments = {any}});
        lowered::Function boundary;
        boundary.nativeBinding = NativeBinding{.parameters = {{.type = array}}};
        module.functions.push_back(std::move(boundary));
        const auto emit = [&]
        {
            return wio::codegen::WirNativeContainerEmitter::emit(module, [](TypeId id)
                                                                 { return "test_type_" + std::to_string(id.value()); });
        };
        const auto spelling = "using _native_any_payload_" + std::to_string(any.value()) + " = ";
        check(emit().find(spelling) != std::string::npos,
              "container-only Any boundary lost nested producer payload without AnyBox instructions");
        module.functions.front().nativeBinding.reset();
        check(emit().empty(), "module with no native boundary emitted unused Any adapters");
    }

    void contracts()
    {
        wio::bytecode::Module module;
        module.types = {{.kind = 19}, {.kind = 5}, {.kind = 17}};
        module.contract.reflection = {
            {.stableTypeId = 100, .type = 0}, {.stableTypeId = 11, .type = 1}, {.stableTypeId = 12, .type = 2}};
        const std::array descriptors{WioNativeAnyPayloadDescriptor{100, &Nested::pack, &Nested::read},
                                     WioNativeAnyPayloadDescriptor{11, &Integers::pack, &Integers::read},
                                     WioNativeAnyPayloadDescriptor{12, &Strings::pack, &Strings::read}};
        const WioNativeAnyApi api{
            WIO_NATIVE_ANY_VERSION,       3, 100, descriptors.data(), &abi::AnyAdapter<100>::packNull,
            &abi::AnyAdapter<100>::isNull};
        bridge::NativePodLayouts layouts;
        std::string error;
        check(bridge::mergeNativeAny(module, api, layouts, error), error.c_str());
        check(layouts.any.at(0).payloads.contains(0), "nested Any descriptor was not registered");

        const auto leaf = Value::any(1, Value::signedInteger(7));
        const auto inner = Value::any(0, leaf);
        const auto outer = Value::any(0, inner);
        Wire wire;
        check(bridge::encodeNativeValue(module, outer, 0, 8, wire.value, error, &layouts), error.c_str());
        auto native = abi::Codec<Any, 100>::read(wire.value);
        check(native.IsBoxed<Any>() && native.AsBoxed<Any>().IsBoxed<Any>(), "nested Any was flattened");
        auto* nativeInner = &native.AsBoxed<Any>();
        nativeInner->AsBoxed<Any>().AsBoxed<std::int32_t>() = 43;
        auto decoded = bridge::decodeNativeValue(module, wire.value, 0, 8, &layouts);
        check(decoded.succeeded() && decoded.value.anyType() == 0 && decoded.value.anyPayload()->anyType() == 0 &&
                  decoded.value.anyPayload()->anyPayload()->anyPayload()->asSignedInteger() == 43,
              "nested shared-cell mutation was not decoded");
        Wire leafWire;
        check(bridge::encodeNativeValue(module, leaf, 0, 8, leafWire.value, error, &layouts), error.c_str());
        auto nativeLeaf = abi::Codec<Any, 100>::read(leafWire.value);
        check(nativeLeaf.Cell() == nativeInner->AsBoxed<Any>().Cell() && nativeLeaf.AsBoxed<std::int32_t>() == 43,
              "nested payload and independently passed leaf split producer identity");

        *nativeInner = Any::Box(std::string{"a\0b", 3});
        decoded = bridge::decodeNativeValue(module, wire.value, 0, 8, &layouts);
        check(decoded.succeeded() && decoded.value.anyPayload()->anyType() == 2 &&
                  decoded.value.anyPayload()->anyPayload()->asString() == std::string_view{"a\0b", 3},
              "replacing an inner Any failed to preserve the outer cell or embedded NUL");
        check(nativeLeaf.AsBoxed<std::int32_t>() == 43, "inner rebind mutated the previously shared leaf");

        auto cleanupCell = Value::any(0, Value::null());
        Wire cleanupWire;
        check(bridge::encodeNativeValue(module, cleanupCell, 0, 8, cleanupWire.value, error, &layouts), error.c_str());
        {
            auto alias = cleanupCell;
            check(alias.takeNativeAnyBackingForCleanup().isEmpty() && !cleanupCell.nativeAnyBacking().isEmpty(),
                  "cleanup detached the producer backing of a still-shared VM Any cell");
        }
        auto transferred = cleanupCell.takeNativeAnyBackingForCleanup();
        check(transferred.kind() == Value::Kind::RuntimeValue && cleanupCell.nativeAnyBacking().isEmpty() &&
                  transferred.foreignHandle().state == cleanupWire.value.payload.handle.state,
              "final Any cleanup did not transfer exactly its own producer claim");

        Wire blank;
        const auto nestedNull = Value::any(0, Value::null());
        check(bridge::encodeNativeValue(module, nestedNull, 0, 8, blank.value, error, &layouts), error.c_str());
        const auto null = bridge::decodeNativeValue(module, blank.value, 0, 8, &layouts);
        check(null.succeeded() && null.value.kind() == Value::Kind::Any && null.value.anyType() == 0 &&
                  null.value.anyPayload()->kind() == Value::Kind::Null,
              "boxed null Any was collapsed to a null outer value");

        Value deep = Value::any(1, Value::signedInteger(1));
        for (unsigned index = 0; index < 70; ++index)
            deep = Value::any(0, std::move(deep));
        Wire rejected;
        check(!bridge::encodeNativeValue(module, deep, 0, 8, rejected.value, error, &layouts) &&
                  error.find("depth limit") != std::string::npos && rejected.value.kind == WIO_NATIVE_ABI_VOID &&
                  deep.nativeAnyBacking().isEmpty(),
              "excessive nesting escaped the encoder depth guard or published a partial outer cell");

        auto cycle = Any::Box(Any{});
        cycle.AsBoxed<Any>() = cycle;
        Wire cyclicWire;
        cyclicWire.value = abi::Codec<Any, 100>::write(cycle);
        const auto references = cycle.Cell()->StrongCount();
        const auto cyclic = bridge::decodeNativeValue(module, cyclicWire.value, 0, 8, &layouts);
        const bool rejectedCycle = !cyclic.succeeded() && cyclic.error.find("depth limit") != std::string::npos &&
                                   cycle.Cell()->StrongCount() == references;
        cycle.AsBoxed<Any>() = Any{};
        check(rejectedCycle, "cyclic producer Any failed bounded decoding or leaked an owning result");

        auto failing = layouts;
        failing.any.at(0).payloads.at(0).pack = &failPack;
        const auto fresh = Value::any(0, Value::any(1, Value::signedInteger(5)));
        check(!bridge::encodeNativeValue(module, fresh, 0, 8, rejected.value, error, &failing) && partialOwners == 0 &&
                  fresh.nativeAnyBacking().isEmpty() && rejected.value.kind == WIO_NATIVE_ABI_VOID,
              "failed nested pack leaked owner or published outer backing");
        failing = layouts;
        failing.any.at(0).payloads.at(0).read = &failRead;
        check(!bridge::decodeNativeValue(module, wire.value, 0, 8, &failing).succeeded() && partialOwners == 0,
              "failed nested read leaked a partial foreign output");

        auto broken = descriptors;
        broken[0].read = nullptr;
        auto invalid = api;
        invalid.payloads = broken.data();
        bridge::NativePodLayouts empty;
        check(!bridge::mergeNativeAny(module, invalid, empty, error) && empty.any.empty(),
              "invalid nested descriptor was partially published");

        const auto shared = Value::any(0, Value::any(1, Value::signedInteger(9)));
        std::array<void*, 8> states{};
        std::array<std::thread, 8> threads;
        for (std::size_t index = 0; index < threads.size(); ++index)
            threads[index] = std::thread(
                [&, index]
                {
                    Wire concurrent;
                    std::string failure;
                    if (bridge::encodeNativeValue(module, shared, 0, 8, concurrent.value, failure, &layouts))
                        states[index] = concurrent.value.payload.handle.state;
                });
        for (auto& thread : threads)
            thread.join();
        for (auto* state : states)
            check(state && state == states.front(), "concurrent nested materialization split outer identity");
    }

    void host(const char* bytecode, const char* library)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "nested Any bytecode decode failed");
        const auto entry = [&](const char* name)
        {
            const auto found = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                    { return decoded.module.string(function.name) == name; });
            check(found != decoded.module.functions.end(), "nested Any host entry missing");
            return found->id;
        };
        std::weak_ptr<bridge::NativeLibrary> lease;
        Value escaped;
        {
            std::string error;
            auto producer = bridge::NativeLibrary::open(std::filesystem::u8path(library), error);
            check(bool(producer), error.c_str());
            lease = producer;
            wio::vm::Machine machine{decoded.module};
            check(machine.loadNativeLibrary(std::filesystem::u8path(library), &error), error.c_str());
            producer.reset();
            const auto live = machine.invoke(entry("Entry"));
            if (!live.succeeded())
                throw std::runtime_error(live.error().code + ": " + live.error().message);
            check(live.value().asSignedInteger() == 0, "nested Any live-host entry failed");
            const auto failedCleanup = machine.invoke(entry("FailingRound"));
            check(!failedCleanup.succeeded() && failedCleanup.error().code == "WVM1211" &&
                      !failedCleanup.error().cleanupFailures.empty(),
                  "nested native-backed object destructor failure was swallowed");
            const auto balanced = machine.invoke(entry("LifecycleBalanced"));
            check(balanced.succeeded() && balanced.value().asBoolean(),
                  "failed nested object cleanup leaked or ran a destructor more than once");
            for (const auto* name : {"WrongLayer", "NullLayer"})
            {
                const auto invalid = machine.invoke(entry(name));
                check(!invalid.succeeded() && invalid.error().code == "WVM1184",
                      "fit any became an identity conversion or recovered a payload from null");
            }
            const auto overflow = machine.invoke(entry("Overflow"));
            check(!overflow.succeeded() && overflow.error().code == "WVM1224" &&
                      overflow.error().message.find("depth limit") != std::string::npos,
                  "producer nesting overflow did not fail deterministically");
            const auto recovered = machine.invoke(entry("Escape"));
            check(recovered.succeeded(), "nested Any host failed to recover after overflow");
            escaped = recovered.value();

            const auto anyType = std::ranges::find(decoded.module.types, std::uint8_t{19}, &wio::bytecode::Type::kind);
            const auto integerType =
                std::ranges::find(decoded.module.types, std::uint8_t{5}, &wio::bytecode::Type::kind);
            check(anyType != decoded.module.types.end() && integerType != decoded.module.types.end(),
                  "nested Any host types missing");
            const auto local =
                Value::any(static_cast<std::uint32_t>(anyType - decoded.module.types.begin()),
                           Value::any(static_cast<std::uint32_t>(integerType - decoded.module.types.begin()),
                                      Value::signedInteger(19)));
            const std::array args{local};
            const auto wrapped = machine.invoke(entry("WrapHost"), args);
            check(wrapped.succeeded() &&
                      wrapped.value().anyPayload()->anyPayload()->anyPayload()->asSignedInteger() == 19,
                  "VM-created nested Any failed the generated native thunk round-trip");
        }
        check(!lease.expired(), "escaped nested Any failed to pin producer code");
        check(escaped.kind() == Value::Kind::Any && escaped.anyPayload()->kind() == Value::Kind::Any,
              "escaped nested Any remained a foreign forwarding token");
        auto inner = *escaped.anyPayload();
        escaped = {};
        check(!lease.expired(), "dropping outer Any invalidated escaped inner producer code");
        auto payload = *inner.anyPayload();
        inner = {};
        check(payload.kind() == Value::Kind::Any && payload.anyPayload()->kind() == Value::Kind::Component,
              "escaped nested POD layers were lost");
        auto component = *payload.anyPayload();
        payload = {};
        auto backing = component.nativePodBacking();
        component = {};
        check(!lease.expired(), "nested POD prototype lost producer code before final child release");
        backing = {};
        check(lease.expired(), "nested Any producer lease leaked after final child release");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "nested Any qualification requires bytecode and producer paths");
        selection();
        contracts();
        host(argv[1], argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
