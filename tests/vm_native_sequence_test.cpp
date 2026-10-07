#include "native_sequence_internal.h"
#include <wir_sequence_abi.h>

#include <array>
#include <iostream>
#include <string>

namespace
{
    using wio::vm::Value;
    namespace bridge = wio::vm::detail;
    using Strings = wio::wir_backend::abi::SequenceAdapter<std::vector<std::string>, 101, 11>;
    using Nested = wio::wir_backend::abi::SequenceAdapter<std::vector<std::vector<std::string>>, 102, 101>;
    using Pointers = wio::wir_backend::abi::SequenceAdapter<std::vector<void*>, 103, 13>;

    bool expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << message << '\n';
        return condition;
    }

    int liveOwners = 0;
    void retain(void*) noexcept
    {
        ++liveOwners;
    }
    void release(void*) noexcept
    {
        --liveOwners;
    }
    std::uint64_t ownerType(const void*) noexcept
    {
        return 11;
    }
    const WioNativeAbiHandleOps ownerOps{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &ownerType};

    WioNativeAbiStatus failingRead(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                   WioNativeAbiFailure*) noexcept
    {
        ++liveOwners;
        *result = {};
        result->kind = WIO_NATIVE_ABI_BYTES;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->owner = {&liveOwners, &ownerOps, 1};
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    WioNativeAbiStatus excessiveCount(const WioNativeAbiValue*, std::uint64_t* result, WioNativeAbiFailure*) noexcept
    {
        *result = 1'000'001;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus failingPack(const WioNativeAbiValue* elements, std::uint64_t count, WioNativeAbiValue* result,
                                   WioNativeAbiFailure* failure) noexcept
    {
        (void)Strings::pack(elements, count, result, failure);
        return WIO_NATIVE_ABI_EXCEPTION;
    }
} // namespace

int main()
{
    wio::bytecode::Module module;
    module.types = {{.kind = 17}, {.kind = 31, .arguments = {0}}, {.kind = 31, .arguments = {1}},
                    {.kind = 20}, {.kind = 30, .arguments = {3}}, {.kind = 31, .arguments = {4}}};
    module.contract.reflection = {{.stableTypeId = 11, .type = 0},  {.stableTypeId = 101, .type = 1},
                                  {.stableTypeId = 102, .type = 2}, {.stableTypeId = 13, .type = 3},
                                  {.stableTypeId = 14, .type = 4},  {.stableTypeId = 103, .type = 5}};
    const std::array descriptors{
        WioNativeSequenceDescriptor{102, 101, &Nested::pack, &Nested::count, &Nested::read},
        WioNativeSequenceDescriptor{101, 11, &Strings::pack, &Strings::count, &Strings::read},
        WioNativeSequenceDescriptor{103, 14, &Pointers::pack, &Pointers::count, &Pointers::read}};
    WioNativeSequenceApi api{WIO_NATIVE_SEQUENCE_VERSION, static_cast<std::uint32_t>(descriptors.size()),
                             descriptors.data()};
    bridge::NativePodLayouts layouts;
    std::string error;
    bool passed = expect(bridge::mergeNativeSequences(module, api, layouts, error), error.c_str());
    passed &= expect(layouts.sequences.size() == 3, "missing nested/nullable sequence adapters");

    const Value original =
        Value::array({Value::array({Value::string(std::string{"a\0b", 3}), Value::string("")}), Value::array({})});
    WioNativeAbiValue wire;
    passed &= expect(bridge::encodeNativeValue(module, original, 2, 8, wire, error, &layouts), error.c_str());
    const auto decoded = bridge::decodeNativeValue(module, wire, 2, 8, &layouts);
    passed &= expect(decoded.succeeded() && decoded.value == original, "nested string round-trip mismatch");
    WioNativeAbiReleaseValue(&wire);
    passed &= expect(decoded.value == original, "decoded sequences borrowed released producer storage");

    passed &=
        expect(bridge::encodeNativeValue(module, *original.element(0), 1, 8, wire, error, &layouts), error.c_str());
    {
        WioNativeAbiValue reference;
        reference.kind = WIO_NATIVE_ABI_REFERENCE;
        reference.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
        reference.payload.mutableSlice = {&wire, sizeof(wire)};
        wio::wir_backend::abi::Frame frame;
        const auto& view = frame.borrow<std::vector<std::string>, 101>(reference, false);
        auto& ref = frame.borrow<std::vector<std::string>, 101>(reference, true);
        passed &= expect(&view == &ref, "sequence view/ref did not share producer storage");
        ref[0] = "updated";
        frame.commit();
        const auto owner = wire.payload.handle.state;
        frame.commit();
        passed &= expect(wire.payload.handle.state == owner, "repeated sequence commit replaced the owner");
    }
    const auto committed = bridge::decodeNativeValue(module, wire, 1, 8, &layouts);
    passed &= expect(committed.succeeded() && committed.value.element(0)->asString() == "updated",
                     "sequence mutable slot did not commit to its wire token");
    WioNativeAbiReleaseValue(&wire);

    int token = 9;
    const Value pointers = Value::array({Value::null(), Value::opaque(3, 13, &token)});
    passed &= expect(bridge::encodeNativeValue(module, pointers, 5, 8, wire, error, &layouts), error.c_str());
    const auto pointerResult = bridge::decodeNativeValue(module, wire, 5, 8, &layouts);
    passed &= expect(pointerResult.succeeded() && pointerResult.value.element(0)->kind() == Value::Kind::Null &&
                         pointerResult.value.element(1)->opaquePointer() == &token,
                     "nullable opaque array round-trip mismatch");
    WioNativeAbiReleaseValue(&wire);

    WioNativeAbiFailure failure;
    const auto wrong = wio::wir_backend::abi::Codec<std::int32_t, 11>::write(3);
    passed &= expect(Strings::pack(&wrong, 1, &wire, &failure) == WIO_NATIVE_ABI_TYPE_MISMATCH &&
                         wire.kind == WIO_NATIVE_ABI_VOID,
                     "producer accepted an invalid sequence element");
    passed &= expect(Strings::pack(nullptr, 1, &wire, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                     "producer accepted missing elements");
    passed &= expect(Strings::pack(nullptr, 0, &wire, &failure) == WIO_NATIVE_ABI_OK, "empty sequence pack failed");
    WioNativeAbiValue element;
    passed &= expect(Strings::read(&wire, 0, &element, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                     "out-of-range sequence read accepted");
    auto malformed = wire;
    malformed.typeId = 102;
    std::uint64_t count = 0;
    passed &= expect(Strings::count(&malformed, &count, &failure) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                     "wrong producer identity accepted");
    WioNativeAbiReleaseValue(&wire);

    const auto sizeBefore = layouts.sequences.size();
    auto bad = descriptors[1];
    bad.elementTypeId = 99;
    const WioNativeSequenceApi badApi{1, 1, &bad};
    passed &=
        expect(!bridge::mergeNativeSequences(module, badApi, layouts, error) && layouts.sequences.size() == sizeBefore,
               "invalid descriptor registration was not transactional");
    bad = descriptors[1];
    bad.read = &failingRead;
    const WioNativeSequenceApi conflict{1, 1, &bad};
    passed &= expect(!bridge::mergeNativeSequences(module, conflict, layouts, error),
                     "conflicting sequence adapter accepted");
    const WioNativeSequenceApi wrongVersion{99, 0, nullptr};
    passed &= expect(!bridge::mergeNativeSequences(module, wrongVersion, layouts, error),
                     "unknown sequence API version accepted");
    bridge::NativePodLayouts missing;
    const WioNativeSequenceApi missingChild{1, 1, descriptors.data()};
    passed &= expect(!bridge::mergeNativeSequences(module, missingChild, missing, error) && missing.sequences.empty(),
                     "nested sequence without element adapter accepted");
    passed &= expect(!bridge::encodeNativeValue(module, original, 2, 8, wire, error, &missing),
                     "VM array without producer adapter accepted");

    const Value strings = Value::array({Value::string("owned")});
    passed &= expect(bridge::encodeNativeValue(module, strings, 1, 8, wire, error, &layouts), error.c_str());
    auto failing = layouts;
    failing.sequences.at(1).read = &failingRead;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 1, 8, &failing).succeeded() && liveOwners == 0,
                     "partially failed element read leaked producer ownership");
    failing.sequences.at(1).count = &excessiveCount;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 1, 8, &failing).succeeded(),
                     "unbounded producer sequence count accepted");
    WioNativeAbiReleaseValue(&wire);
    failing = layouts;
    failing.sequences.at(1).pack = &failingPack;
    passed &= expect(!bridge::encodeNativeValue(module, strings, 1, 8, wire, error, &failing) &&
                         wire.kind == WIO_NATIVE_ABI_VOID,
                     "partially failed pack escaped an owned output");
    passed &= expect(!bridge::compatibleNativeSequenceCopyBack(strings, Value::array({})),
                     "length-changing copy-back accepted");
    passed &= expect(bridge::containsNativeSequencePlace(strings, *strings.element(0)),
                     "sequence cell alias was not detected");
    return passed ? 0 : 1;
}
