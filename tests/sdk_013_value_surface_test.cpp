#include <wio_sdk.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

int main()
{
    using namespace wio::sdk;

    static_assert(product_version.major == 0u && product_version.minor >= 13u);
    static_assert(feature_catalog.size() == 32u);
    static_assert(feature_info(Feature::UnicodeText)->supports(FeatureSurface::HostValue));
    static_assert(feature_info(Feature::UnicodeText)->supports(FeatureSurface::DynamicField));
    static_assert(!feature_info(Feature::ApplicationHost)->supports(FeatureSurface::HostValue));
    assert(find_feature("async-task") == Feature::AsyncTask);

    const auto text = WioText::from_utf8("İstanbul 🚀");
    assert(text.byte_count() == std::string("İstanbul 🚀").size());
    assert(text.code_point_count() == 10u);
    assert(text.at(0u).utf8() == "İ");
    assert(text.slice(9u, 1u).utf8() == "🚀");
    assert(text.slice(9u, static_cast<std::size_t>(-1)).utf8() == "🚀");
    assert(WioText::try_from_utf8(std::string("\xc0\x80", 2u)).has_value() == false);

    const auto option = WioOption<std::int32_t>::some(21);
    assert(option.is_some());
    assert(option.map([](const std::int32_t value) { return value * 2; }).value() == 42);
    assert(option.inspect([](const std::int32_t value) { assert(value == 21); }).to_array().size() == 1u);
    assert(option.zip(WioOption<std::string>::some("twenty-one")).value().second == "twenty-one");
    assert(WioOption<std::int32_t>::none().value_or(7) == 7);

    const auto result = WioResult<std::int32_t>::ok(40).map([](const std::int32_t value) { return value + 2; });
    assert(result.is_ok() && result.value() == 42);
    const auto failed = WioResult<std::int32_t>::error({ WioResultDomain::Io, 9, 22, "read failed" });
    assert(failed.is_error());
    assert(failed.error_value().domain == WioResultDomain::Io);
    assert(result.to_option().value() == 42);

    WioNullable<std::int32_t> nullable = 42;
    assert(nullable.value() == 42);

    WioTuple<std::int32_t, std::string> tuple{ 7, "seven" };
    assert(std::get<0>(tuple) == 7);

    WioQueue<std::string> queue{ "alpha", "beta" };
    queue.enqueue("gamma");
    assert(queue.count() == 3u);
    assert(queue.dequeue() == "alpha");
    assert(queue.first().value() == "beta");
    assert(queue.contains("gamma"));
    assert(queue.remove("gamma"));
    assert(queue.clone().peek() == "beta");

    WioUnorderedSet<std::int32_t> unordered{ 3, 1, 3 };
    assert(unordered.count() == 2u);
    assert(unordered.add(4));
    assert(unordered.contains(1));

    WioOrderedSet<std::int32_t> ordered{ 5, 2, 9 };
    assert(ordered.first() == 2);
    assert(ordered.last() == 9);
    bool emptyOrderedSetRejected = false;
    try { (void)WioOrderedSet<std::int32_t>{}.first(); }
    catch (const std::out_of_range&) { emptyOrderedSetRejected = true; }
    assert(emptyOrderedSetRejected);

    std::array<std::int32_t, 4> numbers{ 1, 2, 3, 4 };
    WioSpan<std::int32_t> span(numbers);
    span.slice(1u, 2u)[0u] = 20;
    assert(numbers[1] == 20);
    assert(span.at(3u) == 4);
    assert(span.first().value() == 1);
    assert(span.last().value() == 4);

    WioByteBuffer buffer(16u);
    buffer.write_u16_le(0x1234u);
    buffer.write_u32_le(0x89abcdefu);
    buffer.rewind();
    assert(buffer.try_read_u16_le().value() == 0x1234u);
    assert(buffer.try_read_u32_le().value() == 0x89abcdefu);
    assert(buffer.get(0u).value() == std::byte{ 0x34 });
    assert(buffer.clone().slice(0u, 2u).size() == 2u);

    WioBytePool pool(32u);
    const auto firstHandle = pool.rent();
    pool.at(firstHandle).write(std::byte{ 0x2a });
    assert(pool.get(firstHandle).value().count() == 1u);
    assert(pool.release(firstHandle));
    assert(!pool.owns(firstHandle));
    const auto secondHandle = pool.rent();
    assert(secondHandle.index == firstHandle.index);
    assert(secondHandle.generation != firstHandle.generation);

    WioPool<std::string> values;
    const auto valueHandle = values.rent("value");
    assert(values.get(valueHandle).value() == "value");
    assert(values.set(valueHandle, "updated"));
    assert(values.at(valueHandle) == "updated");
    assert(values.release(valueHandle));
    assert(values.get(valueHandle).is_none());

    WioBox<std::string> box("boxed");
    WioBox<std::string> boxCopy = box;
    assert(boxCopy.value() == "boxed");
    bool emptyBoxRejected = false;
    try { (void)WioBox<std::string>{}.value(); }
    catch (const std::logic_error&) { emptyBoxRejected = true; }
    assert(emptyBoxRejected);
    WioAny any(std::int32_t{ 42 });
    assert(any.is<std::int32_t>() && any.as<std::int32_t>() == 42);

    WioModuleTypeDescriptor i32Descriptor{};
    i32Descriptor.displayName = "i32";
    i32Descriptor.kind = WIO_MODULE_TYPE_DESC_PRIMITIVE;
    i32Descriptor.abiType = WIO_ABI_I32;
    i32Descriptor.stableTypeId = WioStableTypeId("i32");

    const WioModuleTypeDescriptor* optionArguments[] = { &i32Descriptor };
    WioModuleTypeDescriptor optionDescriptor{};
    optionDescriptor.displayName = "std::Option<i32>";
    optionDescriptor.logicalTypeName = "std::Option";
    optionDescriptor.kind = WIO_MODULE_TYPE_DESC_OPTION;
    optionDescriptor.stableTypeId = WioStableTypeId("std::Option<i32>");
    optionDescriptor.genericArgumentCount = 1u;
    optionDescriptor.genericArguments = optionArguments;

    TypeDescriptorView optionView(&optionDescriptor);
    assert(optionView.is_option());
    assert(optionView.generic_argument_count() == 1u);
    assert(optionView.generic_argument(0u).is_i32());
    assert(optionView.stable_id() == WioStableTypeId("std::Option<i32>"));

    WioModuleApi api{};
    api.descriptorVersion = WIO_MODULE_API_DESCRIPTOR_VERSION;
    api.capabilities = WIO_MODULE_CAP_PRODUCT_VERSION | WIO_MODULE_CAP_TYPE_METADATA_V2 | WIO_MODULE_CAP_TEXT_FIELDS;
    api.productVersion = { WIO_SDK_VERSION_MAJOR, WIO_SDK_VERSION_MINOR, WIO_SDK_VERSION_PATCH };
    api.descriptorSize = sizeof(WioModuleApi);
    const ModuleInfo info = inspect_module_api(&api);
    assert(info.product_version.has_value());
    assert(info.product_version->minor >= 13u);
    assert(info.has_capability(WIO_MODULE_CAP_TEXT_FIELDS));

    bool productMismatchRejected = false;
    api.productVersion.patch += 1u;
    try
    {
        (void)inspect_module_api(&api);
    }
    catch (const Error& error)
    {
        productMismatchRejected = error.code() == ErrorCode::InvalidApiDescriptor;
    }
    assert(productMismatchRejected);

    api.productVersion.patch = WIO_SDK_VERSION_PATCH;
    api.descriptorSize = sizeof(WioModuleApi) - 1u;
    bool truncatedDescriptorRejected = false;
    try
    {
        (void)inspect_module_api(&api);
    }
    catch (const Error& error)
    {
        truncatedDescriptorRejected = error.code() == ErrorCode::InvalidApiDescriptor;
    }
    assert(truncatedDescriptorRejected);

    return 0;
}
