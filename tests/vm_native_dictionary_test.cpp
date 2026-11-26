#include "native_dictionary_internal.h"
#include "native_library.h"
#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wir_dictionary_abi.h>
#include <wir_sequence_abi.h>

#include <array>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <unordered_map>

namespace
{
    using wio::vm::Value;
    namespace bridge = wio::vm::detail;
    using Map = std::unordered_map<std::string, std::int32_t>;
    using Maps = wio::wir_backend::abi::DictionaryAdapter<Map, 201, 12, 11>;
    using Trees = wio::wir_backend::abi::DictionaryAdapter<std::map<std::int32_t, std::string>, 202, 11, 12>;
    using Rows = wio::wir_backend::abi::DictionaryAdapter<std::unordered_map<std::string, std::vector<std::int32_t>>,
                                                          203, 12, 301>;
    using Integers = wio::wir_backend::abi::SequenceAdapter<std::vector<std::int32_t>, 301, 11>;
    using Batches = wio::wir_backend::abi::SequenceAdapter<std::vector<Map>, 302, 201>;

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
    std::uint64_t typeId(const void*) noexcept
    {
        return 12;
    }
    const WioNativeAbiHandleOps operations{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &typeId};

    WioNativeAbiStatus partialRead(const WioNativeAbiValue*, WioNativeAbiValue* entries, std::uint64_t count,
                                   WioNativeAbiFailure*) noexcept
    {
        if (count)
        {
            ++owners;
            entries[0].kind = WIO_NATIVE_ABI_BYTES;
            entries[0].flags = WIO_NATIVE_ABI_VALUE_OWNED;
            entries[0].owner = {&owners, &operations, 1};
        }
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    WioNativeAbiStatus duplicateRead(const WioNativeAbiValue*, WioNativeAbiValue* entries, std::uint64_t count,
                                     WioNativeAbiFailure*) noexcept
    {
        for (std::uint64_t index = 0; index < count; ++index)
        {
            entries[index * 2] = wio::wir_backend::abi::Codec<std::string, 12>::write("duplicate");
            entries[index * 2 + 1] = wio::wir_backend::abi::Codec<std::int32_t, 11>::write(3);
        }
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus oversized(const WioNativeAbiValue*, std::uint64_t* count, WioNativeAbiFailure*) noexcept
    {
        *count = 1'000'001;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus partialPack(const WioNativeAbiValue* entries, std::uint64_t count, WioNativeAbiValue* result,
                                   WioNativeAbiFailure* failure) noexcept
    {
        (void)Maps::pack(entries, count, result, failure);
        return WIO_NATIVE_ABI_EXCEPTION;
    }

    bool qualifyLibraryLifetime(const char* bytecodePath, const char* libraryPath)
    {
        std::ifstream input(std::filesystem::u8path(bytecodePath), std::ios::binary);
        const std::vector<char> bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        auto decoded = wio::bytecode::decode({reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()});
        if (!expect(decoded.succeeded(), "lifetime bytecode decode failed"))
            return false;
        std::uint32_t escape = wio::bytecode::InvalidIndex;
        for (const auto& function : decoded.module.functions)
            if (decoded.module.string(function.name) == "Escape")
                escape = function.id;
        if (!expect(escape != wio::bytecode::InvalidIndex, "lifetime fixture function missing"))
            return false;
        std::string error;
        Value escaped;
        std::weak_ptr<bridge::NativeLibrary> lease;
        {
            auto producer = bridge::NativeLibrary::open(std::filesystem::u8path(libraryPath), error);
            if (!expect(bool(producer), "lifetime producer load failed"))
                return false;
            lease = producer;
            wio::vm::Machine machine{decoded.module};
            if (!expect(machine.loadNativeLibrary(std::filesystem::u8path(libraryPath), &error),
                        "lifetime producer registration failed"))
                return false;
            producer.reset();
            const auto result = machine.invoke(escape);
            if (!expect(result.succeeded() && result.value().kind() == Value::Kind::Dictionary,
                        "lifetime dictionary result failed"))
                return false;
            escaped = result.value();
        }
        if (!expect(!lease.expired(), "escaped dictionary released producer code while POD backing is alive"))
            return false;
        Value child = *escaped.dictionaryValue(Value::string("sample"));
        escaped = {};
        if (!expect(!lease.expired(), "escaped POD child lost its producer lease"))
            return false;
        child = {};
        return expect(lease.expired(), "dictionary POD backing leaked its producer library lease");
    }
} // namespace

int main(int argc, char** argv)
{
    wio::bytecode::Module module;
    module.strings = {"unordered", "ordered"};
    module.types = {{.kind = 5},
                    {.kind = 17},
                    {.kind = 32, .name = 0, .arguments = {1, 0}},
                    {.kind = 32, .name = 1, .arguments = {0, 1}},
                    {.kind = 31, .arguments = {0}},
                    {.kind = 32, .name = 0, .arguments = {1, 4}},
                    {.kind = 31, .arguments = {2}}};
    module.contract.reflection = {{.stableTypeId = 11, .type = 0},  {.stableTypeId = 12, .type = 1},
                                  {.stableTypeId = 201, .type = 2}, {.stableTypeId = 202, .type = 3},
                                  {.stableTypeId = 301, .type = 4}, {.stableTypeId = 203, .type = 5},
                                  {.stableTypeId = 302, .type = 6}};
    const std::array descriptors{
        WioNativeDictionaryDescriptor{201, 12, 11, 0, &Maps::pack, &Maps::count, &Maps::read},
        WioNativeDictionaryDescriptor{202, 11, 12, WIO_NATIVE_DICTIONARY_ORDERED, &Trees::pack, &Trees::count,
                                      &Trees::read},
        WioNativeDictionaryDescriptor{203, 12, 301, 0, &Rows::pack, &Rows::count, &Rows::read}};
    const WioNativeDictionaryApi api{1, 3, descriptors.data()};
    const std::array sequenceDescriptors{
        WioNativeSequenceDescriptor{301, 11, &Integers::pack, &Integers::count, &Integers::read},
        WioNativeSequenceDescriptor{302, 201, &Batches::pack, &Batches::count, &Batches::read}};
    const WioNativeSequenceApi sequences{1, 2, sequenceDescriptors.data()};
    bridge::NativePodLayouts layouts;
    std::string error;
    bool passed =
        expect(bridge::mergeNativeSequences(module, sequences, layouts, error, false), "sequence staging failed");
    passed &=
        expect(bridge::mergeNativeDictionaries(module, api, layouts, error), "cross-container registration failed");

    const Value original = Value::dictionary(
        2,
        {Value::string(std::string{"a\0b", 3}), Value::signedInteger(7), Value::string(""), Value::signedInteger(11)},
        false);
    WioNativeAbiValue wire;
    passed &= expect(bridge::encodeNativeValue(module, original, 2, 8, wire, error, &layouts), "map encode failed");
    const auto decoded = bridge::decodeNativeValue(module, wire, 2, 8, &layouts);
    passed &= expect(decoded.succeeded() && decoded.value == original, "unordered map round-trip mismatch");
    WioNativeAbiReleaseValue(&wire);
    passed &= expect(decoded.value == original, "decoded map depended on released producer storage");

    passed &=
        expect(bridge::encodeNativeValue(module, original, 2, 8, wire, error, &layouts), "ref fixture encode failed");
    {
        WioNativeAbiValue reference;
        reference.kind = WIO_NATIVE_ABI_REFERENCE;
        reference.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
        reference.payload.mutableSlice = {&wire, sizeof(wire)};
        wio::wir_backend::abi::Frame frame;
        const auto& view = frame.borrow<Map, 201>(reference, false);
        auto& ref = frame.borrow<Map, 201>(reference, true);
        passed &= expect(&view == &ref, "map view/ref did not share the producer slot");
        ref.at("") = 41;
        frame.commit();
        const auto owner = wire.payload.handle.state;
        frame.commit();
        passed &= expect(owner == wire.payload.handle.state, "repeated map commit replaced producer ownership");
    }
    const auto committed = bridge::decodeNativeValue(module, wire, 2, 8, &layouts);
    passed &=
        expect(committed.succeeded() && committed.value.dictionaryValue(Value::string(""))->asSignedInteger() == 41,
               "map ref slot failed to commit");
    WioNativeAbiReleaseValue(&wire);

    const Value rows = Value::dictionary(5, {Value::string("row"), Value::array({Value::signedInteger(17)})}, false);
    const Value batches = Value::array({original, Value::dictionary(2, {}, false)});
    for (const auto& [type, input] : std::array<std::pair<std::uint32_t, Value>, 2>{{{5, rows}, {6, batches}}})
    {
        passed &= expect(bridge::encodeNativeValue(module, input, type, 8, wire, error, &layouts),
                         "nested container encode failed");
        const auto result = bridge::decodeNativeValue(module, wire, type, 8, &layouts);
        passed &= expect(result.succeeded() && result.value == input, "nested container round-trip mismatch");
        WioNativeAbiReleaseValue(&wire);
    }

    const Value ordered = Value::dictionary(
        3, {Value::signedInteger(9), Value::string("nine"), Value::signedInteger(1), Value::string("one")}, true);
    passed &=
        expect(bridge::encodeNativeValue(module, ordered, 3, 8, wire, error, &layouts), "ordered map encode failed");
    const auto tree = bridge::decodeNativeValue(module, wire, 3, 8, &layouts);
    passed &= expect(tree.succeeded() && tree.value.dictionaryKey(0)->asSignedInteger() == 1 && tree.value == ordered,
                     "ordered result lost sorted key order");
    WioNativeAbiReleaseValue(&wire);

    std::vector<std::size_t> order;
    const Value reordered = Value::dictionary(
        2,
        {Value::string(""), Value::signedInteger(29), Value::string(std::string{"a\0b", 3}), Value::signedInteger(31)},
        false);
    passed &= expect(bridge::nativeDictionaryCopyBackOrder(original, reordered, order) &&
                         order == std::vector<std::size_t>{1, 0},
                     "copy-back used iteration position instead of key identity");
    const Value renamed = Value::dictionary(
        2, {Value::string("renamed"), Value::signedInteger(7), Value::string(""), Value::signedInteger(11)}, false);
    passed &= expect(!bridge::compatibleNativeSequenceCopyBack(original, renamed), "same-size key mutation accepted");
    const Value grownRow = Value::dictionary(
        5, {Value::string("row"), Value::array({Value::signedInteger(1), Value::signedInteger(2)})}, false);
    passed &= expect(!bridge::compatibleNativeSequenceCopyBack(rows, grownRow), "nested array resize accepted");
    passed &= expect(bridge::containsNativeSequencePlace(original, *original.dictionaryValue(0)) &&
                         bridge::containsNativeSequencePlace(rows, *rows.dictionaryValue(0)->element(0)),
                     "dictionary descendant alias missed");

    auto bad = descriptors[0];
    bad.valueTypeId = 999;
    WioNativeDictionaryApi badApi{1, 1, &bad};
    const auto before = layouts.dictionaries.size();
    passed &= expect(!bridge::mergeNativeDictionaries(module, badApi, layouts, error) &&
                         layouts.dictionaries.size() == before,
                     "invalid metadata registration was not atomic");
    bad = descriptors[0];
    bad.flags = WIO_NATIVE_DICTIONARY_ORDERED;
    passed &=
        expect(!bridge::mergeNativeDictionaries(module, badApi, layouts, error), "unordered/ordered mismatch accepted");
    bad = descriptors[0];
    bad.flags = 2;
    passed &= expect(!bridge::mergeNativeDictionaries(module, badApi, layouts, error), "unknown map flags accepted");
    bad = descriptors[0];
    bad.read = &partialRead;
    passed &=
        expect(!bridge::mergeNativeDictionaries(module, badApi, layouts, error), "conflicting map adapter accepted");
    const WioNativeDictionaryApi invalidVersion{99, 0, nullptr};
    passed &= expect(!bridge::mergeNativeDictionaries(module, invalidVersion, layouts, error),
                     "unknown map API version accepted");
    bridge::NativePodLayouts missing;
    const WioNativeDictionaryApi missingArray{1, 1, &descriptors[2]};
    passed &=
        expect(!bridge::mergeNativeDictionaries(module, missingArray, missing, error) && missing.dictionaries.empty(),
               "map without nested array adapter accepted");
    passed &= expect(!bridge::encodeNativeValue(module, original, 2, 8, wire, error, &missing),
                     "VM map without adapter accepted");

    passed &= expect(bridge::encodeNativeValue(module, original, 2, 8, wire, error, &layouts),
                     "failure fixture encode failed");
    auto failing = layouts;
    failing.dictionaries.at(2).read = &partialRead;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 2, 8, &failing).succeeded() && owners == 0,
                     "partially filled dictionary read leaked owned cells");
    failing.dictionaries.at(2).read = &duplicateRead;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 2, 8, &failing).succeeded(),
                     "duplicate producer keys accepted");
    failing.dictionaries.at(2).count = &oversized;
    passed &= expect(!bridge::decodeNativeValue(module, wire, 2, 8, &failing).succeeded(),
                     "unbounded producer entry count accepted");
    WioNativeAbiReleaseValue(&wire);
    failing = layouts;
    failing.dictionaries.at(2).pack = &partialPack;
    passed &= expect(!bridge::encodeNativeValue(module, original, 2, 8, wire, error, &failing) &&
                         wire.kind == WIO_NATIVE_ABI_VOID,
                     "failed map pack escaped an owned output");

    WioNativeAbiFailure failure;
    std::array<WioNativeAbiValue, 4> entries{wio::wir_backend::abi::Codec<std::string, 12>::write("key"),
                                             wio::wir_backend::abi::Codec<std::int32_t, 11>::write(1),
                                             wio::wir_backend::abi::Codec<std::string, 12>::write("key"),
                                             wio::wir_backend::abi::Codec<std::int32_t, 11>::write(2)};
    passed &= expect(Maps::pack(entries.data(), 2, &wire, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                     "producer silently accepted duplicate keys");
    passed &= expect(Maps::pack(nullptr, 1, &wire, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                     "producer accepted missing entry buffer");
    for (auto& entry : entries)
        WioNativeAbiReleaseValue(&entry);
    passed &= expect(Maps::pack(nullptr, 0, &wire, &failure) == WIO_NATIVE_ABI_OK, "empty map pack failed");
    passed &= expect(Maps::read(&wire, nullptr, 1, &failure) == WIO_NATIVE_ABI_INVALID_ARGUMENT,
                     "producer accepted an incorrect read buffer count");
    WioNativeAbiReleaseValue(&wire);

    std::vector<Value> large;
    for (std::int32_t index = 0; index < 256; ++index)
    {
        large.push_back(Value::string(std::to_string(index % 64)));
        large.push_back(Value::signedInteger(index));
    }
    const Value unique = Value::dictionary(2, std::move(large), false);
    passed &=
        expect(unique.dictionaryCount() == 64 && unique.dictionaryValue(Value::string("7"))->asSignedInteger() == 7,
               "bulk dictionary construction lost first-key-wins semantics");
    if (argc == 3)
        passed &= qualifyLibraryLifetime(argv[1], argv[2]);
    return passed ? 0 : 1;
}
