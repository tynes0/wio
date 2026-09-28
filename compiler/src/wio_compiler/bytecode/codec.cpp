#include "wio/bytecode/codec.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

namespace wio::bytecode
{
    namespace
    {
        class Writer final
        {
        public:
            void u8(const std::uint8_t value) { bytes_.push_back(static_cast<std::byte>(value)); }
            void u16(const std::uint16_t value)
            {
                u8(static_cast<std::uint8_t>(value));
                u8(static_cast<std::uint8_t>(value >> 8u));
            }
            void u32(const std::uint32_t value)
            {
                u16(static_cast<std::uint16_t>(value));
                u16(static_cast<std::uint16_t>(value >> 16u));
            }
            void u64(const std::uint64_t value)
            {
                u32(static_cast<std::uint32_t>(value));
                u32(static_cast<std::uint32_t>(value >> 32u));
            }
            void raw(const std::span<const std::byte> bytes) { bytes_.insert(bytes_.end(), bytes.begin(), bytes.end()); }
            void text(const std::string_view value)
            {
                u32(static_cast<std::uint32_t>(value.size()));
                raw({reinterpret_cast<const std::byte*>(value.data()), value.size()});
            }
            void patchU64(const std::size_t offset, const std::uint64_t value)
            {
                for (std::size_t i = 0; i < 8; ++i)
                    bytes_[offset + i] = static_cast<std::byte>(value >> (i * 8u));
            }
            [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
            [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
            [[nodiscard]] std::vector<std::byte> take() noexcept { return std::move(bytes_); }

        private:
            std::vector<std::byte> bytes_;
        };

        class Reader final
        {
        public:
            explicit Reader(const std::span<const std::byte> bytes) : bytes_(bytes) {}

            bool u8(std::uint8_t& value)
            {
                if (remaining() < 1)
                    return false;
                value = std::to_integer<std::uint8_t>(bytes_[offset_++]);
                return true;
            }
            bool u16(std::uint16_t& value)
            {
                std::uint8_t low = 0;
                std::uint8_t high = 0;
                if (!u8(low) || !u8(high))
                    return false;
                value = static_cast<std::uint16_t>(low | (static_cast<std::uint16_t>(high) << 8u));
                return true;
            }
            bool u32(std::uint32_t& value)
            {
                std::uint16_t low = 0;
                std::uint16_t high = 0;
                if (!u16(low) || !u16(high))
                    return false;
                value = static_cast<std::uint32_t>(low) | (static_cast<std::uint32_t>(high) << 16u);
                return true;
            }
            bool u64(std::uint64_t& value)
            {
                std::uint32_t low = 0;
                std::uint32_t high = 0;
                if (!u32(low) || !u32(high))
                    return false;
                value = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
                return true;
            }
            bool text(std::string& value, const std::uint64_t maximum)
            {
                std::uint32_t size = 0;
                if (!u32(size) || size > maximum || remaining() < size)
                    return false;
                value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
                offset_ += size;
                return true;
            }
            [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - offset_; }
            [[nodiscard]] bool finished() const noexcept { return offset_ == bytes_.size(); }

        private:
            std::span<const std::byte> bytes_;
            std::size_t offset_ = 0;
        };

        struct Section
        {
            SectionKind kind;
            std::vector<std::byte> bytes;
            std::uint32_t count = 0;
            std::uint64_t offset = 0;
        };

        struct SectionView
        {
            SectionKind kind;
            std::span<const std::byte> bytes;
            std::uint32_t count = 0;
        };

        template<typename T, typename Write>
        void writeList(Writer& writer, const std::vector<T>& values, Write&& write)
        {
            writer.u32(static_cast<std::uint32_t>(values.size()));
            for (const T& value : values)
                write(writer, value);
        }

        template<typename T, typename Read>
        bool readList(Reader& reader, std::vector<T>& values, const DecodeLimits& limits, Read&& read)
        {
            std::uint32_t count = 0;
            if (!reader.u32(count) || count > limits.maximumListElements)
                return false;
            values.clear();
            values.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                T value;
                if (!read(reader, value))
                    return false;
                values.push_back(std::move(value));
            }
            return true;
        }

