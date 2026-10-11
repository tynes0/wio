#include "wio/bytecode/codec.h"
#include "wio/vm/machine.h"
#include <wio_native_object.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    void check(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    struct Library
    {
#if defined(_WIN32)
        HMODULE handle;
        explicit Library(const char* path) : handle(LoadLibraryW(std::filesystem::u8path(path).c_str()))
        {
        }
        ~Library()
        {
            if (handle)
                FreeLibrary(handle);
        }
        template <class T> T symbol(const char* name)
        {
            const auto address = GetProcAddress(handle, name);
#else
        void* handle;
        explicit Library(const char* path) : handle(dlopen(path, RTLD_NOW | RTLD_LOCAL))
        {
        }
        ~Library()
        {
            if (handle)
                dlclose(handle);
        }
        template <class T> T symbol(const char* name)
        {
            const auto address = dlsym(handle, name);
#endif
            check(address != nullptr, name);
            return reinterpret_cast<T>(address);
        }
    };
    struct Wire
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };
    std::string bytes(const WioNativeAbiValue& value)
    {
        check(value.kind == WIO_NATIVE_ABI_BYTES, "expected byte slice");
        return value.payload.slice.size ? std::string(static_cast<const char*>(value.payload.slice.data),
                                                      static_cast<std::size_t>(value.payload.slice.size))
                                        : std::string{};
    }
    std::u32string text(const WioNativeAbiValue& value)
    {
        check(value.kind == WIO_NATIVE_ABI_TEXT_UTF32, "expected UTF-32 slice");
        const auto count = value.payload.slice.size / sizeof(char32_t);
        std::u32string result(count, U'\0');
        if (count)
            std::memcpy(result.data(), value.payload.slice.data, value.payload.slice.size);
        return result;
    }
    struct Callback
    {
        WioNativeAbiCallback value{};
        void clear()
        {
            if (value.ops)
                value.ops->release(value.userdata);
            value = {};
        }
        ~Callback()
        {
            clear();
        }
    } captured;
    WioNativeAbiStatus capture(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
                               WioNativeAbiFailure*) noexcept
    {
        if (count != 1 || args[0].kind != WIO_NATIVE_ABI_CALLBACK)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        captured.clear();
        captured.value = args[0].payload.callback;
        captured.value.ops->retain(captured.value.userdata);
        *result = {};
        return WIO_NATIVE_ABI_OK;
    }
    struct Owner
    {
        std::uint64_t typeId;
        unsigned references = 1;
        unsigned releases = 0;
        static const WioNativeAbiHandleOps Ops;
    };
    const WioNativeAbiHandleOps Owner::Ops{
        WIO_NATIVE_ABI_VERSION, 0, [](void* state) noexcept { ++static_cast<Owner*>(state)->references; },
        [](void* state) noexcept
        {
            auto& owner = *static_cast<Owner*>(state);
            --owner.references;
            ++owner.releases;
        },
        [](const void* state) noexcept { return static_cast<const Owner*>(state)->typeId; }};
    struct NativePair
    {
        unsigned references = 1;
        unsigned behavior = 0;
        static const WioNativeAbiCallbackOps Ops;
    };
    const WioNativeAbiCallbackOps NativePair::Ops{
        WIO_NATIVE_ABI_VERSION, WIO_NATIVE_ABI_CALLBACK_CALLER_THREAD,
        [](void* state) noexcept { ++static_cast<NativePair*>(state)->references; },
        [](void* state) noexcept { --static_cast<NativePair*>(state)->references; },
        [](void* state, const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
           WioNativeAbiFailure*) noexcept
        {
            if (count != 2 || args[0].kind != WIO_NATIVE_ABI_REFERENCE || args[1].kind != WIO_NATIVE_ABI_REFERENCE)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            auto* first = static_cast<WioNativeAbiValue*>(args[0].payload.mutableSlice.data);
            auto* second = static_cast<WioNativeAbiValue*>(args[1].payload.mutableSlice.data);
            const auto stringId = first->typeId;
            const auto textId = second->typeId;
            WioNativeAbiReleaseValue(first);
            WioNativeAbiReleaseValue(second);
            first->kind = WIO_NATIVE_ABI_BYTES;
            const auto behavior = static_cast<NativePair*>(state)->behavior;
            first->typeId = behavior == 2 ? textId : stringId;
            first->flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            first->payload.slice = {"changed", 7};
            static const char32_t bad[] = {0xD800};
            static const char32_t good[] = {0x1F30D};
            second->kind = WIO_NATIVE_ABI_TEXT_UTF32;
            second->typeId = behavior == 3 ? stringId : textId;
            second->flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            second->payload.slice = {behavior != 0 ? good : bad, sizeof(bad)};
            *result = {};
            result->kind = WIO_NATIVE_ABI_I64;
            result->payload.signedInteger = 9;
            return WIO_NATIVE_ABI_OK;
        }};

    void host(const char* path, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
        const std::string data{std::istreambuf_iterator<char>{input}, {}};
        const auto decoded = wio::bytecode::decode(std::as_bytes(std::span{data.data(), data.size()}));
        check(decoded.succeeded(), "string borrow bytecode decode failed");
        const auto& module = decoded.module;
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(module.functions,
                                                    [&](const auto& item) { return module.string(item.name) == name; });
            check(found != module.functions.end(), name);
            return found->id;
        };
        const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& item)
                                                    { return module.string(item.logicalName) == "Processor"; });
        check(reflected != module.contract.reflection.end(), "processor contract missing");
        const auto method = [&](const char* name) -> const wio::bytecode::ReflectedMethod&
        {
            const auto found = std::ranges::find_if(reflected->methods,
                                                    [&](const auto& item) { return module.string(item.name) == name; });
            check(found != reflected->methods.end(), name);
            return *found;
        };
        const auto identity = [&](std::uint32_t type)
        {
            const auto found = std::ranges::find(module.contract.reflection, type, &wio::bytecode::Reflection::type);
            check(found != module.contract.reflection.end(), "borrow identity missing");
            return found->stableTypeId;
        };
        const auto stringId =
            identity(module.types[module.functions[function("ModifyString")].parameters[0].type].arguments[0]);
        const auto textId =
            identity(module.types[module.functions[function("ModifyText")].parameters[0].type].arguments[0]);
        Wire escapedString;
        Wire escapedText;
        {
            Library library{producer};
            check(library.handle != nullptr, "string borrow producer load failed");
            const auto registry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry")();
            const auto objects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi")();
            std::vector<WioNativeAbiFunctionDescriptor> descriptors(registry->functions,
                                                                    registry->functions + registry->functionCount);
            for (const auto* name : {"CaptureString", "CaptureText"})
            {
                const auto key = module.string(module.functions[function(name)].nativeBinding.stableKey);
                const auto found =
                    std::ranges::find_if(descriptors, [&](const auto& item) { return item.stableKey == key; });
                check(found != descriptors.end(), "string capture descriptor missing");
                found->thunk = &capture;
            }
            auto changed = *registry;
            changed.functions = descriptors.data();
            {
                wio::vm::Machine machine{module};
                std::string error;
                check(machine.registerNativeRegistry(changed, *objects, &error), error.c_str());
                const auto safe = [&]
                { check(machine.invoke(function("SafePoint")).succeeded(), "callback recovery failed"); };
                const auto invoke = [&](WioNativeAbiValue* args, Wire& result)
                {
                    WioNativeAbiFailure failure{};
                    return captured.value.ops->invoke(captured.value.userdata, args, 2, &result.value, &failure);
                };
                const auto references = [](Wire& token)
                {
                    std::array<WioNativeAbiValue, 2> args{};
                    for (auto& value : args)
                    {
                        value.kind = WIO_NATIVE_ABI_REFERENCE;
                        value.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                        value.typeId = token.value.typeId;
                        value.payload.mutableSlice = {&token.value, sizeof(WioNativeAbiValue)};
                    }
                    args[1].flags |= WIO_NATIVE_ABI_VALUE_MUTABLE;
                    return args;
                };
                const std::string original{"a\0\xF0\x9F\x8C\x8D", 6};
                const std::string suffix{"\0z", 2};
                check(machine.invoke(function("HostString")).succeeded(), "string capture failed");
                Owner owner{stringId};
                Wire token;
                token.value.kind = WIO_NATIVE_ABI_BYTES;
                token.value.typeId = stringId;
                token.value.flags = WIO_NATIVE_ABI_VALUE_OWNED;
                token.value.owner = {&owner, &Owner::Ops, 1};
                token.value.payload.slice = {original.data(), original.size()};
                auto args = references(token);
                check(invoke(args.data(), escapedString) == WIO_NATIVE_ABI_OK, "owned string reference rejected");
                check(bytes(token.value) == original + suffix && bytes(escapedString.value) == original + suffix,
                      "string alias/copy-back lost NUL or emoji");
                check(owner.references == 0 && owner.releases == 1,
                      "overwritten producer string owner not released once");
                for (const auto* wire : {&token.value, &escapedString.value})
                    check(wire->flags == WIO_NATIVE_ABI_VALUE_OWNED && wire->typeId == stringId && wire->owner.ops &&
                              wire->owner.ops->typeId(wire->owner.state) == stringId,
                          "owned string lease identity disagrees");
                for (unsigned variant = 0; variant < 5; ++variant)
                {
                    auto invalid = args;
                    switch (variant)
                    {
                    case 0:
                        invalid[1].flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                        break;
                    case 1:
                        invalid[1].payload.mutableSlice.size -= 1;
                        break;
                    case 2:
                        invalid[1].typeId = textId;
                        break;
                    case 3:
                        invalid[1].owner.state = &owner;
                        break;
                    case 4:
                        invalid[1].kind = WIO_NATIVE_ABI_BYTES;
                        break;
                    }
                    Wire result;
                    check(invoke(invalid.data(), result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                          "malformed string borrow accepted");
                    check(bytes(token.value) == original + suffix, "failed string admission modified input");
                    safe();
                }
                // A byte string is not required to be valid Unicode text.
                const std::string binary{static_cast<char>(0xFF), '\0', 'z'};
                Wire binaryToken;
                binaryToken.value.kind = WIO_NATIVE_ABI_BYTES;
                binaryToken.value.typeId = stringId;
                binaryToken.value.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                binaryToken.value.payload.slice = {binary.data(), binary.size()};
                auto binaryArgs = references(binaryToken);
                Wire binaryResult;
                check(invoke(binaryArgs.data(), binaryResult) == WIO_NATIVE_ABI_OK &&
                          bytes(binaryResult.value) == binary + suffix && bytes(binaryToken.value) == binary + suffix,
                      "byte-string borrow incorrectly enforced Unicode validation");
                {
                    Wire bad;
                    bad.value.kind = WIO_NATIVE_ABI_BYTES;
                    bad.value.typeId = stringId;
                    bad.value.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                    bad.value.payload.slice = {nullptr, 5};
                    auto invalid = references(bad);
                    Wire result;
                    check(invoke(invalid.data(), result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                          "null nonempty string slice accepted");
                    safe();
                }
                check(machine.invoke(function("HostFailure")).succeeded(), "string panic capture failed");
                Wire failed;
                check(invoke(args.data(), failed) == WIO_NATIVE_ABI_PANIC &&
                          bytes(token.value) == std::string("changed\0\xF0\x9F\x8C\x8D", 12),
                      "string panic discarded completed write");
                safe();

                check(machine.invoke(function("HostText")).succeeded(), "text capture failed");
                const std::u32string unicode = {0x0130, 0, 0x1F30D};
                Owner unicodeOwner{textId};
                Wire unicodeToken;
                unicodeToken.value.kind = WIO_NATIVE_ABI_TEXT_UTF32;
                unicodeToken.value.typeId = textId;
                unicodeToken.value.flags = WIO_NATIVE_ABI_VALUE_OWNED;
                unicodeToken.value.owner = {&unicodeOwner, &Owner::Ops, 1};
                unicodeToken.value.payload.slice = {unicode.data(), unicode.size() * sizeof(char32_t)};
                auto textArgs = references(unicodeToken);
                check(invoke(textArgs.data(), escapedText) == WIO_NATIVE_ABI_OK, "text borrow rejected");
                check(text(unicodeToken.value) == unicode + std::u32string(U"\0z", 2) &&
                          text(escapedText.value) == text(unicodeToken.value),
                      "text alias/copy-back lost Unicode scalars");
                check(unicodeToken.value.owner.ops->typeId(unicodeToken.value.owner.state) == textId,
                      "owned text lease identity disagrees");
                check(unicodeOwner.references == 0 && unicodeOwner.releases == 1,
                      "overwritten producer text owner not released once");
                {
                    const char32_t points[] = {0x1F30D, 0};
                    std::array<char, sizeof(points) + 1> packed{};
                    std::memcpy(packed.data() + 1, points, sizeof(points));
                    Wire unaligned;
                    unaligned.value.kind = WIO_NATIVE_ABI_TEXT_UTF32;
                    unaligned.value.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                    unaligned.value.typeId = textId;
                    unaligned.value.payload.slice = {packed.data() + 1, sizeof(points)};
                    auto unalignedArgs = references(unaligned);
                    Wire result;
                    check(invoke(unalignedArgs.data(), result) == WIO_NATIVE_ABI_OK &&
                              text(result.value) == std::u32string{0x1F30D, 0, 0, U'z'},
                          "UTF-32 data slice incorrectly required native alignment");
                }
                static const char32_t surrogate[] = {0xD800};
                static const char32_t oversized[] = {0x110000};
                for (unsigned variant = 0; variant < 4; ++variant)
                {
                    Wire bad;
                    bad.value.kind = WIO_NATIVE_ABI_TEXT_UTF32;
                    bad.value.typeId = textId;
                    bad.value.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                    bad.value.payload.slice = {variant == 1 ? oversized : surrogate, sizeof(surrogate)};
                    if (variant == 2)
                        bad.value.payload.slice.size = 3;
                    if (variant == 3)
                        bad.value.payload.slice.data = nullptr;
                    auto invalid = references(bad);
                    Wire result;
                    check(invoke(invalid.data(), result) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                          "invalid Unicode borrow accepted");
                    safe();
                }

                const auto receiver = machine.invoke(function("HostReceiver"));
                check(receiver.succeeded(), "hosted receiver capture failed");
                auto saved = *reinterpret_cast<const WioNativeObjectHost*>(
                    static_cast<std::uintptr_t>(receiver.value().asUnsignedInteger()));
                WioNativeAbiRetain(saved.owner);
                NativePair pair;
                WioNativeAbiValue callback{};
                callback.kind = WIO_NATIVE_ABI_CALLBACK;
                callback.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
                callback.typeId = identity(method("Pair").parameterTypes[0]);
                callback.payload.callback = {&pair, &NativePair::Ops};
                const auto dispatch =
                    [&](const char* name, const WioNativeAbiValue* values, unsigned count, Wire& result)
                {
                    WioNativeAbiFailure failure{};
                    return saved.invoke(&saved, method(name).stableId, values, count, &result.value, &failure);
                };
                Wire rejected;
                check(dispatch("Pair", &callback, 1, rejected) == WIO_NATIVE_ABI_PANIC,
                      "invalid native Unicode copy-back accepted");
                Wire unchangedString;
                Wire unchangedText;
                check(dispatch("ReadString", nullptr, 0, unchangedString) == WIO_NATIVE_ABI_OK &&
                          bytes(unchangedString.value) == "original",
                      "VM partially committed earlier string argument");
                check(dispatch("ReadText", nullptr, 0, unchangedText) == WIO_NATIVE_ABI_OK &&
                          text(unchangedText.value) == U"original",
                      "VM partially committed invalid Unicode argument");
                for (pair.behavior = 2; pair.behavior <= 3; ++pair.behavior)
                {
                    Wire badIdentity;
                    check(dispatch("Pair", &callback, 1, badIdentity) == WIO_NATIVE_ABI_PANIC,
                          "native copy-back accepted a conflicting pointee identity");
                    Wire originalString;
                    check(dispatch("ReadString", nullptr, 0, originalString) == WIO_NATIVE_ABI_OK &&
                              bytes(originalString.value) == "original",
                          "identity failure partially committed earlier cell");
                }
                pair.behavior = 1;
                Wire accepted;
                check(dispatch("Pair", &callback, 1, accepted) == WIO_NATIVE_ABI_OK &&
                          accepted.value.payload.signedInteger == 9,
                      "native callback did not recover from invalid copy-back");
                check(pair.references == 1, "hosted callback owning claim leaked");
                WioNativeAbiRelease(&saved.owner);
                check(machine.invoke(function("HostClear")).succeeded(), "hosted receiver release failed");
            }
            Wire stale;
            WioNativeAbiValue args[2]{};
            WioNativeAbiFailure failure{};
            check(captured.value.ops->invoke(captured.value.userdata, args, 2, &stale.value, &failure) ==
                      WIO_NATIVE_ABI_STALE_HANDLE,
                  "closed machine accepted string/text callback");
            captured.clear();
        }
        check(bytes(escapedString.value) == std::string("a\0\xF0\x9F\x8C\x8D\0z", 8),
              "escaped string result lost storage after shutdown");
        const std::u32string expectedText = {0x0130, 0, 0x1F30D, 0, U'z'};
        const auto actualText = text(escapedText.value);
        check(actualText == expectedText, "escaped text result lost storage after shutdown");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3, "expected bytecode and producer paths");
        host(argv[1], argv[2]);
        std::cout << "callback-string-borrow-host-ok\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
