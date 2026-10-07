#include "native_library.h"

#include "wio/vm/machine.h"

#include <wio_native_abi.h>
#include <wio_native_pod.h>
#include <wio_native_sequence.h>
#include <wio_native_dictionary.h>

#include <system_error>
#include <map>
#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace wio::vm::detail
{
    namespace
    {
#if defined(_WIN32)
        using LibraryHandle = HMODULE;
#else
        using LibraryHandle = void*;
#endif

        std::string loaderError()
        {
#if defined(_WIN32)
            const DWORD code = GetLastError();
            if (code == 0)
                return "unknown Windows loader error";

            LPSTR buffer = nullptr;
            const DWORD size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
            std::string message = size != 0 && buffer ? std::string(buffer, size) : "unknown Windows loader error";
            if (buffer)
                LocalFree(buffer);
            while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
                message.pop_back();
            return message;
#else
            const char* message = dlerror();
            return message ? message : "unknown dynamic loader error";
#endif
        }

        LibraryHandle openLibrary(const std::filesystem::path& path)
        {
#if defined(_WIN32)
            return LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
            return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        }

        void closeLibrary(const LibraryHandle handle) noexcept
        {
            if (!handle)
                return;
#if defined(_WIN32)
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
        }

        template <typename T> T loadSymbol(const LibraryHandle handle, const char* name) noexcept
        {
#if defined(_WIN32)
            return reinterpret_cast<T>(GetProcAddress(handle, name));
#else
            dlerror();
            return reinterpret_cast<T>(dlsym(handle, name));
#endif
        }
    } // namespace

    struct NativeLibrary::State final
    {
        std::filesystem::path path;
        LibraryHandle handle = nullptr;
        const WioNativeAbiRegistry* registry = nullptr;
        const WioNativeTaskApi* tasks = nullptr;
        const WioNativeTaskCompletionApi* completions = nullptr;
        const WioNativePodLayoutApi* pods = nullptr;
        const WioNativePodDefaultApi* defaults = nullptr;
        const WioNativeSequenceApi* sequences = nullptr;
        const WioNativeDictionaryApi* dictionaries = nullptr;
        std::mutex machineMutex;
        std::size_t activeMachines = 0;

        ~State()
        {
            if (activeMachines != 0 && tasks && tasks->shutdown)
                tasks->shutdown();
            closeLibrary(handle);
        }
    };

    NativeLibrary::~NativeLibrary() = default;

    std::shared_ptr<NativeLibrary> NativeLibrary::open(const std::filesystem::path& input, std::string& error)
    {
        // One process-wide lease per physical library prevents one machine's
        // teardown from shutting down a producer still used by another VM.
        static std::mutex cacheMutex;
        static std::map<std::filesystem::path, std::weak_ptr<NativeLibrary>> cache;
        std::scoped_lock cacheLock{cacheMutex};
        std::error_code pathError;
        std::filesystem::path path = std::filesystem::weakly_canonical(std::filesystem::absolute(input), pathError);
        if (pathError)
            path = std::filesystem::absolute(input).make_preferred();
        else
            path.make_preferred();

        if (const auto found = cache.find(path); found != cache.end())
        {
            if (auto loaded = found->second.lock())
            {
                error.clear();
                return loaded;
            }
            cache.erase(found);
        }
        // Windows case variants and hard-link aliases can identify the same
        // producer despite having different canonical path spellings.
        for (const auto& [cachedPath, cached] : cache)
        {
            if (auto loaded = cached.lock())
            {
                std::error_code identityError;
                if (std::filesystem::equivalent(cachedPath, path, identityError) && !identityError)
                {
                    error.clear();
                    return loaded;
                }
            }
        }

        if (!std::filesystem::is_regular_file(path, pathError) || pathError)
        {
            error = "native VM module does not exist: " + path.string();
            return {};
        }

        auto library = std::shared_ptr<NativeLibrary>{new NativeLibrary{}};
        library->state_ = std::make_unique<State>();
        library->state_->path = path;
        LibraryHandle handle = openLibrary(path);
        library->state_->handle = handle;
        if (!handle)
        {
            error = "could not load native VM module '" + path.string() + "': " + loaderError();
            return {};
        }

        const auto fail = [&](std::string message) -> std::shared_ptr<NativeLibrary>
        {
            error = std::move(message);
            return {};
        };

        const auto getRegistry = loadSymbol<WioGetNativeAbiRegistryFn>(handle, "WioGetNativeAbiRegistry");
        if (!getRegistry)
            return fail("native VM module '" + path.string() + "' does not export WioGetNativeAbiRegistry");
        const WioNativeAbiRegistry* registry = getRegistry();
        if (!registry)
            return fail("native VM module '" + path.string() + "' returned a null native ABI registry");

        const auto getTasks = loadSymbol<WioGetNativeTaskApiFn>(handle, "WioGetNativeTaskApi");
        const auto getCompletions =
            loadSymbol<WioGetNativeTaskCompletionApiFn>(handle, "WioGetNativeTaskCompletionApi");
        if (!getTasks && getCompletions)
            return fail("native VM module '" + path.string() +
                        "' exports task completions without the native task API");

        library->state_->registry = registry;
        if (const auto getPods = loadSymbol<WioGetNativePodLayoutApiFn>(handle, "WioGetNativePodLayoutApi"))
        {
            library->state_->pods = getPods();
            if (!library->state_->pods)
                return fail("native VM module returned a null POD layout sidecar");
        }
        if (getTasks)
        {
            library->state_->tasks = getTasks();
            if (getCompletions)
                library->state_->completions = getCompletions();
            if (!library->state_->tasks || (getCompletions && !library->state_->completions))
            {
                error = "native VM module returned a null native task sidecar";
                return {};
            }
            if (library->state_->tasks->abiVersion != WIO_NATIVE_ABI_VERSION)
            {
                error = "native VM module task API version does not match the VM wire ABI";
                return {};
            }
        }
        if (const auto getDefaults = loadSymbol<WioGetNativePodDefaultApiFn>(handle, "WioGetNativePodDefaultApi"))
        {
            library->state_->defaults = getDefaults();
            if (!library->state_->defaults || !library->state_->pods)
                return fail("native VM module default sidecar requires a non-null API and POD layout sidecar");
        }
        if (const auto getSequences = loadSymbol<WioGetNativeSequenceApiFn>(handle, "WioGetNativeSequenceApi"))
        {
            library->state_->sequences = getSequences();
            if (!library->state_->sequences)
                return fail("native VM module returned a null sequence sidecar");
        }
        if (const auto getDictionaries = loadSymbol<WioGetNativeDictionaryApiFn>(handle, "WioGetNativeDictionaryApi"))
        {
            library->state_->dictionaries = getDictionaries();
            if (!library->state_->dictionaries)
                return fail("native VM module returned a null dictionary sidecar");
        }
        error.clear();
        cache.insert_or_assign(library->path(), library);
        return library;
    }

    bool NativeLibrary::registerWith(Machine& machine, std::string& error) const
    {
        if (!state_ || !state_->registry)
        {
            error = "native VM module is not loaded";
            return false;
        }
        std::scoped_lock lock{state_->machineMutex};
        const bool registered =
            state_->dictionaries
                ? machine.registerNativeRegistry(*state_->registry, *state_->dictionaries, &error, state_->sequences,
                                                 state_->pods, state_->defaults,
                                                 state_->completions ? state_->tasks : nullptr, state_->completions)
            : state_->sequences
                ? machine.registerNativeRegistry(*state_->registry, *state_->sequences, &error, state_->pods,
                                                 state_->defaults, state_->completions ? state_->tasks : nullptr,
                                                 state_->completions)
            : state_->pods ? machine.registerNativeRegistry(*state_->registry, *state_->pods,
                                                            state_->completions ? state_->tasks : nullptr,
                                                            state_->completions, &error, state_->defaults)
            : state_->tasks && state_->completions
                ? machine.registerNativeRegistry(*state_->registry, *state_->tasks, *state_->completions, &error)
                : machine.registerNativeRegistry(*state_->registry, &error);
        if (registered)
            ++state_->activeMachines;
        return registered;
    }

    void NativeLibrary::releaseMachine() const noexcept
    {
        std::scoped_lock lock{state_->machineMutex};
        if (state_->activeMachines == 0 || --state_->activeMachines != 0)
            return;
        // Quiesce producer threads while the host still holds its library
        // lease. Deferring this to the final callback's lease release could
        // unload the DLL while returning into producer code on that thread.
        if (state_->tasks && state_->tasks->shutdown)
            state_->tasks->shutdown();
    }

    const std::filesystem::path& NativeLibrary::path() const noexcept
    {
        return state_->path;
    }
} // namespace wio::vm::detail