        void writeLocation(Writer& writer, const SourceLocation& location)
        {
            writer.u32(location.file);
            writer.u64(location.line);
            writer.u64(location.column);
        }

        bool readLocation(Reader& reader, SourceLocation& location)
        {
            return reader.u32(location.file) && reader.u64(location.line) && reader.u64(location.column);
        }

        void writeSpan(Writer& writer, const SourceSpan& span)
        {
            writeLocation(writer, span.begin);
            writeLocation(writer, span.end);
        }

        bool readSpan(Reader& reader, SourceSpan& span)
        {
            return readLocation(reader, span.begin) && readLocation(reader, span.end);
        }

        void writeParameter(Writer& writer, const Parameter& parameter)
        {
            writer.u32(parameter.value);
            writer.u32(parameter.name);
            writer.u32(parameter.type);
            writer.u8(parameter.ownership);
            writer.u8(parameter.borrowLifetime);
            writeSpan(writer, parameter.source);
        }

        bool readParameter(Reader& reader, Parameter& parameter)
        {
            return reader.u32(parameter.value) && reader.u32(parameter.name) && reader.u32(parameter.type) &&
                   reader.u8(parameter.ownership) && reader.u8(parameter.borrowLifetime) &&
                   readSpan(reader, parameter.source);
        }

        void writeU32List(Writer& writer, const std::vector<std::uint32_t>& values)
        {
            writeList(writer, values, [](Writer& output, const std::uint32_t value) { output.u32(value); });
        }

        bool readU32List(Reader& reader, std::vector<std::uint32_t>& values, const DecodeLimits& limits)
        {
            return readList(reader, values, limits,
                            [](Reader& input, std::uint32_t& value) { return input.u32(value); });
        }

        void writeU8List(Writer& writer, const std::vector<std::uint8_t>& values)
        {
            writeList(writer, values, [](Writer& output, const std::uint8_t value) { output.u8(value); });
        }

        bool readU8List(Reader& reader, std::vector<std::uint8_t>& values, const DecodeLimits& limits)
        {
            return readList(reader, values, limits,
                            [](Reader& input, std::uint8_t& value) { return input.u8(value); });
        }

        void writeInstruction(Writer& writer, const Instruction& instruction)
        {
            writer.u16(static_cast<std::uint16_t>(instruction.opcode));
            writer.u32(instruction.result);
            writer.u32(instruction.resultType);
            writeU32List(writer, instruction.operands);
            writeList(writer, instruction.targets,
                      [](Writer& output, const BranchTarget& target)
                      {
                          output.u32(target.block);
                          writeU32List(output, target.arguments);
                      });
            writer.u32(instruction.callee);
            writer.u32(instruction.global);
            writer.u32(instruction.constant);
            writer.u8(instruction.unaryOperator);
            writer.u8(instruction.binaryOperator);
            writer.u8(instruction.conversionKind);
            writer.u32(instruction.selector);
            writer.u32(instruction.projectionIndex);
            writeU32List(writer, instruction.signatureTypes);
            writeU32List(writer, instruction.genericArguments);
            writeU8List(writer, instruction.captureKinds);
            writeU8List(writer, instruction.expandedOperands);
            writeU32List(writer, instruction.stringSegments);
            writer.u32(instruction.specializationKey);
            writer.u8(instruction.intrinsicFamily);
            writer.u8(instruction.asyncOperation);
            writer.u8(instruction.asyncExecutor);
            writer.u32(instruction.targetType);
            writer.u8(instruction.resultOwnership);
            writer.u8(instruction.borrowLifetime);
            writer.u32(instruction.borrowOrigin);
            writer.u8(instruction.storageClass);
            writer.u8(instruction.escapeClass);
            writer.u8(instruction.boundsCheck);
            writeSpan(writer, instruction.source);
        }

