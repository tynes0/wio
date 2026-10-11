#pragma once

#include <wir_object_abi.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

namespace vm_callback_string_borrow
{
    inline const std::string Bytes{"a\0\xF0\x9F\x8C\x8D", 6};
    inline const std::string Suffix{"\0z", 2};
    inline const auto Unicode = wio::runtime::Text::FromUtf8(std::string("\xC4\xB0\0\xF0\x9F\x8C\x8D", 7));
    inline const auto TextSuffix = wio::runtime::Text::FromUtf8(Suffix);

    template <class A> std::int32_t String(std::function<std::string(A, A)> callback, std::string initial)
    {
        auto value = initial;
        const auto result = callback(A::borrowView(value), A::borrow(value));
        return value == initial + Suffix && result == value ? 0 : 1;
    }
    template <class A> std::int32_t Large(std::function<std::string(A, A)> callback)
    {
        return String(callback, std::string(32768, 'x'));
    }
    template <class A> std::int32_t Reverse(std::function<std::string(A, A)> callback)
    {
        auto value = Bytes;
        const auto result = callback(A::borrow(value), A::borrowView(value));
        return value == Bytes + Suffix && result == value ? 0 : 1;
    }
    template <class A> std::int32_t Text(std::function<wio::runtime::Text(A, A)> callback, wio::runtime::Text initial)
    {
        auto value = initial;
        const auto expected = initial.Utf8() + Suffix;
        const auto result = callback(A::borrowView(value), A::borrow(value));
        return value.Utf8() == expected && result.Utf8() == expected ? 0 : 1;
    }
    inline std::function<std::int32_t()> storedString;
    inline std::function<std::int32_t()> storedText;
    template <class A> void RememberString(std::function<std::string(A, A)> callback)
    {
        storedString = [callback] { return String(callback, Bytes) + Large(callback); };
    }
    template <class A> void RememberText(std::function<wio::runtime::Text(A, A)> callback)
    {
        storedText = [callback] { return Text(callback, Unicode); };
    }
    inline std::int32_t Run()
    {
        return storedString() + storedText();
    }
    inline void Clear()
    {
        storedString = {};
        storedText = {};
    }
    template <class R, class A, class B> void Capture(std::function<R(A, B)>)
    {
    }

    inline const WioNativeAbiCallbackOps PairOps{
        WIO_NATIVE_ABI_VERSION, 0, nullptr, nullptr,
        [](void* state, const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
           WioNativeAbiFailure*) noexcept
        {
            if (count != 2)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            auto* bytes = static_cast<WioNativeAbiValue*>(args[0].payload.mutableSlice.data);
            auto* unicode = static_cast<WioNativeAbiValue*>(args[1].payload.mutableSlice.data);
            WioNativeAbiReleaseValue(bytes);
            *bytes = wio::wir_backend::abi::Codec<std::string, 101>::write("changed");
            const auto mode = *static_cast<unsigned*>(state);
            if (mode == 2)
                bytes->typeId = 102;
            WioNativeAbiReleaseValue(unicode);
            const std::uint32_t invalid = 0xD800;
            static const std::uint32_t bad[] = {invalid};
            static const std::uint32_t good[] = {0x1F30D};
            unicode->kind = WIO_NATIVE_ABI_TEXT_UTF32;
            unicode->typeId = mode == 3 ? 101 : 102;
            unicode->flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            unicode->payload.slice = {mode != 0 ? good : bad, sizeof(bad)};
            *result = {};
            result->kind = WIO_NATIVE_ABI_I64;
            result->payload.signedInteger = 9;
            return WIO_NATIVE_ABI_OK;
        }};
    template <class A, class B> std::int32_t AtomicProbe(std::function<std::int32_t(A, B)>)
    {
        using Codec =
            wio::wir_backend::abi::CallbackCodecWithBorrows<std::function<std::int32_t(A, B)>, 100, 103,
                                                            std::integer_sequence<bool, true, true>, 101, 102>;
        unsigned mode = 0;
        WioNativeAbiValue wire{};
        wire.kind = WIO_NATIVE_ABI_CALLBACK;
        wire.payload.callback = {&mode, &PairOps};
        auto callback = Codec::read(wire);
        std::string bytes = "original";
        auto unicode = wio::runtime::Text::FromUtf8("original");
        try
        {
            (void)callback(A::borrow(bytes), B::borrow(unicode));
            return 1;
        }
        catch (const wio::wir_backend::abi::Error&)
        {
            if (bytes != "original" || unicode.Utf8() != "original")
                return 2;
        }
        for (mode = 2; mode <= 3; ++mode)
        {
            try
            {
                (void)callback(A::borrow(bytes), B::borrow(unicode));
                return 4;
            }
            catch (const wio::wir_backend::abi::Error&)
            {
                if (bytes != "original" || unicode.Utf8() != "original")
                    return 5;
            }
        }
        mode = 1;
        return callback(A::borrow(bytes), B::borrow(unicode)) == 9 && bytes == "changed" &&
                       unicode.Utf8() == "\xF0\x9F\x8C\x8D"
                   ? 0
                   : 3;
    }

    template <class R, class C, class A> A parameter(R (C::*)(A));
    template <class Signature> struct Arguments;
    template <class R, class A, class B> struct Arguments<std::function<R(A, B)>>
    {
        using Cell = A;
    };
    template <class T> std::int32_t Hosted(wio::runtime::Ref<T> processor)
    {
        using StringCallback = decltype(parameter(&T::_WF_ApplyString_function));
        using StringCell = typename Arguments<StringCallback>::Cell;
        StringCallback bytes = [](StringCell readonly, StringCell writable)
        {
            writable.write(readonly.read() + Suffix);
            return readonly.read();
        };
        if (processor->_WF_ApplyString_function(bytes) != Bytes + Suffix ||
            processor->_WF_Forwarded_function(bytes) != 0)
            return 1;
        using TextCallback = decltype(parameter(&T::_WF_ApplyText_function));
        using TextCell = typename Arguments<TextCallback>::Cell;
        TextCallback text = [](TextCell readonly, TextCell writable)
        {
            writable.write(wio::runtime::Text::FromUtf8(readonly.read().Utf8() + Suffix));
            return readonly.read();
        };
        return processor->_WF_ApplyText_function(text).Utf8() == Unicode.Utf8() + Suffix ? 0 : 2;
    }

    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> hostLease;
    template <class T> std::uint64_t CaptureHost(wio::runtime::Ref<T> processor)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(processor.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        capturedHost = proxy->hostLease().host();
        hostLease = [processor] {};
        return reinterpret_cast<std::uintptr_t>(&capturedHost);
    }
    inline void ClearHost()
    {
        hostLease = {};
        capturedHost = {};
    }
    inline void Print()
    {
        std::cout << "native-callback-string-borrow-ok:30\n";
    }
} // namespace vm_callback_string_borrow
