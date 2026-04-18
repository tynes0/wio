#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "module_api.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace wio::sdk
{
    enum class ErrorCode
    {
        InvalidArgument,
        LibraryOpenFailed,
        SymbolLookupFailed,
        ApiUnavailable,
        InvalidApiDescriptor,
        ExportNotFound,
        CommandNotFound,
        EventNotFound,
        EventHookNotFound,
        SignatureMismatch,
        InvokeFailed,
        LifecycleFailed,
        ReloadFailed,
        IoFailure
    };

    class Error : public std::runtime_error
    {
    public:
        Error(ErrorCode code, std::string message)
            : std::runtime_error(std::move(message)),
              code_(code)
        {
        }

        [[nodiscard]] ErrorCode code() const noexcept
        {
            return code_;
        }

    private:
        ErrorCode code_;
    };

    namespace detail
    {
#if defined(_WIN32)
        using LibraryHandle = HMODULE;
#else
        using LibraryHandle = void*;
#endif

        template <typename>
        struct FunctionTraits;

        template <typename TReturn, typename... TArgs>
        struct FunctionTraits<TReturn(TArgs...)>
        {
            using ReturnType = TReturn;
            using ArgumentTuple = std::tuple<TArgs...>;
            using StdFunction = std::function<TReturn(TArgs...)>;
            static constexpr size_t Arity = sizeof...(TArgs);

            template <size_t Index>
            using Argument = std::tuple_element_t<Index, ArgumentTuple>;
        };

        template <typename TReturn, typename... TArgs>
        struct FunctionTraits<TReturn(*)(TArgs...)> : FunctionTraits<TReturn(TArgs...)>
        {
        };

        template <typename TReturn, typename... TArgs>
        struct FunctionTraits<std::function<TReturn(TArgs...)>> : FunctionTraits<TReturn(TArgs...)>
        {
        };

        template <typename T>
        using Decay = std::remove_cv_t<std::remove_reference_t<T>>;

        inline std::string dynamicLibraryErrorMessage()
        {
#if defined(_WIN32)
            const DWORD errorCode = GetLastError();
            if (errorCode == 0)
                return "Unknown Windows loader error.";

            LPSTR buffer = nullptr;
            const DWORD length = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&buffer),
                0,
                nullptr
            );

            std::string result = length > 0 && buffer != nullptr ? std::string(buffer, length) : "Unknown Windows loader error.";
            if (buffer != nullptr)
                LocalFree(buffer);

            while (!result.empty() && (result.back() == '\r' || result.back() == '\n'))
                result.pop_back();

            return result;
#else
            const char* error = dlerror();
            return error != nullptr ? std::string(error) : "Unknown dynamic loader error.";
#endif
        }

        inline LibraryHandle openLibrary(const std::filesystem::path& path)
        {
#if defined(_WIN32)
            return LoadLibraryA(path.string().c_str());
#else
            return dlopen(path.string().c_str(), RTLD_NOW);
#endif
        }

        inline void closeLibrary(LibraryHandle handle) noexcept
        {
            if (handle == nullptr)
                return;

#if defined(_WIN32)
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
        }

        template <typename TSymbol>
        TSymbol loadSymbol(LibraryHandle handle, const char* symbolName)
        {
#if defined(_WIN32)
            auto* rawSymbol = GetProcAddress(handle, symbolName);
#else
            dlerror();
            void* rawSymbol = dlsym(handle, symbolName);
#endif
            return reinterpret_cast<TSymbol>(rawSymbol);
        }

        template <typename T>
        constexpr bool AlwaysFalse = false;

        template <typename T>
        constexpr WioAbiType getAbiType()
        {
            using U = Decay<T>;

            if constexpr (std::is_same_v<U, void>)
            {
                return WIO_ABI_VOID;
            }
            else if constexpr (std::is_same_v<U, bool>)
            {
                return WIO_ABI_BOOL;
            }
            else if constexpr (std::is_same_v<U, char>)
            {
                return WIO_ABI_CHAR;
            }
            else if constexpr (std::is_same_v<U, unsigned char>)
            {
                return WIO_ABI_UCHAR;
            }
            else if constexpr (std::is_same_v<U, std::byte>)
            {
                return WIO_ABI_BYTE;
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (sizeof(U) == sizeof(float))
                    return WIO_ABI_F32;
                else if constexpr (sizeof(U) == sizeof(double))
                    return WIO_ABI_F64;
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_same_v<U, signed char>)
                {
                    return WIO_ABI_I8;
                }
                else if constexpr (std::is_unsigned_v<U>)
                {
                    if constexpr (sizeof(U) == 1)
                        return WIO_ABI_U8;
                    else if constexpr (sizeof(U) == 2)
                        return WIO_ABI_U16;
                    else if constexpr (sizeof(U) == 4)
                        return WIO_ABI_U32;
                    else if constexpr (sizeof(U) == 8)
                        return WIO_ABI_U64;
                }
                else
                {
                    if constexpr (sizeof(U) == 1)
                        return WIO_ABI_I8;
                    else if constexpr (sizeof(U) == 2)
                        return WIO_ABI_I16;
                    else if constexpr (sizeof(U) == 4)
                        return WIO_ABI_I32;
                    else if constexpr (sizeof(U) == 8)
                        return WIO_ABI_I64;
                }
            }
            else
            {
                static_assert(AlwaysFalse<T>, "Unsupported Wio SDK ABI type.");
            }
        }

        inline const char* abiTypeName(WioAbiType type) noexcept
        {
            switch (type)
            {
            case WIO_ABI_UNKNOWN: return "unknown";
            case WIO_ABI_VOID: return "void";
            case WIO_ABI_BOOL: return "bool";
            case WIO_ABI_CHAR: return "char";
            case WIO_ABI_UCHAR: return "uchar";
            case WIO_ABI_BYTE: return "byte";
            case WIO_ABI_I8: return "i8";
            case WIO_ABI_I16: return "i16";
            case WIO_ABI_I32: return "i32";
            case WIO_ABI_I64: return "i64";
            case WIO_ABI_U8: return "u8";
            case WIO_ABI_U16: return "u16";
            case WIO_ABI_U32: return "u32";
            case WIO_ABI_U64: return "u64";
            case WIO_ABI_ISIZE: return "isize";
            case WIO_ABI_USIZE: return "usize";
            case WIO_ABI_F32: return "f32";
            case WIO_ABI_F64: return "f64";
            default: return "unknown";
            }
        }

        inline const char* invokeStatusName(std::int32_t status) noexcept
        {
            switch (status)
            {
            case WIO_INVOKE_OK: return "ok";
            case WIO_INVOKE_EXPORT_NOT_FOUND: return "export_not_found";
            case WIO_INVOKE_BAD_ARGUMENTS: return "bad_arguments";
            case WIO_INVOKE_TYPE_MISMATCH: return "type_mismatch";
            case WIO_INVOKE_RESULT_REQUIRED: return "result_required";
            case WIO_INVOKE_NOT_CALLABLE: return "not_callable";
            default: return "unknown";
            }
        }

        template <typename T>
        WioValue toWioValue(T&& value)
        {
            using U = Decay<T>;

            WioValue result{};
            result.type = getAbiType<U>();

            if constexpr (std::is_same_v<U, bool>)
            {
                result.value.v_bool = value;
            }
            else if constexpr (std::is_same_v<U, char>)
            {
                result.value.v_char = value;
            }
            else if constexpr (std::is_same_v<U, unsigned char>)
            {
                result.value.v_uchar = value;
            }
            else if constexpr (std::is_same_v<U, std::byte>)
            {
                result.value.v_byte = std::to_integer<std::uint8_t>(value);
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (sizeof(U) == sizeof(float))
                    result.value.v_f32 = static_cast<float>(value);
                else
                    result.value.v_f64 = static_cast<double>(value);
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_same_v<U, signed char>)
                    result.value.v_i8 = static_cast<std::int8_t>(value);
                else if constexpr (std::is_unsigned_v<U>)
                {
                    if constexpr (sizeof(U) == 1)
                        result.value.v_u8 = static_cast<std::uint8_t>(value);
                    else if constexpr (sizeof(U) == 2)
                        result.value.v_u16 = static_cast<std::uint16_t>(value);
                    else if constexpr (sizeof(U) == 4)
                        result.value.v_u32 = static_cast<std::uint32_t>(value);
                    else
                        result.value.v_u64 = static_cast<std::uint64_t>(value);
                }
                else
                {
                    if constexpr (sizeof(U) == 1)
                        result.value.v_i8 = static_cast<std::int8_t>(value);
                    else if constexpr (sizeof(U) == 2)
                        result.value.v_i16 = static_cast<std::int16_t>(value);
                    else if constexpr (sizeof(U) == 4)
                        result.value.v_i32 = static_cast<std::int32_t>(value);
                    else
                        result.value.v_i64 = static_cast<std::int64_t>(value);
                }
            }

            return result;
        }

        template <typename T>
        T fromWioValue(const WioValue& value)
        {
            using U = Decay<T>;
            const WioAbiType expectedType = getAbiType<U>();
            if (value.type != expectedType)
            {
                std::ostringstream message;
                message << "Wio SDK type mismatch: expected '" << abiTypeName(expectedType)
                        << "' but received '" << abiTypeName(value.type) << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            if constexpr (std::is_same_v<U, bool>)
            {
                return static_cast<T>(value.value.v_bool);
            }
            else if constexpr (std::is_same_v<U, char>)
            {
                return static_cast<T>(value.value.v_char);
            }
            else if constexpr (std::is_same_v<U, unsigned char>)
            {
                return static_cast<T>(value.value.v_uchar);
            }
            else if constexpr (std::is_same_v<U, std::byte>)
            {
                return static_cast<T>(std::byte{ value.value.v_byte });
            }
            else if constexpr (std::is_floating_point_v<U>)
            {
                if constexpr (sizeof(U) == sizeof(float))
                    return static_cast<T>(value.value.v_f32);
                else
                    return static_cast<T>(value.value.v_f64);
            }
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_same_v<U, signed char>)
                    return static_cast<T>(value.value.v_i8);
                else if constexpr (std::is_unsigned_v<U>)
                {
                    if constexpr (sizeof(U) == 1)
                        return static_cast<T>(value.value.v_u8);
                    else if constexpr (sizeof(U) == 2)
                        return static_cast<T>(value.value.v_u16);
                    else if constexpr (sizeof(U) == 4)
                        return static_cast<T>(value.value.v_u32);
                    else
                        return static_cast<T>(value.value.v_u64);
                }
                else
                {
                    if constexpr (sizeof(U) == 1)
                        return static_cast<T>(value.value.v_i8);
                    else if constexpr (sizeof(U) == 2)
                        return static_cast<T>(value.value.v_i16);
                    else if constexpr (sizeof(U) == 4)
                        return static_cast<T>(value.value.v_i32);
                    else
                        return static_cast<T>(value.value.v_i64);
                }
            }
        }

        template <typename Signature, size_t... Indices>
        void validateExportSignature(const WioModuleExport* exportEntry,
                                     std::string_view name,
                                     std::string_view bindingKind,
                                     std::index_sequence<Indices...>)
        {
            using Traits = FunctionTraits<Signature>;

            if (exportEntry == nullptr || exportEntry->invoke == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not resolve " << bindingKind << " '" << name << "'.";
                throw Error(ErrorCode::ExportNotFound, message.str());
            }

            constexpr std::uint32_t expectedCount = static_cast<std::uint32_t>(Traits::Arity);
            if (exportEntry->parameterCount != expectedCount)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " '" << name
                        << "': expected " << expectedCount << " parameters but module exports "
                        << exportEntry->parameterCount << ".";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            const WioAbiType expectedReturnType = getAbiType<typename Traits::ReturnType>();
            if (exportEntry->returnType != expectedReturnType)
            {
                std::ostringstream message;
                message << "Wio SDK signature mismatch for " << bindingKind << " '" << name
                        << "': expected return type '" << abiTypeName(expectedReturnType)
                        << "' but module exports '" << abiTypeName(exportEntry->returnType) << "'.";
                throw Error(ErrorCode::SignatureMismatch, message.str());
            }

            const std::array<WioAbiType, Traits::Arity> expectedParameterTypes = {
                getAbiType<typename Traits::template Argument<Indices>>()...
            };

            for (size_t i = 0; i < expectedParameterTypes.size(); ++i)
            {
                const WioAbiType actualType = exportEntry->parameterTypes != nullptr
                    ? exportEntry->parameterTypes[i]
                    : WIO_ABI_UNKNOWN;

                if (actualType != expectedParameterTypes[i])
                {
                    std::ostringstream message;
                    message << "Wio SDK signature mismatch for " << bindingKind << " '" << name
                            << "': parameter " << i << " expected '" << abiTypeName(expectedParameterTypes[i])
                            << "' but module exports '" << abiTypeName(actualType) << "'.";
                    throw Error(ErrorCode::SignatureMismatch, message.str());
                }
            }
        }

        template <typename Signature>
        void validateExportSignature(const WioModuleExport* exportEntry,
                                     std::string_view name,
                                     std::string_view bindingKind)
        {
            using Traits = FunctionTraits<Signature>;
            validateExportSignature<Signature>(
                exportEntry,
                name,
                bindingKind,
                std::make_index_sequence<Traits::Arity>{}
            );
        }

        template <typename Signature, size_t... Indices>
        auto bindExport(const WioModuleExport* exportEntry,
                        std::string name,
                        std::string bindingKind,
                        std::index_sequence<Indices...>) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            validateExportSignature<Signature>(exportEntry, name, bindingKind);

            return typename Traits::StdFunction(
                [exportEntry, name = std::move(name), bindingKind = std::move(bindingKind)](typename Traits::template Argument<Indices>... args) -> typename Traits::ReturnType
                {
                    constexpr size_t argCount = Traits::Arity;
                    std::array<WioValue, argCount> wioArgs{ toWioValue(args)... };
                    WioValue result{};

                    const std::int32_t status = exportEntry->invoke(
                        argCount > 0 ? wioArgs.data() : nullptr,
                        static_cast<std::uint32_t>(argCount),
                        std::is_void_v<typename Traits::ReturnType> ? nullptr : &result
                    );

                    if (status != WIO_INVOKE_OK)
                    {
                        std::ostringstream message;
                        message << "Wio SDK invoke failure for " << bindingKind << " '" << name
                                << "': " << invokeStatusName(status) << '.';
                        throw Error(ErrorCode::InvokeFailed, message.str());
                    }

                    if constexpr (std::is_void_v<typename Traits::ReturnType>)
                    {
                        return;
                    }
                    else
                    {
                        return fromWioValue<typename Traits::ReturnType>(result);
                    }
                }
            );
        }

        template <typename Signature>
        auto bindExport(const WioModuleExport* exportEntry,
                        std::string name,
                        std::string bindingKind) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return bindExport<Signature>(
                exportEntry,
                std::move(name),
                std::move(bindingKind),
                std::make_index_sequence<Traits::Arity>{}
            );
        }

        template <typename Signature, size_t... Indices>
        auto bindReloadable(std::function<typename FunctionTraits<Signature>::StdFunction(std::string_view)> resolver,
                            std::string name,
                            std::index_sequence<Indices...>) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return typename Traits::StdFunction(
                [resolver = std::move(resolver), name = std::move(name)](typename Traits::template Argument<Indices>... args) -> typename Traits::ReturnType
                {
                    auto callable = resolver(name);
                    if constexpr (std::is_void_v<typename Traits::ReturnType>)
                    {
                        callable(std::forward<typename Traits::template Argument<Indices>>(args)...);
                        return;
                    }
                    else
                    {
                        return callable(std::forward<typename Traits::template Argument<Indices>>(args)...);
                    }
                }
            );
        }

        template <typename Signature>
        auto bindReloadable(std::function<typename FunctionTraits<Signature>::StdFunction(std::string_view)> resolver,
                            std::string name) -> typename FunctionTraits<Signature>::StdFunction
        {
            using Traits = FunctionTraits<Signature>;
            return bindReloadable<Signature>(
                std::move(resolver),
                std::move(name),
                std::make_index_sequence<Traits::Arity>{}
            );
        }
    }

    template <typename Signature>
    auto wio_load_export(const WioModuleApi* api, std::string_view logicalName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_command(const WioModuleApi* api, std::string_view commandName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_event_hook(const WioModuleApi* api, std::string_view hookName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_event(const WioModuleApi* api, std::string_view eventName) -> typename detail::FunctionTraits<Signature>::StdFunction;

    template <typename Signature>
    auto wio_load_function(const WioModuleApi* api, std::string_view name) -> typename detail::FunctionTraits<Signature>::StdFunction;

    class Module
    {
    public:
        Module() = default;

        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;

        Module(Module&& other) noexcept
        {
            *this = std::move(other);
        }

        Module& operator=(Module&& other) noexcept
        {
            if (this == &other)
                return *this;

            close();
            handle_ = other.handle_;
            api_ = other.api_;
            path_ = std::move(other.path_);
            started_ = other.started_;

            other.handle_ = nullptr;
            other.api_ = nullptr;
            other.started_ = false;
            return *this;
        }

        ~Module()
        {
            close();
        }

        [[nodiscard]] static Module open(const std::filesystem::path& libraryPath)
        {
            if (libraryPath.empty())
                throw Error(ErrorCode::InvalidArgument, "Wio SDK expected a non-empty library path.");

            std::filesystem::path absolutePath = std::filesystem::absolute(libraryPath).make_preferred();
            if (!std::filesystem::exists(absolutePath))
            {
                std::ostringstream message;
                message << "Wio SDK could not find module library '" << absolutePath.string() << "'.";
                throw Error(ErrorCode::LibraryOpenFailed, message.str());
            }

            Module module;
            module.path_ = absolutePath;
            module.handle_ = detail::openLibrary(absolutePath);
            if (module.handle_ == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK failed to open '" << absolutePath.string() << "': "
                        << detail::dynamicLibraryErrorMessage();
                throw Error(ErrorCode::LibraryOpenFailed, message.str());
            }

            WioModuleGetApiFn getApi = detail::loadSymbol<WioModuleGetApiFn>(module.handle_, "WioModuleGetApi");
            if (getApi == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK could not load WioModuleGetApi from '" << absolutePath.string() << "': "
                        << detail::dynamicLibraryErrorMessage();
                throw Error(ErrorCode::SymbolLookupFailed, message.str());
            }

            module.api_ = getApi();
            if (module.api_ == nullptr)
            {
                std::ostringstream message;
                message << "Wio SDK received a null module API pointer from '" << absolutePath.string() << "'.";
                throw Error(ErrorCode::ApiUnavailable, message.str());
            }

            if (module.api_->descriptorVersion != WIO_MODULE_API_DESCRIPTOR_VERSION)
            {
                std::ostringstream message;
                message << "Wio SDK descriptor version mismatch for '" << absolutePath.string() << "': expected "
                        << WIO_MODULE_API_DESCRIPTOR_VERSION << " but module reports "
                        << module.api_->descriptorVersion << '.';
                throw Error(ErrorCode::InvalidApiDescriptor, message.str());
            }

            return module;
        }

        [[nodiscard]] static Module load(const std::filesystem::path& libraryPath)
        {
            Module module = open(libraryPath);
            module.start();
            return module;
        }

        void start()
        {
            if (started_)
                return;

            if (api_ != nullptr && api_->load != nullptr)
            {
                const std::int32_t status = api_->load();
                if (status != 0)
                {
                    std::ostringstream message;
                    message << "Wio SDK module load failed for '" << path_.string()
                            << "' with status " << status << '.';
                    throw Error(ErrorCode::LifecycleFailed, message.str());
                }
            }

            started_ = true;
        }

        void unload() noexcept
        {
            if (!started_)
                return;

            if (api_ != nullptr && api_->unload != nullptr)
                api_->unload();

            started_ = false;
        }

        void close() noexcept
        {
            unload();
            detail::closeLibrary(handle_);
            handle_ = nullptr;
            api_ = nullptr;
            path_.clear();
        }

        [[nodiscard]] const WioModuleApi* raw_api() const noexcept
        {
            return api_;
        }

        [[nodiscard]] const WioModuleApi& api() const
        {
            if (api_ == nullptr)
                throw Error(ErrorCode::ApiUnavailable, "Wio SDK module API is not available.");
            return *api_;
        }

        [[nodiscard]] bool has_api() const noexcept
        {
            return api_ != nullptr;
        }

        [[nodiscard]] bool started() const noexcept
        {
            return started_;
        }

        [[nodiscard]] bool supports_save_state() const noexcept
        {
            return api_ != nullptr && api_->saveState != nullptr;
        }

        [[nodiscard]] bool supports_restore_state() const noexcept
        {
            return api_ != nullptr && api_->restoreState != nullptr;
        }

        void update(float deltaTime) const
        {
            if (api_ != nullptr && api_->update != nullptr)
                api_->update(deltaTime);
        }

        [[nodiscard]] std::int32_t save_state() const
        {
            if (api_ == nullptr || api_->saveState == nullptr)
                throw Error(ErrorCode::LifecycleFailed, "Wio SDK save_state() is not available for this module.");

            return api_->saveState();
        }

        void restore_state(std::int32_t snapshot) const
        {
            if (api_ == nullptr || api_->restoreState == nullptr)
                throw Error(ErrorCode::LifecycleFailed, "Wio SDK restore_state() is not available for this module.");

            const std::int32_t status = api_->restoreState(snapshot);
            if (status != 0)
            {
                std::ostringstream message;
                message << "Wio SDK restore_state() failed for '" << path_.string()
                        << "' with status " << status << '.';
                throw Error(ErrorCode::LifecycleFailed, message.str());
            }
        }

        template <typename Signature>
        auto load_export(std::string_view logicalName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_export<Signature>(api_, logicalName);
        }

        template <typename Signature>
        auto load_command(std::string_view commandName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_command<Signature>(api_, commandName);
        }

        template <typename Signature>
        auto load_event_hook(std::string_view hookName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_event_hook<Signature>(api_, hookName);
        }

        template <typename Signature>
        auto load_event(std::string_view eventName) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_event<Signature>(api_, eventName);
        }

        template <typename Signature>
        auto load_function(std::string_view name) const -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            return wio::sdk::wio_load_function<Signature>(api_, name);
        }

    private:
        detail::LibraryHandle handle_ = nullptr;
        const WioModuleApi* api_ = nullptr;
        std::filesystem::path path_;
        bool started_ = false;
    };

    struct HotReloadOptions
    {
        std::filesystem::path stagingDirectory{};
        bool preserveState = true;
        bool invokeUnload = true;
        bool autoReload = false;
    };

    class HotReloadModule
    {
    public:
        explicit HotReloadModule(std::filesystem::path libraryPath, HotReloadOptions options = {})
            : sourcePath_(std::filesystem::absolute(std::move(libraryPath)).make_preferred()),
              options_(std::move(options)),
              autoReloadEnabled_(options_.autoReload)
        {
            if (options_.stagingDirectory.empty())
                options_.stagingDirectory = sourcePath_.parent_path() / ".wio_hot_reload";
        }

        [[nodiscard]] static HotReloadModule load(const std::filesystem::path& libraryPath, HotReloadOptions options = {})
        {
            HotReloadModule module(libraryPath, std::move(options));
            module.start();
            return module;
        }

        void start()
        {
            if (module_.has_api())
                return;

            loadFresh(/*allowRestore=*/false);
        }

        void enable_auto_reload(bool enabled = true) noexcept
        {
            autoReloadEnabled_ = enabled;
        }

        [[nodiscard]] bool auto_reload_enabled() const noexcept
        {
            return autoReloadEnabled_;
        }

        void set_source_path(const std::filesystem::path& libraryPath)
        {
            sourcePath_ = std::filesystem::absolute(libraryPath).make_preferred();
        }

        [[nodiscard]] const std::filesystem::path& source_path() const noexcept
        {
            return sourcePath_;
        }

        [[nodiscard]] std::uint64_t generation() const noexcept
        {
            return generation_;
        }

        [[nodiscard]] const Module& current() const noexcept
        {
            return module_;
        }

        [[nodiscard]] const WioModuleApi* raw_api() const noexcept
        {
            return module_.raw_api();
        }

        bool reload_if_changed()
        {
            if (sourcePath_.empty() || !std::filesystem::exists(sourcePath_))
                return false;

            const auto writeTime = std::filesystem::last_write_time(sourcePath_);
            if (writeTime <= lastLoadedWriteTime_)
                return false;

            reload();
            return true;
        }

        void reload()
        {
            loadFresh(/*allowRestore=*/true);
        }

        void reload_from(const std::filesystem::path& libraryPath)
        {
            set_source_path(libraryPath);
            reload();
        }

        void update(float deltaTime)
        {
            maybeAutoReload();
            module_.update(deltaTime);
        }

        template <typename Signature>
        auto load_export(std::string_view logicalName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_export<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(logicalName));
        }

        template <typename Signature>
        auto load_command(std::string_view commandName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_command<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(commandName));
        }

        template <typename Signature>
        auto load_event_hook(std::string_view hookName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_event_hook<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(hookName));
        }

        template <typename Signature>
        auto load_event(std::string_view eventName) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view name)
                {
                    maybeAutoReload();
                    return module_.template load_event<Signature>(name);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(eventName));
        }

        template <typename Signature>
        auto load_function(std::string_view name) -> typename detail::FunctionTraits<Signature>::StdFunction
        {
            using StdFunction = typename detail::FunctionTraits<Signature>::StdFunction;
            std::function<StdFunction(std::string_view)> resolver =
                [this](std::string_view logicalName)
                {
                    maybeAutoReload();
                    return module_.template load_function<Signature>(logicalName);
                };
            return detail::bindReloadable<Signature>(std::move(resolver), std::string(name));
        }

    private:
        void maybeAutoReload()
        {
            if (autoReloadEnabled_)
                reload_if_changed();
        }

        std::filesystem::path stageCopy()
        {
            if (!std::filesystem::exists(sourcePath_))
            {
                std::ostringstream message;
                message << "Wio SDK hot reload source library '" << sourcePath_.string() << "' does not exist.";
                throw Error(ErrorCode::ReloadFailed, message.str());
            }

            std::filesystem::create_directories(options_.stagingDirectory);
            ++generation_;

            std::ostringstream fileName;
            fileName << sourcePath_.stem().string() << ".hot." << generation_ << sourcePath_.extension().string();
            std::filesystem::path stagedPath = (options_.stagingDirectory / fileName.str()).make_preferred();

            std::error_code ec;
            std::filesystem::copy_file(sourcePath_, stagedPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                std::ostringstream message;
                message << "Wio SDK failed to stage hot reload copy from '" << sourcePath_.string()
                        << "' to '" << stagedPath.string() << "': " << ec.message();
                throw Error(ErrorCode::IoFailure, message.str());
            }

            stagedArtifacts_.push_back(stagedPath);
            return stagedPath;
        }

        void pruneStagedArtifacts() noexcept
        {
            if (stagedArtifacts_.size() <= 1)
                return;

            for (size_t i = 0; i + 1 < stagedArtifacts_.size(); ++i)
            {
                std::error_code ec;
                std::filesystem::remove(stagedArtifacts_[i], ec);
            }

            stagedArtifacts_.erase(stagedArtifacts_.begin(), stagedArtifacts_.end() - 1);
        }

        void loadFresh(bool allowRestore)
        {
            std::optional<std::int32_t> snapshot;
            if (allowRestore && options_.preserveState && module_.supports_save_state())
                snapshot = module_.save_state();

            if (options_.invokeUnload)
                module_.unload();
            module_.close();

            std::filesystem::path stagedPath = stageCopy();
            Module nextModule = Module::open(stagedPath);

            if (snapshot.has_value() && nextModule.supports_restore_state())
            {
                nextModule.restore_state(*snapshot);
            }
            else
            {
                nextModule.start();
            }

            module_ = std::move(nextModule);
            lastLoadedWriteTime_ = std::filesystem::last_write_time(sourcePath_);
            pruneStagedArtifacts();
        }

        std::filesystem::path sourcePath_;
        HotReloadOptions options_;
        Module module_;
        std::filesystem::file_time_type lastLoadedWriteTime_{};
        bool autoReloadEnabled_ = false;
        std::uint64_t generation_ = 0;
        std::vector<std::filesystem::path> stagedArtifacts_;
    };

    template <typename Signature>
    auto wio_load_export(const WioModuleApi* api, std::string_view logicalName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(logicalName);
        const WioModuleExport* exportEntry = WioFindModuleExport(api, ownedName.c_str());
        if (exportEntry == nullptr)
        {
            std::ostringstream message;
            message << "Wio SDK could not find export '" << ownedName << "'.";
            throw Error(ErrorCode::ExportNotFound, message.str());
        }

        return detail::bindExport<Signature>(exportEntry, ownedName, "export");
    }

    template <typename Signature>
    auto wio_load_command(const WioModuleApi* api, std::string_view commandName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(commandName);
        const WioModuleCommand* commandEntry = WioFindModuleCommand(api, ownedName.c_str());
        if (commandEntry == nullptr || commandEntry->exportEntry == nullptr)
        {
            std::ostringstream message;
            message << "Wio SDK could not find command '" << ownedName << "'.";
            throw Error(ErrorCode::CommandNotFound, message.str());
        }

        return detail::bindExport<Signature>(commandEntry->exportEntry, ownedName, "command");
    }

    template <typename Signature>
    auto wio_load_event_hook(const WioModuleApi* api, std::string_view hookName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(hookName);
        const WioModuleEventHook* hookEntry = WioFindModuleEventHook(api, ownedName.c_str());
        if (hookEntry == nullptr || hookEntry->exportEntry == nullptr)
        {
            std::ostringstream message;
            message << "Wio SDK could not find event hook '" << ownedName << "'.";
            throw Error(ErrorCode::EventHookNotFound, message.str());
        }

        return detail::bindExport<Signature>(hookEntry->exportEntry, ownedName, "event hook");
    }

    template <typename Signature>
    auto wio_load_event(const WioModuleApi* api, std::string_view eventName) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        using Traits = detail::FunctionTraits<Signature>;
        if constexpr (!std::is_void_v<typename Traits::ReturnType>)
        {
            throw Error(ErrorCode::SignatureMismatch, "Wio SDK event broadcasters currently require a void return type.");
        }

        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(eventName);
        bool foundAny = false;
        if (api->eventHooks != nullptr)
        {
            for (std::uint32_t i = 0; i < api->eventHookCount; ++i)
            {
                const WioModuleEventHook& hook = api->eventHooks[i];
                if (hook.eventName == nullptr || std::strcmp(hook.eventName, ownedName.c_str()) != 0)
                    continue;

                foundAny = true;
                detail::validateExportSignature<Signature>(hook.exportEntry, ownedName, "event hook");
            }
        }

        if (!foundAny)
        {
            std::ostringstream message;
            message << "Wio SDK could not find any event hooks for event '" << ownedName << "'.";
            throw Error(ErrorCode::EventNotFound, message.str());
        }

        return typename Traits::StdFunction(
            [api, ownedName](auto... args) -> void
            {
                std::array<WioValue, sizeof...(args)> wioArgs{ detail::toWioValue(args)... };
                const std::int32_t status = WioBroadcastModuleEvent(
                    api,
                    ownedName.c_str(),
                    sizeof...(args) > 0 ? wioArgs.data() : nullptr,
                    static_cast<std::uint32_t>(sizeof...(args))
                );

                if (status != WIO_INVOKE_OK)
                {
                    std::ostringstream message;
                    message << "Wio SDK failed to broadcast event '" << ownedName
                            << "': " << detail::invokeStatusName(status) << '.';
                    throw Error(ErrorCode::InvokeFailed, message.str());
                }
            }
        );
    }

    template <typename Signature>
    auto wio_load_function(const WioModuleApi* api, std::string_view name) -> typename detail::FunctionTraits<Signature>::StdFunction
    {
        if (api == nullptr)
            throw Error(ErrorCode::ApiUnavailable, "Wio SDK expected a valid module API pointer.");

        const std::string ownedName(name);

        if (const WioModuleExport* exportEntry = WioFindModuleExport(api, ownedName.c_str()); exportEntry != nullptr)
            return detail::bindExport<Signature>(exportEntry, ownedName, "export");

        if (const WioModuleCommand* commandEntry = WioFindModuleCommand(api, ownedName.c_str());
            commandEntry != nullptr && commandEntry->exportEntry != nullptr)
        {
            return detail::bindExport<Signature>(commandEntry->exportEntry, ownedName, "command");
        }

        if (const WioModuleEventHook* hookEntry = WioFindModuleEventHook(api, ownedName.c_str());
            hookEntry != nullptr && hookEntry->exportEntry != nullptr)
        {
            return detail::bindExport<Signature>(hookEntry->exportEntry, ownedName, "event hook");
        }

        std::ostringstream message;
        message << "Wio SDK could not resolve function '" << ownedName
                << "' as an export, command, or event hook.";
        throw Error(ErrorCode::ExportNotFound, message.str());
    }

}