        bool readInstruction(Reader& reader, Instruction& instruction, const DecodeLimits& limits)
        {
            std::uint16_t opcode = 0;
            if (!reader.u16(opcode) || !reader.u32(instruction.result) || !reader.u32(instruction.resultType) ||
                !readU32List(reader, instruction.operands, limits) ||
                !readList(reader, instruction.targets, limits,
                          [&](Reader& input, BranchTarget& target)
                          { return input.u32(target.block) && readU32List(input, target.arguments, limits); }) ||
                !reader.u32(instruction.callee) || !reader.u32(instruction.global) ||
                !reader.u32(instruction.constant) || !reader.u8(instruction.unaryOperator) ||
                !reader.u8(instruction.binaryOperator) || !reader.u8(instruction.conversionKind) ||
                !reader.u32(instruction.selector) || !reader.u32(instruction.projectionIndex) ||
                !readU32List(reader, instruction.signatureTypes, limits) ||
                !readU32List(reader, instruction.genericArguments, limits) ||
                !readU8List(reader, instruction.captureKinds, limits) ||
                !readU8List(reader, instruction.expandedOperands, limits) ||
                !readU32List(reader, instruction.stringSegments, limits) ||
                !reader.u32(instruction.specializationKey) || !reader.u8(instruction.intrinsicFamily) ||
                !reader.u8(instruction.asyncOperation) || !reader.u8(instruction.asyncExecutor) ||
                !reader.u32(instruction.targetType) || !reader.u8(instruction.resultOwnership) ||
                !reader.u8(instruction.borrowLifetime) || !reader.u32(instruction.borrowOrigin) ||
                !reader.u8(instruction.storageClass) || !reader.u8(instruction.escapeClass) ||
                !reader.u8(instruction.boundsCheck) || !readSpan(reader, instruction.source))
                return false;
            instruction.opcode = static_cast<Opcode>(opcode);
            return true;
        }

        Section makeManifestSection(const Module& module)
        {
            Writer writer;
            writer.u32(module.name);
            writer.u8(module.moduleKind);
            writer.u64(module.stableId);
            writer.u32(module.logicalName);
            writer.u32(module.stableKey);
            writer.u32(module.abiDescriptorVersion);
            return {SectionKind::Manifest, writer.take(), 1};
        }

        Section makeStringsSection(const Module& module)
        {
            Writer writer;
            writer.u32(static_cast<std::uint32_t>(module.strings.size()));
            for (const std::string& value : module.strings)
                writer.text(value);
            return {SectionKind::Strings, writer.take(), static_cast<std::uint32_t>(module.strings.size())};
        }

        Section makeConstantsSection(const Module& module)
        {
            Writer writer;
            writer.u32(static_cast<std::uint32_t>(module.constants.size()));
            for (const Constant& constant : module.constants)
            {
                writer.u8(static_cast<std::uint8_t>(constant.kind));
                writer.u64(constant.bits);
                writer.u32(constant.string);
            }
            return {SectionKind::Constants, writer.take(), static_cast<std::uint32_t>(module.constants.size())};
        }

        Section makeTypesSection(const Module& module)
        {
            Writer writer;
            writer.u32(static_cast<std::uint32_t>(module.types.size()));
            for (const Type& type : module.types)
            {
                writer.u8(type.kind);
                writer.u32(type.name);
                writeU32List(writer, type.arguments);
                writer.u64(type.staticExtent);
                writer.u32(type.extentParameter);
                writer.u8(type.nominalKind);
                writer.u8(type.nominalRepresentation);
                writer.u8(type.nominalValueModel);
                writer.u8(type.ownership);
                writer.u8(type.cleanup);
                writer.u8(type.flags);
            }
            return {SectionKind::Types, writer.take(), static_cast<std::uint32_t>(module.types.size())};
        }

        Section makeGlobalsSection(const Module& module)
        {
            Writer writer;
            writer.u32(static_cast<std::uint32_t>(module.globals.size()));
            for (const Global& global : module.globals)
            {
                writer.u32(global.id);
                writer.u32(global.name);
                writer.u32(global.type);
                writer.u32(global.initializer);
                writeSpan(writer, global.source);
                writer.u8(global.flags);
            }
            return {SectionKind::Globals, writer.take(), static_cast<std::uint32_t>(module.globals.size())};
        }

