#pragma once

#include "std_async.h"
#include "std_process.h"

#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wio::runtime::std_async_process
{
    struct OperationResult final
    {
        bool succeeded = false;
        std::int32_t exitCode = 0;
        std_process::ProcessError error = std_process::ProcessError::none;
        std::int32_t nativeError = 0;
        std::string message;
        std::string output;
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
                throw std::invalid_argument("invalid asynchronous process payload");
            std::uint64_t value = 0;
            for (unsigned shift = 0; shift < 64; shift += 8)
                value |= static_cast<std::uint64_t>(static_cast<unsigned char>(source[offset++])) << shift;
            return value;
        }

        inline std::string ReadString(const std::string& source, std::size_t& offset)
        {
            const auto size = ReadU64(source, offset);
            if (size > static_cast<std::uint64_t>(source.size() - offset))
                throw std::invalid_argument("invalid asynchronous process payload");
            std::string value = source.substr(offset, static_cast<std::size_t>(size));
            offset += static_cast<std::size_t>(size);
            return value;
        }

        inline std::string Encode(const OperationResult& result)
        {
            std::string payload("WAP1", 4);
            payload.push_back(result.succeeded ? '\1' : '\0');
            AppendU64(payload, static_cast<std::uint64_t>(static_cast<std::int64_t>(result.exitCode)));
            payload.push_back(static_cast<char>(result.error));
            AppendU64(payload, static_cast<std::uint64_t>(static_cast<std::int64_t>(result.nativeError)));
            AppendString(payload, result.message);
            AppendString(payload, result.output);
            return payload;
        }

        inline OperationResult Decode(const std::string& payload)
        {
            if (payload.size() < 6 || payload.compare(0, 4, "WAP1") != 0)
                throw std::invalid_argument("invalid asynchronous process payload");
            std::size_t offset = 4;
            OperationResult result;
            result.succeeded = payload[offset++] != '\0';
            result.exitCode = static_cast<std::int32_t>(
                std::bit_cast<std::int64_t>(ReadU64(payload, offset)));
            result.error = static_cast<std_process::ProcessError>(
                static_cast<unsigned char>(payload[offset++]));
            result.nativeError = static_cast<std::int32_t>(
                std::bit_cast<std::int64_t>(ReadU64(payload, offset)));
            result.message = ReadString(payload, offset);
            result.output = ReadString(payload, offset);
            if (offset != payload.size())
                throw std::invalid_argument("invalid asynchronous process payload");
            return result;
        }
    }

    inline AsyncTask<std::string> Run(
        const std::string& command,
        const std::vector<std::string>& args,
        const std::string& workingDirectory)
    {
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>([command, args, workingDirectory]
            {
                OperationResult value;
                int exitCode = 0;
                int nativeError = 0;
                value.succeeded = std_process::TryRunResult(
                    command, args, workingDirectory, exitCode,
                    value.error, nativeError, value.message);
                value.exitCode = static_cast<std::int32_t>(exitCode);
                value.nativeError = static_cast<std::int32_t>(nativeError);
                return value;
            }));
        co_return detail::Encode(result);
    }

    inline AsyncTask<std::string> Capture(
        const std::string& command,
        const std::vector<std::string>& args,
        const std::string& workingDirectory)
    {
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>([command, args, workingDirectory]
            {
                OperationResult value;
                int exitCode = 0;
                int nativeError = 0;
                value.succeeded = std_process::TryRunCapture(
                    command, args, workingDirectory, exitCode, value.output,
                    value.error, nativeError, value.message);
                value.exitCode = static_cast<std::int32_t>(exitCode);
                value.nativeError = static_cast<std::int32_t>(nativeError);
                return value;
            }));
        co_return detail::Encode(result);
    }

    inline void Decode(
        const std::string& payload,
        bool& succeeded,
        std::int32_t& exitCode,
        std::string& output,
        std_process::ProcessError& error,
        std::int32_t& nativeError,
        std::string& message)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        exitCode = result.exitCode;
        output = result.output;
        error = result.error;
        nativeError = result.nativeError;
        message = result.message;
    }
}
