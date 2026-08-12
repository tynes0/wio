#pragma once

#include "std_async.h"
#include "std_net.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wio::runtime::std_async_net
{
    struct OperationResult final
    {
        bool succeeded = false;
        std::string error;
        std::size_t count = 0;
        std::string bytes;
        std::string remoteAddress;
        std::uint16_t remotePort = 0;
        std::vector<std::string> addresses;
    };

    namespace detail
    {
        inline void AppendU64(std::string& target, std::uint64_t value)
        {
            for (unsigned shift = 0; shift < 64; shift += 8)
                target.push_back(static_cast<char>((value >> shift) & 0xffu));
        }

        inline void AppendString(std::string& target, const std::string& value)
        {
            AppendU64(target, static_cast<std::uint64_t>(value.size()));
            target.append(value);
        }

        inline std::uint64_t ReadU64(const std::string& source, std::size_t& offset)
        {
            if (offset > source.size() || source.size() - offset < 8)
                throw std::invalid_argument("invalid asynchronous network payload");
            std::uint64_t value = 0;
            for (unsigned shift = 0; shift < 64; shift += 8)
                value |= static_cast<std::uint64_t>(static_cast<unsigned char>(source[offset++])) << shift;
            return value;
        }

        inline std::string ReadString(const std::string& source, std::size_t& offset)
        {
            const auto size = ReadU64(source, offset);
            if (size > static_cast<std::uint64_t>(source.size() - offset))
                throw std::invalid_argument("invalid asynchronous network payload");
            std::string value = source.substr(offset, static_cast<std::size_t>(size));
            offset += static_cast<std::size_t>(size);
            return value;
        }

        inline std::string Encode(const OperationResult& result)
        {
            std::string payload("WAN1", 4);
            payload.push_back(result.succeeded ? '\1' : '\0');
            AppendString(payload, result.error);
            AppendU64(payload, static_cast<std::uint64_t>(result.count));
            AppendString(payload, result.bytes);
            AppendString(payload, result.remoteAddress);
            AppendU64(payload, result.remotePort);
            AppendU64(payload, static_cast<std::uint64_t>(result.addresses.size()));
            for (const auto& address : result.addresses)
                AppendString(payload, address);
            return payload;
        }

        inline OperationResult Decode(const std::string& payload)
        {
            if (payload.size() < 5 || payload.compare(0, 4, "WAN1") != 0)
                throw std::invalid_argument("invalid asynchronous network payload");
            std::size_t offset = 4;
            OperationResult result;
            result.succeeded = payload[offset++] != '\0';
            result.error = ReadString(payload, offset);
            result.count = static_cast<std::size_t>(ReadU64(payload, offset));
            result.bytes = ReadString(payload, offset);
            result.remoteAddress = ReadString(payload, offset);
            result.remotePort = static_cast<std::uint16_t>(ReadU64(payload, offset));
            const auto addressCount = ReadU64(payload, offset);
            if (addressCount > static_cast<std::uint64_t>(payload.size()))
                throw std::invalid_argument("invalid asynchronous network payload");
            result.addresses.reserve(static_cast<std::size_t>(addressCount));
            for (std::uint64_t index = 0; index < addressCount; ++index)
                result.addresses.push_back(ReadString(payload, offset));
            if (offset != payload.size())
                throw std::invalid_argument("invalid asynchronous network payload");
            return result;
        }

        inline std::shared_ptr<void> Acquire(void* handle, std::string& error)
        {
            if (!std_net::Retain(handle, error))
                return {};
            return std::shared_ptr<void>(handle, [](void* value) { std_net::Release(value); });
        }
    }

    inline AsyncTask<std::string> Resolve(const std::string& host, const std::uint16_t port)
    {
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>([host, port]
            {
                OperationResult value;
                value.succeeded = std_net::Resolve(host, port, value.addresses, value.error);
                return value;
            }));
        co_return detail::Encode(result);
    }

    inline AsyncTask<std::string> Send(void* handle, const std::string& bytes)
    {
        std::string error;
        auto lease = detail::Acquire(handle, error);
        if (!lease)
            co_return detail::Encode(OperationResult{.error = std::move(error)});
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>([lease = std::move(lease), bytes]
            {
                OperationResult value;
                value.succeeded = std_net::Send(lease.get(), bytes, value.count, value.error);
                return value;
            }));
        co_return detail::Encode(result);
    }

    inline AsyncTask<std::string> Receive(void* handle, const std::size_t maximumBytes)
    {
        std::string error;
        auto lease = detail::Acquire(handle, error);
        if (!lease)
            co_return detail::Encode(OperationResult{.error = std::move(error)});
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>([lease = std::move(lease), maximumBytes]
            {
                OperationResult value;
                value.succeeded = std_net::Receive(
                    lease.get(), maximumBytes, value.bytes, value.error);
                return value;
            }));
        co_return detail::Encode(result);
    }

    inline AsyncTask<std::string> UdpSendTo(
        void* handle, const std::string& host, const std::uint16_t port,
        const std::string& bytes)
    {
        std::string error;
        auto lease = detail::Acquire(handle, error);
        if (!lease)
            co_return detail::Encode(OperationResult{.error = std::move(error)});
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>(
                [lease = std::move(lease), host, port, bytes]
                {
                    OperationResult value;
                    value.succeeded = std_net::UdpSendTo(
                        lease.get(), host, port, bytes, value.count, value.error);
                    return value;
                }));
        co_return detail::Encode(result);
    }

    inline AsyncTask<std::string> UdpReceiveFrom(void* handle, const std::size_t maximumBytes)
    {
        std::string error;
        auto lease = detail::Acquire(handle, error);
        if (!lease)
            co_return detail::Encode(OperationResult{.error = std::move(error)});
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>([lease = std::move(lease), maximumBytes]
            {
                OperationResult value;
                value.succeeded = std_net::UdpReceiveFrom(
                    lease.get(), maximumBytes, value.bytes, value.remoteAddress,
                    value.remotePort, value.error);
                return value;
            }));
        co_return detail::Encode(result);
    }

    inline void DecodeAddresses(
        const std::string& payload, bool& succeeded,
        std::vector<std::string>& addresses, std::string& error)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        addresses = result.addresses;
        error = result.error;
    }

    inline void DecodeSend(
        const std::string& payload, bool& succeeded,
        std::size_t& count, std::string& error)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        count = result.count;
        error = result.error;
    }

    inline void DecodeReceive(
        const std::string& payload, bool& succeeded,
        std::string& bytes, std::string& error)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        bytes = result.bytes;
        error = result.error;
    }

    inline void DecodeDatagram(
        const std::string& payload, bool& succeeded, std::string& bytes,
        std::string& remoteAddress, std::uint16_t& remotePort, std::string& error)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        bytes = result.bytes;
        remoteAddress = result.remoteAddress;
        remotePort = result.remotePort;
        error = result.error;
    }
}