        Section makeFunctionsSection(const Module& module)
        {
            Writer writer;
            writer.u32(static_cast<std::uint32_t>(module.functions.size()));
            for (const Function& function : module.functions)
            {
                writer.u32(function.id);
                writer.u32(function.name);
                writeList(writer, function.parameters, writeParameter);
                writer.u32(function.returnType);
                writer.u32(function.callableType);
                writer.u32(function.ownerType);
                writer.u32(function.methodSlot);
                writer.u32(function.captureParameterCount);
                writeU32List(writer, function.genericParameters);
                writer.u32(function.genericOrigin);
                writeU32List(writer, function.specializationArguments);
                writer.u32(function.specializationKey);
                writeSpan(writer, function.source);
                writer.u16(function.flags);
            }
            return {SectionKind::Functions, writer.take(), static_cast<std::uint32_t>(module.functions.size())};
        }

        Section makeCodeSection(const Module& module)
        {
            Writer writer;
            writer.u32(static_cast<std::uint32_t>(module.functions.size()));
            for (std::uint32_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex)
            {
                writer.u32(functionIndex);
                const Function& function = module.functions[functionIndex];
                writeList(writer, function.blocks,
                          [](Writer& output, const Block& block)
                          {
                              output.u32(block.id);
                              output.u32(block.name);
                              writeList(output, block.parameters, writeParameter);
                              writeList(output, block.instructions, writeInstruction);
                              writeSpan(output, block.source);
                          });
            }
            return {SectionKind::Code, writer.take(), static_cast<std::uint32_t>(module.functions.size())};
        }

        bool sectionEndValid(const std::uint64_t offset, const std::uint64_t size, const std::uint64_t fileSize)
        {
            return offset <= fileSize && size <= fileSize - offset;
        }
    } // namespace

