#include "std_net.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace wio::runtime::std_net
{
    namespace
    {
#if defined(_WIN32)
        using NativeSocket = SOCKET;
        constexpr NativeSocket invalidSocket = INVALID_SOCKET;
        struct WinsockRuntime
        {
            WinsockRuntime() { WSADATA data{}; ready = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
            ~WinsockRuntime() { if (ready) WSACleanup(); }
            bool ready = false;
        };
        WinsockRuntime& runtime() { static WinsockRuntime value; return value; }
        int lastSocketError() noexcept { return WSAGetLastError(); }
        void closeNative(NativeSocket value) noexcept { if (value != invalidSocket) closesocket(value); }
#else
        using NativeSocket = int;
        constexpr NativeSocket invalidSocket = -1;
        int lastSocketError() noexcept { return errno; }
        void closeNative(NativeSocket value) noexcept { if (value != invalidSocket) ::close(value); }
#endif
        std::atomic<std::uint64_t> liveSocketCount{0};

        struct SocketHandle
        {
            SocketHandle() { liveSocketCount.fetch_add(1, std::memory_order_relaxed); }
            ~SocketHandle() { liveSocketCount.fetch_sub(1, std::memory_order_relaxed); }
            std::atomic<std::size_t> references{1};
            std::mutex lifecycleMutex;
            std::mutex sendMutex;
            std::mutex receiveMutex;
            NativeSocket value = invalidSocket;
            bool closed = false;
            std::uint64_t receiveTimeoutMilliseconds = 0;
        };
        std::string errorMessage(const char* operation)
        {
            return std::string(operation) + " failed with native socket error " + std::to_string(lastSocketError()) + ".";
        }
        SocketHandle* asHandle(void* value) noexcept { return static_cast<SocketHandle*>(value); }

        class SocketLease final
        {
        public:
            explicit SocketLease(void* handle) noexcept : handle_(handle) {}
            SocketLease(const SocketLease&) = delete;
            SocketLease& operator=(const SocketLease&) = delete;
            ~SocketLease() { if (handle_) Release(handle_); }
        private:
            void* handle_ = nullptr;
        };

        bool ensureRuntime(std::string& error) noexcept
        {
#if defined(_WIN32)
            if (!runtime().ready) { error = "Winsock initialization failed."; return false; }
#endif
            return true;
        }

        bool waitReadable(
            SocketHandle* state,
            const bool honorConfiguredTimeout,
            const char* operation,
            NativeSocket& value,
            std::string& error) noexcept
        {
            using Clock = std::chrono::steady_clock;
            const std::uint64_t configuredTimeout = honorConfiguredTimeout
                ? state->receiveTimeoutMilliseconds
                : 0;
            const auto deadline = configuredTimeout == 0
                ? Clock::time_point::max()
                : Clock::now() + std::chrono::milliseconds(configuredTimeout);

            while (true)
            {
                {
                    std::lock_guard lifecycleLock(state->lifecycleMutex);
                    if (state->closed)
                    {
                        error = "socket is closed";
                        return false;
                    }
                    value = state->value;
                }

                std::uint64_t waitMicroseconds = 50'000;
                if (deadline != Clock::time_point::max())
                {
                    const auto now = Clock::now();
                    if (now >= deadline)
                    {
                        error = std::string(operation) + " timed out";
                        return false;
                    }
                    const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
                    waitMicroseconds = std::min<std::uint64_t>(
                        waitMicroseconds, static_cast<std::uint64_t>(std::max<std::int64_t>(1, remaining)));
                }

                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(value, &readable);
                timeval timeout{
                    static_cast<long>(waitMicroseconds / 1'000'000),
                    static_cast<long>(waitMicroseconds % 1'000'000)};
#if defined(_WIN32)
                const int ready = select(0, &readable, nullptr, nullptr, &timeout);
#else
                const int ready = select(value + 1, &readable, nullptr, nullptr, &timeout);
#endif
                if (ready > 0)
                    return true;
                if (ready == 0)
                    continue;

                std::lock_guard lifecycleLock(state->lifecycleMutex);
                error = state->closed
                    ? "socket is closed"
                    : errorMessage(operation);
                return false;
            }
        }
    }

    bool Resolve(const std::string_view host, const std::uint16_t port,
                 std::vector<std::string>& addresses, std::string& error) noexcept
    {
        addresses.clear(); error.clear();
        if (!ensureRuntime(error)) return false;
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
        addrinfo* raw = nullptr;
        const std::string hostText(host);
        const std::string service = std::to_string(port);
        const int result = getaddrinfo(hostText.c_str(), service.c_str(), &hints, &raw);
        if (result != 0) { error = "DNS resolution failed: " + std::string(gai_strerror(result)); return false; }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> entries(raw, freeaddrinfo);
        for (addrinfo* entry = raw; entry; entry = entry->ai_next)
        {
            char buffer[NI_MAXHOST]{};
            if (getnameinfo(entry->ai_addr, static_cast<socklen_t>(entry->ai_addrlen),
                            buffer, sizeof(buffer), nullptr, 0, NI_NUMERICHOST) == 0)
            {
                if (std::find(addresses.begin(), addresses.end(), buffer) == addresses.end())
                    addresses.emplace_back(buffer);
            }
        }
        return !addresses.empty();
    }

    bool TcpConnect(const std::string_view host, const std::uint16_t port,
                    const std::uint64_t timeoutMilliseconds, void*& handle, std::string& error) noexcept
    {
        handle = nullptr; error.clear();
        if (!ensureRuntime(error)) return false;
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
        addrinfo* raw = nullptr;
        const std::string hostText(host); const std::string service = std::to_string(port);
        const int result = getaddrinfo(hostText.c_str(), service.c_str(), &hints, &raw);
        if (result != 0) { error = "DNS resolution failed: " + std::string(gai_strerror(result)); return false; }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> entries(raw, freeaddrinfo);
        for (addrinfo* entry = raw; entry; entry = entry->ai_next)
        {
            NativeSocket socketValue = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
            if (socketValue == invalidSocket) continue;
            auto candidate = std::make_unique<SocketHandle>(); candidate->value = socketValue;
            std::string ignored;
            if (timeoutMilliseconds != 0) static_cast<void>(SetTimeout(candidate.get(), timeoutMilliseconds, ignored));
            if (connect(socketValue, entry->ai_addr, static_cast<int>(entry->ai_addrlen)) == 0)
            {
                handle = candidate.release(); return true;
            }
            closeNative(socketValue);
        }
        error = errorMessage("connect"); return false;
    }

    bool TcpListen(const std::string_view bindAddress, const std::uint16_t port,
                   const std::int32_t backlog, void*& handle, std::string& error) noexcept
    {
        handle = nullptr; error.clear();
        if (!ensureRuntime(error)) return false;
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_PASSIVE;
        addrinfo* raw = nullptr;
        const std::string hostText(bindAddress); const std::string service = std::to_string(port);
        const char* hostPointer = hostText.empty() ? nullptr : hostText.c_str();
        const int result = getaddrinfo(hostPointer, service.c_str(), &hints, &raw);
        if (result != 0) { error = "Address resolution failed: " + std::string(gai_strerror(result)); return false; }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> entries(raw, freeaddrinfo);
        for (addrinfo* entry = raw; entry; entry = entry->ai_next)
        {
            NativeSocket socketValue = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
            if (socketValue == invalidSocket) continue;
            int enabled = 1;
            setsockopt(socketValue, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&enabled), sizeof(enabled));
            if (bind(socketValue, entry->ai_addr, static_cast<int>(entry->ai_addrlen)) == 0 &&
                listen(socketValue, backlog) == 0)
            {
                auto resultHandle = std::make_unique<SocketHandle>(); resultHandle->value = socketValue;
                handle = resultHandle.release(); return true;
            }
            closeNative(socketValue);
        }
        error = errorMessage("listen"); return false;
    }

    bool TcpAccept(void* listener, void*& handle, std::string& error) noexcept
    {
        handle = nullptr; error.clear();
        if (!Retain(listener, error)) return false;
        SocketLease lease(listener);
        auto* state = asHandle(listener);
        std::lock_guard receiveLock(state->receiveMutex);
        while (true)
        {
            NativeSocket value = invalidSocket;
            if (!waitReadable(state, false, "accept wait", value, error))
                return false;
            NativeSocket accepted = accept(value, nullptr, nullptr);
            if (accepted == invalidSocket)
            {
                std::lock_guard lifecycleLock(state->lifecycleMutex);
                error = state->closed ? "socket is closed" : errorMessage("accept");
                return false;
            }
            {
                std::lock_guard lifecycleLock(state->lifecycleMutex);
                if (state->closed)
                {
                    closeNative(accepted);
                    error = "socket is closed";
                    return false;
                }
            }
            auto result = std::make_unique<SocketHandle>();
            result->value = accepted;
            handle = result.release();
            return true;
        }
    }

    bool SetTimeout(void* handle, const std::uint64_t milliseconds, std::string& error) noexcept
    {
        error.clear();
        if (!Retain(handle, error)) return false;
        SocketLease lease(handle);
        auto* state = asHandle(handle);
        std::scoped_lock operationLock(state->sendMutex, state->receiveMutex);
#if defined(_WIN32)
        const DWORD timeout = static_cast<DWORD>(std::min<std::uint64_t>(milliseconds, 0xffffffffu));
        const char* data = reinterpret_cast<const char*>(&timeout); const int size = sizeof(timeout);
#else
        const timeval timeout{ static_cast<time_t>(milliseconds / 1000u), static_cast<suseconds_t>((milliseconds % 1000u) * 1000u) };
        const char* data = reinterpret_cast<const char*>(&timeout); const socklen_t size = sizeof(timeout);
#endif
        const NativeSocket value = state->value;
        if (setsockopt(value, SOL_SOCKET, SO_RCVTIMEO, data, size) != 0 ||
            setsockopt(value, SOL_SOCKET, SO_SNDTIMEO, data, size) != 0)
        { error = errorMessage("setsockopt"); return false; }
        state->receiveTimeoutMilliseconds = milliseconds;
        return true;
    }

    std::uint16_t LocalPort(void* handle) noexcept
    {
        std::string error;
        if (!Retain(handle, error)) return 0;
        SocketLease lease(handle);
        auto* state = asHandle(handle);
        std::lock_guard receiveLock(state->receiveMutex);
        sockaddr_storage address{};
#if defined(_WIN32)
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        if (getsockname(state->value, reinterpret_cast<sockaddr*>(&address), &size) != 0)
            return 0;
        if (address.ss_family == AF_INET)
            return ntohs(reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
        if (address.ss_family == AF_INET6)
            return ntohs(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_port);
        return 0;
    }

    bool Send(void* handle, const std::string_view bytes, std::size_t& sent, std::string& error) noexcept
    {
        sent = 0; error.clear();
        if (!Retain(handle, error)) return false;
        SocketLease lease(handle);
        auto* state = asHandle(handle);
        std::lock_guard sendLock(state->sendMutex);
        while (sent < bytes.size())
        {
            const auto result = send(state->value, bytes.data() + sent,
                static_cast<int>(bytes.size() - sent), 0);
            if (result <= 0) { error = errorMessage("send"); return false; }
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    bool Receive(void* handle, const std::size_t maximumBytes, std::string& bytes, std::string& error) noexcept
    {
        bytes.clear(); error.clear();
        if (!Retain(handle, error)) return false;
        SocketLease lease(handle);
        auto* state = asHandle(handle);
        std::lock_guard receiveLock(state->receiveMutex);
        std::vector<char> buffer(std::max<std::size_t>(1, maximumBytes));
        NativeSocket value = invalidSocket;
        if (!waitReadable(state, true, "receive wait", value, error)) return false;
        const auto result = recv(value, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (result < 0) { error = errorMessage("receive"); return false; }
        bytes.assign(buffer.data(), static_cast<std::size_t>(result)); return true;
    }

    bool UdpBind(const std::string_view bindAddress, const std::uint16_t port,
                 void*& handle, std::string& error) noexcept
    {
        handle = nullptr; error.clear();
        if (!ensureRuntime(error)) return false;
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_DGRAM; hints.ai_flags = AI_PASSIVE;
        addrinfo* raw = nullptr;
        const std::string hostText(bindAddress); const std::string service = std::to_string(port);
        const char* hostPointer = hostText.empty() ? nullptr : hostText.c_str();
        const int lookup = getaddrinfo(hostPointer, service.c_str(), &hints, &raw);
        if (lookup != 0) { error = "UDP bind address resolution failed: " + std::string(gai_strerror(lookup)); return false; }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> entries(raw, freeaddrinfo);
        for (addrinfo* entry = raw; entry; entry = entry->ai_next)
        {
            NativeSocket socketValue = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
            if (socketValue == invalidSocket) continue;
            if (bind(socketValue, entry->ai_addr, static_cast<int>(entry->ai_addrlen)) == 0)
            {
                auto resultHandle = std::make_unique<SocketHandle>(); resultHandle->value = socketValue;
                handle = resultHandle.release(); return true;
            }
            closeNative(socketValue);
        }
        error = errorMessage("UDP bind"); return false;
    }

    bool UdpSendTo(void* handle, const std::string_view host, const std::uint16_t port,
                   const std::string_view bytes, std::size_t& sent, std::string& error) noexcept
    {
        sent = 0; error.clear();
        if (!Retain(handle, error)) return false;
        SocketLease lease(handle);
        auto* state = asHandle(handle);
        std::lock_guard sendLock(state->sendMutex);
        addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_DGRAM;
        addrinfo* raw = nullptr;
        const std::string hostText(host); const std::string service = std::to_string(port);
        const int lookup = getaddrinfo(hostText.c_str(), service.c_str(), &hints, &raw);
        if (lookup != 0) { error = "UDP destination resolution failed: " + std::string(gai_strerror(lookup)); return false; }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> entries(raw, freeaddrinfo);
        for (addrinfo* entry = raw; entry; entry = entry->ai_next)
        {
            const auto result = sendto(state->value, bytes.data(), static_cast<int>(bytes.size()), 0,
                entry->ai_addr, static_cast<int>(entry->ai_addrlen));
            if (result >= 0) { sent = static_cast<std::size_t>(result); return true; }
        }
        error = errorMessage("UDP sendto"); return false;
    }

    bool UdpReceiveFrom(void* handle, const std::size_t maximumBytes, std::string& bytes,
                        std::string& remoteAddress, std::uint16_t& remotePort, std::string& error) noexcept
    {
        bytes.clear(); remoteAddress.clear(); remotePort = 0; error.clear();
        if (!Retain(handle, error)) return false;
        SocketLease lease(handle);
        auto* state = asHandle(handle);
        std::lock_guard receiveLock(state->receiveMutex);
        std::vector<char> buffer(std::max<std::size_t>(1, maximumBytes));
        sockaddr_storage address{};
#if defined(_WIN32)
        int addressSize = sizeof(address);
#else
        socklen_t addressSize = sizeof(address);
#endif
        NativeSocket value = invalidSocket;
        if (!waitReadable(state, true, "UDP receive wait", value, error)) return false;
        const auto result = recvfrom(value, buffer.data(), static_cast<int>(buffer.size()), 0,
            reinterpret_cast<sockaddr*>(&address), &addressSize);
        if (result < 0) { error = errorMessage("UDP recvfrom"); return false; }
        bytes.assign(buffer.data(), static_cast<std::size_t>(result));
        char hostBuffer[NI_MAXHOST]{};
        if (getnameinfo(reinterpret_cast<const sockaddr*>(&address), addressSize,
                        hostBuffer, sizeof(hostBuffer), nullptr, 0, NI_NUMERICHOST) == 0)
            remoteAddress = hostBuffer;
        if (address.ss_family == AF_INET)
            remotePort = ntohs(reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
        else if (address.ss_family == AF_INET6)
            remotePort = ntohs(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_port);
        return true;
    }

    bool Retain(void* handle, std::string& error) noexcept
    {
        error.clear();
        if (!handle) { error = "socket handle is null"; return false; }
        auto* state = asHandle(handle);
        std::lock_guard lock(state->lifecycleMutex);
        if (state->closed) { error = "socket is closed"; return false; }
        state->references.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    void Release(void* handle) noexcept
    {
        if (!handle) return;
        auto* state = asHandle(handle);
        if (state->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete state;
    }

    std::uint64_t LiveSocketCount() noexcept
    {
        return liveSocketCount.load(std::memory_order_acquire);
    }

    void Close(void* handle) noexcept
    {
        if (!handle) return;
        auto* state = asHandle(handle);
        NativeSocket value = invalidSocket;
        {
            std::lock_guard lock(state->lifecycleMutex);
            if (state->closed) return;
            state->closed = true;
            value = state->value;
        }
        if (value != invalidSocket)
        {
#if defined(_WIN32)
            shutdown(value, SD_BOTH);
#else
            shutdown(value, SHUT_RDWR);
#endif
            // shutdown wakes blocking send/receive/accept. Waiting for both
            // operation lanes before close prevents the native descriptor from
            // being recycled while an already-retained operation still uses it.
            std::scoped_lock operationLock(state->sendMutex, state->receiveMutex);
            closeNative(value);
            state->value = invalidSocket;
        }
        Release(state);
    }
}
