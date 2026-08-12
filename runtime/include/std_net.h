#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::runtime::std_net
{
    [[nodiscard]] bool Resolve(
        std::string_view host,
        std::uint16_t port,
        std::vector<std::string>& addresses,
        std::string& error) noexcept;
    [[nodiscard]] bool TcpConnect(
        std::string_view host,
        std::uint16_t port,
        std::uint64_t timeoutMilliseconds,
        void*& handle,
        std::string& error) noexcept;
    [[nodiscard]] bool TcpListen(
        std::string_view bindAddress,
        std::uint16_t port,
        std::int32_t backlog,
        void*& handle,
        std::string& error) noexcept;
    [[nodiscard]] bool TcpAccept(void* listener, void*& handle, std::string& error) noexcept;
    [[nodiscard]] bool SetTimeout(void* handle, std::uint64_t milliseconds, std::string& error) noexcept;
    [[nodiscard]] std::uint16_t LocalPort(void* handle) noexcept;
    [[nodiscard]] bool Send(void* handle, std::string_view bytes, std::size_t& sent, std::string& error) noexcept;
    [[nodiscard]] bool Receive(void* handle, std::size_t maximumBytes, std::string& bytes, std::string& error) noexcept;
    [[nodiscard]] bool UdpBind(
        std::string_view bindAddress,
        std::uint16_t port,
        void*& handle,
        std::string& error) noexcept;
    [[nodiscard]] bool UdpSendTo(
        void* handle,
        std::string_view host,
        std::uint16_t port,
        std::string_view bytes,
        std::size_t& sent,
        std::string& error) noexcept;
    [[nodiscard]] bool UdpReceiveFrom(
        void* handle,
        std::size_t maximumBytes,
        std::string& bytes,
        std::string& remoteAddress,
        std::uint16_t& remotePort,
        std::string& error) noexcept;
    [[nodiscard]] bool Retain(void* handle, std::string& error) noexcept;
    [[nodiscard]] std::uint64_t LiveSocketCount() noexcept;
    void Release(void* handle) noexcept;
    void Close(void* handle) noexcept;
}