    std::uint64_t payloadChecksum(const std::span<const std::byte> bytes) noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        for (const std::byte byte : bytes)
        {
            hash ^= std::to_integer<std::uint8_t>(byte);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    std::vector<std::byte> encode(const Module& module)
    {
        std::array<Section, 7> sections{
            makeManifestSection(module), makeStringsSection(module), makeConstantsSection(module),
            makeTypesSection(module), makeGlobalsSection(module), makeFunctionsSection(module), makeCodeSection(module)};
        const std::uint64_t directoryOffset = HeaderSize;
        std::uint64_t nextOffset = HeaderSize + SectionEntrySize * sections.size();
        for (Section& section : sections)
        {
            section.offset = nextOffset;
            nextOffset += section.bytes.size();
        }

        Writer writer;
        writer.raw(Magic);
        writer.u16(FormatMajor);
        writer.u16(FormatMinor);
        writer.u32(HeaderSize);
        writer.u32(LittleEndianMarker);
        writer.u32(static_cast<std::uint32_t>(sections.size()));
        writer.u64(directoryOffset);
        writer.u64(nextOffset);
        writer.u64(0);
        for (const Section& section : sections)
        {
            writer.u32(static_cast<std::uint32_t>(section.kind));
            writer.u32(0);
            writer.u64(section.offset);
            writer.u64(section.bytes.size());
            writer.u32(section.count);
            writer.u32(0);
        }
        for (const Section& section : sections)
            writer.raw(section.bytes);
        writer.patchU64(40, payloadChecksum(std::span{writer.bytes()}.subspan(HeaderSize)));
        return writer.take();
    }

    DecodeResult decode(const std::span<const std::byte> bytes, const DecodeLimits& limits)
    {
        DecodeResult result;
        auto fail = [&](const std::string& message) -> DecodeResult
        {
            result.error = message;
            return std::move(result);
        };
        if (bytes.size() < HeaderSize || bytes.size() > limits.maximumFileBytes)
            return fail("Bytecode size is outside the configured limits");
        if (!std::equal(Magic.begin(), Magic.end(), bytes.begin()))
            return fail("Invalid WIOB magic");

        Reader header{bytes.subspan(8, HeaderSize - 8)};
        std::uint16_t major = 0;
        std::uint16_t minor = 0;
        std::uint32_t headerSize = 0;
        std::uint32_t endian = 0;
        std::uint32_t sectionCount = 0;
        std::uint64_t directoryOffset = 0;
        std::uint64_t fileSize = 0;
        std::uint64_t checksum = 0;
        if (!header.u16(major) || !header.u16(minor) || !header.u32(headerSize) || !header.u32(endian) ||
            !header.u32(sectionCount) || !header.u64(directoryOffset) || !header.u64(fileSize) ||
            !header.u64(checksum))
            return fail("Truncated WIOB header");
        if (major != FormatMajor || minor > FormatMinor)
            return fail("Unsupported WIOB format version");
        if (headerSize != HeaderSize || endian != LittleEndianMarker || fileSize != bytes.size())
            return fail("Invalid WIOB header values");
        if (sectionCount == 0 || sectionCount > limits.maximumSections ||
            directoryOffset > bytes.size() || sectionCount > (bytes.size() - directoryOffset) / SectionEntrySize)
            return fail("Invalid WIOB section directory");
        if (payloadChecksum(bytes.subspan(HeaderSize)) != checksum)
            return fail("WIOB payload checksum mismatch");

        Reader directory{bytes.subspan(static_cast<std::size_t>(directoryOffset),
                                       static_cast<std::size_t>(sectionCount) * SectionEntrySize)};
        const std::uint64_t minimumSectionOffset =
            directoryOffset + static_cast<std::uint64_t>(sectionCount) * SectionEntrySize;
        std::unordered_map<std::uint32_t, SectionView> sections;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        for (std::uint32_t index = 0; index < sectionCount; ++index)
        {
            std::uint32_t kind = 0;
            std::uint32_t flags = 0;
            std::uint64_t offset = 0;
            std::uint64_t size = 0;
            std::uint32_t count = 0;
            std::uint32_t reserved = 0;
            if (!directory.u32(kind) || !directory.u32(flags) || !directory.u64(offset) || !directory.u64(size) ||
                !directory.u32(count) || !directory.u32(reserved))
                return fail("Truncated WIOB section entry");
            if (flags != 0 || reserved != 0 || count > limits.maximumRecords ||
                !sectionEndValid(offset, size, bytes.size()) || offset < minimumSectionOffset)
                return fail("Invalid WIOB section entry");
            if (!sections.emplace(kind,
                                  SectionView{static_cast<SectionKind>(kind),
                                              bytes.subspan(static_cast<std::size_t>(offset),
                                                            static_cast<std::size_t>(size)),
                                              count})
                     .second)
                return fail("Duplicate WIOB section");
            ranges.emplace_back(offset, offset + size);
        }
        std::ranges::sort(ranges);
        for (std::size_t i = 1; i < ranges.size(); ++i)
        {
            if (ranges[i].first < ranges[i - 1].second)
                return fail("Overlapping WIOB sections");
        }
        auto require = [&](const SectionKind kind) -> const SectionView*
        {
            const auto found = sections.find(static_cast<std::uint32_t>(kind));
            return found == sections.end() ? nullptr : &found->second;
        };
        const SectionView* manifest = require(SectionKind::Manifest);
        const SectionView* strings = require(SectionKind::Strings);
        const SectionView* constants = require(SectionKind::Constants);
        const SectionView* types = require(SectionKind::Types);
        const SectionView* globals = require(SectionKind::Globals);
        const SectionView* functions = require(SectionKind::Functions);
        const SectionView* code = require(SectionKind::Code);
        if (!manifest || !strings || !constants || !types || !globals || !functions || !code)
            return fail("WIOB file is missing a required section");

        {
            Reader reader{manifest->bytes};
            if (!reader.u32(result.module.name) || !reader.u8(result.module.moduleKind) ||
                !reader.u64(result.module.stableId) || !reader.u32(result.module.logicalName) ||
                !reader.u32(result.module.stableKey) || !reader.u32(result.module.abiDescriptorVersion) ||
                !reader.finished())
                return fail("Malformed WIOB manifest section");
        }
        {
            Reader reader{strings->bytes};
            std::uint32_t count = 0;
            if (!reader.u32(count) || count != strings->count || count > limits.maximumStrings)
                return fail("Malformed WIOB string table");
            result.module.strings.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                std::string value;
                if (!reader.text(value, limits.maximumFileBytes))
                    return fail("Malformed WIOB string entry");
                result.module.strings.push_back(std::move(value));
            }
            if (!reader.finished())
                return fail("Trailing data in WIOB string table");
        }
        {
            Reader reader{constants->bytes};
            std::uint32_t count = 0;
            if (!reader.u32(count) || count != constants->count || count > limits.maximumRecords)
                return fail("Malformed WIOB constant table");
            result.module.constants.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Constant constant;
                std::uint8_t kind = 0;
                if (!reader.u8(kind) || !reader.u64(constant.bits) || !reader.u32(constant.string))
                    return fail("Malformed WIOB constant entry");
                constant.kind = static_cast<ConstantKind>(kind);
                result.module.constants.push_back(constant);
            }
            if (!reader.finished())
                return fail("Trailing data in WIOB constant table");
        }
        {
            Reader reader{types->bytes};
            std::uint32_t count = 0;
            if (!reader.u32(count) || count != types->count || count > limits.maximumRecords)
                return fail("Malformed WIOB type table");
            result.module.types.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Type type;
                if (!reader.u8(type.kind) || !reader.u32(type.name) || !readU32List(reader, type.arguments, limits) ||
                    !reader.u64(type.staticExtent) || !reader.u32(type.extentParameter) ||
                    !reader.u8(type.nominalKind) || !reader.u8(type.nominalRepresentation) ||
                    !reader.u8(type.nominalValueModel) || !reader.u8(type.ownership) || !reader.u8(type.cleanup) ||
                    !reader.u8(type.flags))
                    return fail("Malformed WIOB type entry");
                result.module.types.push_back(std::move(type));
            }
            if (!reader.finished())
                return fail("Trailing data in WIOB type table");
        }
        {
            Reader reader{globals->bytes};
            std::uint32_t count = 0;
            if (!reader.u32(count) || count != globals->count || count > limits.maximumRecords)
                return fail("Malformed WIOB global table");
            result.module.globals.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Global global;
                if (!reader.u32(global.id) || !reader.u32(global.name) || !reader.u32(global.type) ||
                    !reader.u32(global.initializer) || !readSpan(reader, global.source) || !reader.u8(global.flags))
                    return fail("Malformed WIOB global entry");
                result.module.globals.push_back(std::move(global));
            }
            if (!reader.finished())
                return fail("Trailing data in WIOB global table");
        }
        {
            Reader reader{functions->bytes};
            std::uint32_t count = 0;
            if (!reader.u32(count) || count != functions->count || count > limits.maximumRecords)
                return fail("Malformed WIOB function table");
            result.module.functions.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                Function function;
                if (!reader.u32(function.id) || !reader.u32(function.name) ||
                    !readList(reader, function.parameters, limits, readParameter) ||
                    !reader.u32(function.returnType) || !reader.u32(function.callableType) ||
                    !reader.u32(function.ownerType) || !reader.u32(function.methodSlot) ||
                    !reader.u32(function.captureParameterCount) ||
                    !readU32List(reader, function.genericParameters, limits) || !reader.u32(function.genericOrigin) ||
                    !readU32List(reader, function.specializationArguments, limits) ||
                    !reader.u32(function.specializationKey) || !readSpan(reader, function.source) ||
                    !reader.u16(function.flags))
                    return fail("Malformed WIOB function entry");
                result.module.functions.push_back(std::move(function));
            }
            if (!reader.finished())
                return fail("Trailing data in WIOB function table");
        }
        {
            Reader reader{code->bytes};
            std::uint32_t count = 0;
            if (!reader.u32(count) || count != code->count || count != result.module.functions.size())
                return fail("Malformed WIOB code section");
            std::vector<bool> decoded(count, false);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                std::uint32_t functionIndex = 0;
                if (!reader.u32(functionIndex) || functionIndex >= count || decoded[functionIndex])
                    return fail("Invalid WIOB code function index");
                decoded[functionIndex] = true;
                Function& function = result.module.functions[functionIndex];
                if (!readList(reader, function.blocks, limits,
                              [&](Reader& input, Block& block)
                              {
                                  return input.u32(block.id) && input.u32(block.name) &&
                                         readList(input, block.parameters, limits, readParameter) &&
                                         readList(input, block.instructions, limits,
                                                  [&](Reader& nested, Instruction& instruction)
                                                  { return readInstruction(nested, instruction, limits); }) &&
                                         readSpan(input, block.source);
                              }))
                    return fail("Malformed WIOB code body");
            }
            if (!reader.finished())
                return fail("Trailing data in WIOB code section");
        }
        return result;
    }
} // namespace wio::bytecode