template <typename Signature>
auto wio_load_export(const WioModuleApi* api, std::string_view logicalName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_export<Signature>(api, logicalName);
}

template <typename Signature>
auto wio_load_export(const wio::sdk::Module& module, std::string_view logicalName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_export<Signature>(logicalName);
}

template <typename Signature>
auto wio_load_export(wio::sdk::HotReloadModule& module, std::string_view logicalName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_export<Signature>(logicalName);
}

template <typename Signature>
auto wio_load_command(const WioModuleApi* api, std::string_view commandName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_command<Signature>(api, commandName);
}

template <typename Signature>
auto wio_load_command(const wio::sdk::Module& module, std::string_view commandName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_command<Signature>(commandName);
}

template <typename Signature>
auto wio_load_command(wio::sdk::HotReloadModule& module, std::string_view commandName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_command<Signature>(commandName);
}

template <typename Signature>
auto wio_load_event_hook(const WioModuleApi* api, std::string_view hookName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_event_hook<Signature>(api, hookName);
}

template <typename Signature>
auto wio_load_event_hook(const wio::sdk::Module& module, std::string_view hookName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event_hook<Signature>(hookName);
}

template <typename Signature>
auto wio_load_event_hook(wio::sdk::HotReloadModule& module, std::string_view hookName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event_hook<Signature>(hookName);
}

template <typename Signature>
auto wio_load_event(const WioModuleApi* api, std::string_view eventName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_event<Signature>(api, eventName);
}

template <typename Signature>
auto wio_load_event(const wio::sdk::Module& module, std::string_view eventName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event<Signature>(eventName);
}

template <typename Signature>
auto wio_load_event(wio::sdk::HotReloadModule& module, std::string_view eventName) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_event<Signature>(eventName);
}

template <typename Signature>
auto wio_load_function(const WioModuleApi* api, std::string_view name) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return wio::sdk::wio_load_function<Signature>(api, name);
}

template <typename Signature>
auto wio_load_function(const wio::sdk::Module& module, std::string_view name) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_function<Signature>(name);
}

template <typename Signature>
auto wio_load_function(wio::sdk::HotReloadModule& module, std::string_view name) -> typename wio::sdk::detail::FunctionTraits<Signature>::StdFunction
{
    return module.template load_function<Signature>(name);
}
