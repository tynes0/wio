#pragma once

#include "std_async.h"
#include "std_fs.h"

#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wio::runtime::std_async_fs
{
    struct OperationResult final
    {
        bool succeeded = false;
        std::int32_t error = 0;
        std::int64_t nativeError = 0;
        std::string message;
        std::string text;
        std::vector<std::string> texts;
        bool boolean = false;
        bool isFile = false;
        bool isDirectory = false;
        std::int64_t size = -1;
        std::int64_t lastWriteTime = 0;
        bool executable = false;
    };

    namespace detail
    {
        inline void AppendU64(std::string& output, std::uint64_t value)
        {
            for (unsigned shift = 0; shift < 64; shift += 8)
                output.push_back(static_cast<char>((value >> shift) & 0xffu));
        }

        inline void AppendString(std::string& output, const std::string& value)
        {
            AppendU64(output, static_cast<std::uint64_t>(value.size()));
            output.append(value);
        }

        inline std::uint64_t ReadU64(const std::string& input, std::size_t& offset)
        {
            if (input.size() - offset < 8)
                throw std::invalid_argument("invalid asynchronous filesystem payload");
            std::uint64_t value = 0;
            for (unsigned shift = 0; shift < 64; shift += 8)
                value |= static_cast<std::uint64_t>(static_cast<unsigned char>(input[offset++])) << shift;
            return value;
        }

        inline std::string ReadString(const std::string& input, std::size_t& offset)
        {
            const auto size = ReadU64(input, offset);
            if (size > static_cast<std::uint64_t>(input.size() - offset))
                throw std::invalid_argument("invalid asynchronous filesystem payload");
            std::string value = input.substr(offset, static_cast<std::size_t>(size));
            offset += static_cast<std::size_t>(size);
            return value;
        }

        inline std::string Encode(const OperationResult& result)
        {
            std::string output("WAF1", 4);
            output.push_back(result.succeeded ? '\1' : '\0');
            AppendU64(output, static_cast<std::uint64_t>(static_cast<std::int64_t>(result.error)));
            AppendU64(output, static_cast<std::uint64_t>(result.nativeError));
            AppendString(output, result.message);
            AppendString(output, result.text);
            output.push_back(result.boolean ? '\1' : '\0');
            output.push_back(result.isFile ? '\1' : '\0');
            output.push_back(result.isDirectory ? '\1' : '\0');
            output.push_back(result.executable ? '\1' : '\0');
            AppendU64(output, static_cast<std::uint64_t>(result.size));
            AppendU64(output, static_cast<std::uint64_t>(result.lastWriteTime));
            AppendU64(output, static_cast<std::uint64_t>(result.texts.size()));
            for (const auto& value : result.texts)
                AppendString(output, value);
            return output;
        }

        inline OperationResult Decode(const std::string& input)
        {
            if (input.size() < 5 || input.compare(0, 4, "WAF1") != 0)
                throw std::invalid_argument("invalid asynchronous filesystem payload");
            std::size_t offset = 4;
            OperationResult result;
            result.succeeded = input[offset++] != '\0';
            result.error = static_cast<std::int32_t>(
                std::bit_cast<std::int64_t>(ReadU64(input, offset)));
            result.nativeError = std::bit_cast<std::int64_t>(ReadU64(input, offset));
            result.message = ReadString(input, offset);
            result.text = ReadString(input, offset);
            if (input.size() - offset < 4)
                throw std::invalid_argument("invalid asynchronous filesystem payload");
            result.boolean = input[offset++] != '\0';
            result.isFile = input[offset++] != '\0';
            result.isDirectory = input[offset++] != '\0';
            result.executable = input[offset++] != '\0';
            result.size = std::bit_cast<std::int64_t>(ReadU64(input, offset));
            result.lastWriteTime = std::bit_cast<std::int64_t>(ReadU64(input, offset));
            const auto count = ReadU64(input, offset);
            if (count > static_cast<std::uint64_t>(input.size()))
                throw std::invalid_argument("invalid asynchronous filesystem payload");
            result.texts.reserve(static_cast<std::size_t>(count));
            for (std::uint64_t index = 0; index < count; ++index)
                result.texts.push_back(ReadString(input, offset));
            if (offset != input.size())
                throw std::invalid_argument("invalid asynchronous filesystem payload");
            return result;
        }
    }

    template<typename Action>
    AsyncTask<std::string> Submit(Action action)
    {
        OperationResult result = co_await RunIoAsync<OperationResult>(
            std::function<OperationResult()>(std::move(action)));
        co_return detail::Encode(result);
    }

    inline AsyncTask<std::string> ReadText(const std::string& path)
    {
        return Submit([path]
        {
            OperationResult result;
            result.succeeded = std_fs::TryReadTextResult(
                path, result.text, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> WriteText(const std::string& path, const std::string& text)
    {
        return Submit([path, text]
        {
            OperationResult result;
            result.succeeded = std_fs::TryWriteTextResult(
                path, text, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> AppendText(const std::string& path, const std::string& text)
    {
        return Submit([path, text]
        {
            OperationResult result;
            result.succeeded = std_fs::TryAppendTextResult(
                path, text, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> CreateDirectories(const std::string& path)
    {
        return Submit([path]
        {
            OperationResult result;
            result.succeeded = std_fs::TryCreateDirectoriesResult(
                path, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> Remove(const std::string& path)
    {
        return Submit([path]
        {
            OperationResult result;
            result.succeeded = std_fs::TryRemoveResult(
                path, result.boolean, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> RemoveAll(const std::string& path)
    {
        return Submit([path]
        {
            OperationResult result;
            result.succeeded = std_fs::TryRemoveAllResult(
                path, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> CopyFile(const std::string& source, const std::string& target)
    {
        return Submit([source, target]
        {
            OperationResult result;
            result.succeeded = std_fs::TryCopyFileResult(
                source, target, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> MoveFile(const std::string& source, const std::string& target)
    {
        return Submit([source, target]
        {
            OperationResult result;
            result.succeeded = std_fs::TryMoveFileResult(
                source, target, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> ReplaceFileAtomic(const std::string& source, const std::string& target)
    {
        return Submit([source, target]
        {
            OperationResult result;
            result.succeeded = std_fs::TryReplaceFileAtomicResult(
                source, target, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> ListFilesRecursive(const std::string& path)
    {
        return Submit([path]
        {
            OperationResult result;
            result.succeeded = std_fs::TryListFilesRecursiveResult(
                path, result.texts, result.error, result.nativeError, result.message);
            return result;
        });
    }

    inline AsyncTask<std::string> Metadata(const std::string& path)
    {
        return Submit([path]
        {
            OperationResult result;
            result.succeeded = std_fs::TryMetadataResult(
                path, result.isFile, result.isDirectory, result.size,
                result.lastWriteTime, result.executable, result.error,
                result.nativeError, result.message);
            return result;
        });
    }

    inline void DecodeUnit(
        const std::string& payload, bool& succeeded, std::int32_t& error,
        std::int64_t& nativeError, std::string& message)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        error = result.error;
        nativeError = result.nativeError;
        message = result.message;
    }

    inline void DecodeText(
        const std::string& payload, bool& succeeded, std::string& value,
        std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        value = result.text;
        error = result.error;
        nativeError = result.nativeError;
        message = result.message;
    }

    inline void DecodeBoolean(
        const std::string& payload, bool& succeeded, bool& value,
        std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        value = result.boolean;
        error = result.error;
        nativeError = result.nativeError;
        message = result.message;
    }

    inline void DecodeTexts(
        const std::string& payload, bool& succeeded, std::vector<std::string>& values,
        std::int32_t& error, std::int64_t& nativeError, std::string& message)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        values = result.texts;
        error = result.error;
        nativeError = result.nativeError;
        message = result.message;
    }

    inline void DecodeMetadata(
        const std::string& payload, bool& succeeded, bool& isFile,
        bool& isDirectory, std::int64_t& size, std::int64_t& lastWriteTime,
        bool& executable, std::int32_t& error, std::int64_t& nativeError,
        std::string& message)
    {
        const auto result = detail::Decode(payload);
        succeeded = result.succeeded;
        isFile = result.isFile;
        isDirectory = result.isDirectory;
        size = result.size;
        lastWriteTime = result.lastWriteTime;
        executable = result.executable;
        error = result.error;
        nativeError = result.nativeError;
        message = result.message;
    }
}
