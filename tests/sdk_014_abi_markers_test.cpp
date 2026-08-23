#include <wio_sdk.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>

int main()
{
    using namespace wio::sdk;

    static_assert(WioU8::abi_type == WIO_ABI_U8);
    static_assert(WioUChar::abi_type == WIO_ABI_UCHAR);
    static_assert(WioI64::abi_type == WIO_ABI_I64);
    static_assert(WioISize::abi_type == WIO_ABI_ISIZE);
    static_assert(WioU64::abi_type == WIO_ABI_U64);
    static_assert(WioUSize::abi_type == WIO_ABI_USIZE);
    static_assert(!std::is_same_v<WioU64, WioUSize>);
    static_assert(!std::is_same_v<WioU8, WioUChar>);

    const WioValue fixed64 = detail::toWioValue(WioU64{0xfedcba9876543210ull});
    const WioValue pointer64 = detail::toWioValue(WioUSize{static_cast<std::uintptr_t>(0x1234u)});
    const WioValue fixed8 = detail::toWioValue(WioU8{0xabu});
    const WioValue character8 = detail::toWioValue(WioUChar{static_cast<unsigned char>('W')});
    const WioValue fixedSigned = detail::toWioValue(WioI64{-42});
    const WioValue pointerSigned = detail::toWioValue(WioISize{static_cast<std::intptr_t>(-7)});

    assert(fixed64.type == WIO_ABI_U64 && fixed64.value.v_u64 == 0xfedcba9876543210ull);
    assert(pointer64.type == WIO_ABI_USIZE && pointer64.value.v_usize == static_cast<std::uintptr_t>(0x1234u));
    assert(fixed8.type == WIO_ABI_U8 && fixed8.value.v_u8 == 0xabu);
    assert(character8.type == WIO_ABI_UCHAR && character8.value.v_uchar == static_cast<unsigned char>('W'));
    assert(fixedSigned.type == WIO_ABI_I64 && fixedSigned.value.v_i64 == -42);
    assert(pointerSigned.type == WIO_ABI_ISIZE && pointerSigned.value.v_isize == static_cast<std::intptr_t>(-7));

    assert(detail::fromWioValue<WioU64>(fixed64).value() == 0xfedcba9876543210ull);
    assert(detail::fromWioValue<WioUSize>(pointer64).value() == static_cast<std::uintptr_t>(0x1234u));
    assert(detail::fromWioValue<WioU8>(fixed8).value() == 0xabu);
    assert(detail::fromWioValue<WioUChar>(character8).value() == static_cast<unsigned char>('W'));
    assert(detail::fromWioValue<WioI64>(fixedSigned).value() == -42);
    assert(detail::fromWioValue<WioISize>(pointerSigned).value() == static_cast<std::intptr_t>(-7));

    assert(detail::hostFieldTypeName<WioU64>() == "u64");
    assert(detail::hostFieldTypeName<WioUSize>() == "usize");
    assert(detail::hostFieldTypeName<WioU8>() == "u8");
    assert(detail::hostFieldTypeName<WioUChar>() == "uchar");

    WioModuleTypeDescriptor u64Descriptor{};
    u64Descriptor.displayName = "u64";
    u64Descriptor.kind = WIO_MODULE_TYPE_DESC_PRIMITIVE;
    u64Descriptor.abiType = WIO_ABI_U64;
    assert(detail::matchesTypeDescriptor<WioU64>(TypeDescriptorView(&u64Descriptor)));
    assert(!detail::matchesTypeDescriptor<WioUSize>(TypeDescriptorView(&u64Descriptor)));

    bool mismatchRejected = false;
    try
    {
        (void)detail::fromWioValue<WioUSize>(fixed64);
    }
    catch (const Error& error)
    {
        mismatchRejected = error.code() == ErrorCode::SignatureMismatch;
    }
    assert(mismatchRejected);

    return 0;
}
