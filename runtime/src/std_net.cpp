#include "std_net.h"

#include <algorithm>
#include <cstring>
#include <memory>

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
        struct SocketHandle { NativeSocket value = invalidSocket; };
        std::string errorMessage(const char* operation)
        {
            return std::string(operation) + " failed with native socket error " + std::to_string(lastSocketError()) + ".";
        }
        SocketHandle* asHandle(void* value) noexcept { return static_cast<SocketHandle*>(value); }

        bool ensureRuntime(std::string& error) noexcept
        {
#if defined(_WIN32)
            if (!runtime().ready) { error = "Winsock initialization failed."; return false; }
#endif
            return true;
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
        NativeSocket accepted = accept(asHandle(listener)->value, nullptr, nullptr);
        if (accepted == invalidSocket) { error = errorMessage("accept"); return false; }
        auto result = std::make_unique<SocketHandle>(); result->value = accepted; handle = result.release(); return true;
    }

    bool SetTimeout(void* handle, const std::uint64_t milliseconds, std::string& error) noexcept
    {
        error.clear();
#if defined(_WIN32)
        const DWORD timeout = static_cast<DWORD>(std::min<std::uint64_t>(milliseconds, 0xffffffffu));
        const char* data = reinterpret_cast<const char*>(&timeout); const int size = sizeof(timeout);
#else
        const timeval timeout{ static_cast<time_t>(milliseconds / 1000u), static_cast<suseconds_t>((milliseconds % 1000u) * 1000u) };
        const char* data = reinterpret_cast<const char*>(&timeout); const socklen_t size = sizeof(timeout);
#endif
        const NativeSocket value = asHandle(handle)->value;
        if (setsockopt(value, SOL_SOCKET, SO_RCVTIMEO, data, size) != 0 ||
            setsockopt(value, SOL_SOCKET, SO_SNDTIMEO, data, size) != 0)
        { error = errorMessage("setsockopt"); return false; }
        return true;
    }

    std::uint16_t LocalPort(void* handle) noexcept
    {
        sockaddr_storage address{};
#if defined(_WIN32)
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        if (getsockname(asHandle(handle)->value, reinterpret_cast<sockaddr*>(&address), &size) != 0)
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
        while (sent < bytes.size())
        {
            const auto result = send(asHandle(handle)->value, bytes.data() + sent,
                static_cast<int>(bytes.size() - sent), 0);
            if (result <= 0) { error = errorMessage("send"); return false; }
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    bool Receive(void* handle, const std::size_t maximumBytes, std::string& bytes, std::string& error) noexcept
    {
        bytes.clear(); error.clear();
        std::vector<char> buffer(std::max<std::size_t>(1, maximumBytes));
        const auto result = recv(asHandle(handle)->value, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (result < 0) { error = errorMessage("receive"); return false; }
        bytes.assign(buffer.data(), static_cast<std::size_t>(result)); return true;
    }

    void Close(void* handle) noexcept
    {
        if (!handle) return;
        auto* socketHandle = asHandle(handle); closeNative(socketHandle->value); delete socketHandle;
    }
}
