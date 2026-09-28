#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace wio::bytecode
{
    inline constexpr std::array<std::byte, 8> Magic{
        std::byte{'W'}, std::byte{'I'}, std::byte{'O'}, std::byte{'B'},
        std::byte{'C'}, std::byte{0x0d}, std::byte{0x0a}, std::byte{0x1a}};
    inline constexpr std::uint16_t FormatMajor = 1;
    inline constexpr std::uint16_t FormatMinor = 0;
    inline constexpr std::uint32_t LittleEndianMarker = 0x01020304u;
    inline constexpr std::uint32_t InvalidIndex = (std::numeric_limits<std::uint32_t>::max)();
    inline constexpr std::uint32_t HeaderSize = 48;
    inline constexpr std::uint32_t SectionEntrySize = 32;

    enum class SectionKind : std::uint32_t
    {
        Manifest = 1,
        Strings = 2,
        Constants = 3,
        Types = 4,
        Globals = 5,
        Functions = 6,
        Code = 7,
        ModuleContract = 8,
        Debug = 9
    };

    enum class ConstantKind : std::uint8_t
    {
        Empty = 0,
        Null = 1,
        Boolean = 2,
        SignedInteger = 3,
        UnsignedInteger = 4,
        Float64 = 5,
        String = 6
    };

    // Stable bytecode opcodes. Values are file-format ABI and must never be
    // reordered or reused. New operations are appended with new values.
    enum class Opcode : std::uint16_t
    {
        Constant = 0x0000,
        GenericConstant = 0x0001,
        DefaultValue = 0x0002,
        Unary = 0x0003,
        Binary = 0x0004,
        RangeContains = 0x0005,
        Convert = 0x0006,
        Call = 0x0007,
        NativeInvoke = 0x0008,
        FunctionReference = 0x0009,
        ClosureCreate = 0x000a,
        IndirectCall = 0x000b,
        ExtensionCall = 0x000c,
        MethodCall = 0x000d,
        VirtualCall = 0x000e,
        InterfaceCall = 0x000f,
        Upcast = 0x0010,
        CheckedCast = 0x0011,
        TypeTest = 0x0012,
        IdentityEqual = 0x0013,
        VariantTest = 0x0014,
        VariantPayload = 0x0015,
        ArrayLength = 0x0016,
        ArrayElement = 0x0017,
        ArrayCreate = 0x0018,
        ArrayGet = 0x0019,
        DictionaryCreate = 0x001a,
        DictionaryGet = 0x001b,
        DictionaryPlace = 0x001c,
        Interpolate = 0x001d,
        EnumConstant = 0x001e,
        IntrinsicCall = 0x001f,
        AnyBox = 0x0020,
        AnyCheckedCast = 0x0021,
        AnyTypeTest = 0x0022,
        NullableWrap = 0x0023,
        NullableUnwrap = 0x0024,
        IteratorCreate = 0x0025,
        IteratorHasNext = 0x0026,
        IteratorValue = 0x0027,
        IteratorAdvance = 0x0028,
        ResultIsError = 0x0029,
        ResultValue = 0x002a,
        ResultUnwrap = 0x002b,
        ResultPropagate = 0x002c,
        CancellationCheck = 0x002d,
        CoroutineSuspend = 0x002e,
        CoroutineResume = 0x002f,
        CoroutineComplete = 0x0030,
        GlobalPlace = 0x0031,
        LocalPlace = 0x0032,
        PlaceInit = 0x0033,
        Load = 0x0034,
        Store = 0x0035,
        FieldPlace = 0x0036,
        ArrayPlace = 0x0037,
        Borrow = 0x0038,
        ConstructComponent = 0x0039,
        ConstructObject = 0x003a,
        Retain = 0x003b,
        CopyValue = 0x003c,
        MoveValue = 0x003d,
        Replace = 0x003e,
        Release = 0x003f,
        DropValue = 0x0040,
        ReleasePlace = 0x0041,
        DropPlace = 0x0042,
        Return = 0x0043,
        Jump = 0x0044,
        CondJump = 0x0045,
        Unreachable = 0x0046
    };

    [[nodiscard]] std::string_view sectionKindName(SectionKind kind) noexcept;
    [[nodiscard]] std::string_view opcodeName(Opcode opcode) noexcept;
    [[nodiscard]] bool isKnownOpcode(Opcode opcode) noexcept;
    [[nodiscard]] bool isTerminator(Opcode opcode) noexcept;
    [[nodiscard]] bool producesValue(Opcode opcode) noexcept;
} // namespace wio::bytecode
