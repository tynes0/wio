#include "wio/codegen/native_dependency_snapshot.h"

#include "std_hash.h"
#include "std_serialization.h"
#include "wio/common/filesystem/filesystem.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wio::codegen
{
    namespace
    {
        constexpr std::size_t FileLimit = 512 * 1024 * 1024;
        constexpr std::size_t TotalLimit = 1024 * 1024 * 1024;
        constexpr std::size_t DependencyLimit = 4 * 1024 * 1024;
        constexpr std::size_t InputLimit = 16384;

        std::pair<bool, std::string> environmentValue(const char* name)
        {
#ifdef _WIN32
            std::wstring key;
            for (const char* c = name; *c; ++c)
                key += static_cast<wchar_t>(*c);
            SetLastError(ERROR_SUCCESS);
            const auto required = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
            if (required == 0)
                return {GetLastError() != ERROR_ENVVAR_NOT_FOUND, {}};
            std::wstring value(required, L'\0');
            const auto copied = GetEnvironmentVariableW(key.c_str(), value.data(), required);
            if (copied >= required)
                throw std::runtime_error("Native dependency environment changed while being read");
            value.resize(copied);
            return {true, common::filesystem::toUtf8(std::filesystem::path(value))};
#else
            const auto* value = std::getenv(name);
            return {value != nullptr, value ? value : ""};
#endif
        }

        std::string pathText(const std::filesystem::path& path)
        {
            return common::filesystem::toGenericUtf8(std::filesystem::absolute(path).lexically_normal());
        }

        std::string resolvedText(const std::filesystem::path& path)
        {
            std::error_code error;
            const auto resolved = std::filesystem::weakly_canonical(path, error);
            return error ? pathText(path) : pathText(resolved);
        }

        bool samePath(const std::filesystem::path& left, const std::filesystem::path& right)
        {
            std::error_code error;
            if (std::filesystem::equivalent(left, right, error) && !error)
                return true;
            auto a = resolvedText(left);
            auto b = resolvedText(right);
#ifdef _WIN32
            const auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
            std::ranges::transform(a, a.begin(), lower);
            std::ranges::transform(b, b.begin(), lower);
#endif
            return a == b;
        }

        std::string trim(std::string value)
        {
            const auto begin = value.find_first_not_of(" \r\n\t");
            if (begin == std::string::npos)
                return {};
            return value.substr(begin, value.find_last_not_of(" \r\n\t") - begin + 1);
        }

        std::string jsonArray(const std::vector<std::string>& values)
        {
            std::string out = "[";
            for (const auto& value : values)
            {
                if (out.size() != 1)
                    out += ',';
                out += runtime::std_serialization::JsonQuote(value);
            }
            return out + ']';
        }
    } // namespace

    NativeDependencySnapshot::NativeDependencySnapshot(NativeDependencyConfig config, Runner runner)
        : config_(std::move(config)), runner_(std::move(runner))
    {
        if (const auto resolved = resolveTool(common::filesystem::toUtf8(config_.compiler)); !resolved.empty())
            config_.compiler = resolved;
    }

    void NativeDependencySnapshot::reason(std::string message)
    {
        if (std::ranges::find(reasons_, message) == reasons_.end())
            reasons_.push_back(std::move(message));
    }

    bool NativeDependencySnapshot::readBounded(const std::filesystem::path& path, std::string& bytes,
                                               std::size_t limit) const
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size > limit)
            return false;
        bytes.resize(static_cast<std::size_t>(size));
        std::ifstream stream(path, std::ios::binary);
        if (!stream || !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
            return false;
        // A file growing during the read is not a coherent observation either.
        return stream.peek() == std::char_traits<char>::eof();
    }

    void NativeDependencySnapshot::addInput(const std::filesystem::path& path, std::string_view role)
    {
        const auto key = pathText(path);
        observedPaths_.push_back(path);
        if (inputs_.contains(key))
            return;
        if (inputs_.size() >= InputLimit)
        {
            reason("input-count-limit");
            return;
        }
        std::string bytes;
        if (!readBounded(path, bytes, std::min(FileLimit, TotalLimit - hashedBytes_)))
        {
            reason("unreadable-or-oversized-input: " + key);
            return;
        }
        hashedBytes_ += bytes.size();
        inputs_.emplace(
            key, Input{key, resolvedText(path), runtime::std_hash::Sha256(bytes), std::string(role), bytes.size()});
    }

    std::filesystem::path NativeDependencySnapshot::resolveTool(const std::string& value) const
    {
        const auto name = common::filesystem::fromUtf8(trim(value));
        if (name.empty())
            return {};
        if (name.has_parent_path())
            return std::filesystem::absolute(name);
        std::vector<std::filesystem::path> directories{config_.compiler.parent_path()};
        const auto [hasPath, environment] = environmentValue("PATH");
        if (hasPath)
        {
            std::stringstream parts(environment);
            std::string part;
#ifdef _WIN32
            constexpr char separator = ';';
#else
            constexpr char separator = ':';
#endif
            while (std::getline(parts, part, separator))
                directories.push_back(part.empty() ? std::filesystem::current_path()
                                                   : common::filesystem::fromUtf8(part));
        }
        for (const auto& directory : directories)
        {
            auto candidate = directory / name;
#ifdef _WIN32
            if (!candidate.has_extension())
                candidate += ".exe";
#endif
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error))
                return std::filesystem::absolute(candidate);
        }
        return {};
    }

    std::string NativeDependencySnapshot::probe(const std::string& argument)
    {
        const auto result = runner_({common::filesystem::toUtf8(config_.compiler), argument});
        if (result.exitCode != 0 || result.output.size() > DependencyLimit)
        {
            reason("toolchain-probe-failed: " + argument);
            return {};
        }
        probes_[argument] = runtime::std_hash::Sha256(trim(result.output));
        return trim(result.output);
    }

    bool NativeDependencySnapshot::preprocessingArguments(std::vector<std::string>& result)
    {
        // Never forward unknown user flags to an extra process. In particular,
        // response files, plugins, dependency/output flags and modules can have
        // side effects or add inputs not represented by a Make dependency file.
        const auto& arguments = config_.compileArguments;
        bool safe = true;
        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            const auto& arg = arguments[i];
            if (arg == "-I" || arg == "-D" || arg == "-U" || arg == "-isystem" || arg == "-iquote" ||
                arg == "-include" || arg == "-imacros" || arg == "-isysroot" || arg == "--sysroot" ||
                arg == "-target" || arg == "--target")
            {
                if (++i == arguments.size())
                {
                    reason("missing-option-value: " + arg);
                    return false;
                }
                result.push_back(arg);
                result.push_back(arguments[i]);
            }
            else if (arg == "-shared" || arg == "-static" || arg == "-Wa,-mbig-obj")
            {
                // Recorded in the ordered invocation, not a preprocessor option.
            }
            else if (arg == "-pthread" || arg == "-fPIC" || arg == "-fpic" || arg == "-fPIE" ||
                     arg == "-fno-exceptions" || arg == "-fno-rtti" || arg == "-fexceptions" || arg == "-frtti" ||
                     arg == "-ffast-math" || arg == "-fno-fast-math" || arg == "-fwrapv" ||
                     arg == "-fno-strict-aliasing" || arg == "-pedantic" || arg == "-pedantic-errors" || arg == "-w" ||
                     arg == "-g" || arg == "-g0" || arg.starts_with("-std=") || arg.starts_with("--sysroot=") ||
                     arg.starts_with("--target=") || arg.starts_with("-march=") || arg.starts_with("-mtune=") ||
                     arg == "-m32" || arg == "-m64" || arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3" ||
                     arg == "-Os" || arg == "-Oz" || arg == "-Ofast" || arg == "-Og" ||
                     (arg.size() > 2 && (arg.starts_with("-I") || arg.starts_with("-D") || arg.starts_with("-U"))) ||
                     (arg.starts_with("-W") && !arg.starts_with("-Wp,") && !arg.starts_with("-Wa,") &&
                      !arg.starts_with("-Wl,")))
            {
                result.push_back(arg);
            }
            else
            {
                safe = false;
                reason("unclassified-compile-option: " + arg);
            }
        }
        return safe;
    }

    void NativeDependencySnapshot::captureBeforeBuild()
    {
        addInput(config_.compiler, "compiler");
        addInput(config_.generator, "wio-generator");
        addInput(config_.generatedSource, "generated-source");
        for (const auto& source : config_.sources)
            addInput(source, "native-source");
        for (const auto& input : config_.explicitInputs)
            addInput(input, "explicit-native-input");
        for (const char* key : {"PATH", "CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH",
                                "COMPILER_PATH", "GCC_EXEC_PREFIX", "SDKROOT", "DEVELOPER_DIR", "INCLUDE", "LIB",
                                "LANG", "LC_ALL", "SOURCE_DATE_EPOCH", "MACOSX_DEPLOYMENT_TARGET"})
        {
            const auto [present, value] = environmentValue(key);
            environment_[key] = present ? runtime::std_hash::Sha256(std::string("set:") + value) : "unset";
        }
        const auto version = probe("--version");
        const bool clang = version.find("clang") != std::string::npos;
        probe("-dumpmachine");
        probe("-print-search-dirs");
        if (clang)
            probe("-print-resource-dir");
        else
        {
            probe("-print-sysroot");
            probe("-dumpspecs");
        }
        for (const char* tool : {"cc1plus", "as", "ld", "collect2"})
        {
            if (clang && (std::string_view(tool) == "cc1plus" || std::string_view(tool) == "collect2"))
                continue;
            const auto value = probe(std::string("-print-prog-name=") + tool);
            const auto path = resolveTool(value);
            if (path.empty())
                reason(std::string("unresolved-tool: ") + tool);
            else
                addInput(path, "toolchain-tool");
        }

        std::vector<std::string> arguments;
        optionsUnderstood_ = preprocessingArguments(arguments);
        if (!optionsUnderstood_)
            return;
        auto sources = config_.sources;
        sources.insert(sources.begin(), config_.generatedSource);
        for (std::size_t i = 0; i < sources.size(); ++i)
        {
            const auto dependencies = workspace_.file("preprocessor-dependencies", i);
            const auto preprocessed = workspace_.file("preprocessed", i);
            std::vector<std::string> command{common::filesystem::toUtf8(config_.compiler)};
            command.insert(command.end(), arguments.begin(), arguments.end());
            for (const auto& argument : {"-E", "-P", "-MD", "-MF"})
                command.emplace_back(argument);
            command.push_back(common::filesystem::toUtf8(dependencies));
            command.insert(command.end(), {"-MT", "wio_native_inputs", "-o", common::filesystem::toUtf8(preprocessed),
                                           common::filesystem::toUtf8(sources[i])});
            const auto result = runner_(command);
            std::string bytes;
            std::vector<std::string> paths;
            if (result.exitCode != 0 || !readBounded(dependencies, bytes, DependencyLimit) ||
                !parseMakeDependencies(bytes, paths))
            {
                reason("preprocessor-dependency-capture-failed: " + pathText(sources[i]));
                continue;
            }
            for (const auto& path : paths)
                addInput(common::filesystem::fromUtf8(path), "preprocessor-input");
            if (readBounded(preprocessed, bytes, FileLimit))
                preprocessed_.push_back(runtime::std_hash::Sha256(bytes));
            else
                reason("preprocessed-output-unreadable-or-oversized: " + pathText(sources[i]));
        }
    }

    std::vector<std::string> NativeDependencySnapshot::linkerCaptureArguments()
    {
        for (const auto& argument : config_.linkArguments)
        {
            if (argument.starts_with("-fuse-ld") || argument.starts_with("--ld-path") || argument.starts_with("-B") ||
                argument.starts_with("-specs") || argument.starts_with("--specs") || argument.starts_with("@"))
            {
                reason("linker-capture-skipped-for-tool-selection-option: " + argument);
                return {};
            }
        }
        if (!optionsUnderstood_)
        {
            reason("linker-capture-skipped-for-unclassified-options");
            return {};
        }
        const auto tool = resolveTool(probe("-print-prog-name=ld"));
        if (tool.empty())
        {
            reason("linker-dependency-capture-unavailable");
            return {};
        }
#ifdef __APPLE__
        const auto help = runner_({common::filesystem::toUtf8(tool), "-help"});
        // Apple ld does not consistently advertise this established diagnostic
        // option in -help. Recognize its own version identity as well; do not
        // assume that every executable named ld is Apple's linker.
        const auto version = runner_({common::filesystem::toUtf8(tool), "-v"});
        if (help.output.find("-dependency_info") != std::string::npos ||
            version.output.find("PROJECT:ld") != std::string::npos)
        {
            linkerFormat_ = "darwin-dependency-info";
            linkerDependencies_ = workspace_.file("linker-dependencies");
            return {"-Xlinker", "-dependency_info", "-Xlinker", common::filesystem::toUtf8(linkerDependencies_)};
        }
#else
        const auto help = runner_({common::filesystem::toUtf8(tool), "--help"});
        if (help.output.find("--dependency-file") != std::string::npos)
        {
            linkerFormat_ = "gnu-make";
            linkerDependencies_ = workspace_.file("linker-dependencies");
            return {"-Xlinker", "--dependency-file=" + common::filesystem::toUtf8(linkerDependencies_)};
        }
#endif
        reason("linker-dependency-format-unsupported");
        return {};
    }

    void NativeDependencySnapshot::captureAfterBuild(const std::filesystem::path& artifact)
    {
        // Validate files observed before the compiler/linker ran. This detects
        // ordinary concurrent changes, not arbitrary A->B->A filesystem races.
        for (const auto& [key, input] : inputs_)
        {
            std::string bytes;
            const auto path = common::filesystem::fromUtf8(key);
            if (!readBounded(path, bytes, FileLimit) || resolvedText(path) != input.resolvedPath ||
                runtime::std_hash::Sha256(bytes) != input.sha256)
            {
                knownInputsStable_ = false;
                reason("input-changed-during-build: " + key);
            }
        }
        if (!linkerDependencies_.empty())
        {
            std::string bytes;
            std::vector<std::string> paths;
            const bool read = readBounded(linkerDependencies_, bytes, DependencyLimit);
            const bool parsed = read && (linkerFormat_ == "gnu-make" ? parseMakeDependencies(bytes, paths)
                                                                     : parseDarwinDependencies(bytes, paths, missing_));
            if (!parsed)
                reason("linker-dependency-capture-malformed");
            else
                for (const auto& path : paths)
                    addInput(common::filesystem::fromUtf8(path), "linker-input");
        }
        std::string bytes;
        if (readBounded(artifact, bytes, FileLimit))
            artifactSha256_ = runtime::std_hash::Sha256(bytes);
        else
            reason("artifact-unreadable-or-oversized");
        captureFinished_ = true;
    }

    bool NativeDependencySnapshot::containsInput(const std::filesystem::path& path) const
    {
        if (samePath(path, config_.generatedSource) || samePath(path, config_.generator) ||
            samePath(path, config_.compiler))
            return true;
        for (const auto& [key, input] : inputs_)
            if (samePath(path, common::filesystem::fromUtf8(key)))
                return true;
        for (const auto& input : observedPaths_)
            if (samePath(path, input))
                return true;
        for (const auto& source : config_.sources)
            if (samePath(path, source))
                return true;
        for (const auto& input : config_.explicitInputs)
            if (samePath(path, input))
                return true;
        return false;
    }

    std::string NativeDependencySnapshot::toJson() const
    {
        using runtime::std_serialization::JsonQuote;
        std::string out = "{\n  \"schemaVersion\": 1,\n  \"kind\": \"wio-native-dependencies\",\n"
                          "  \"algorithm\": \"sha256\",\n  \"cacheReuseEnabled\": false,\n"
                          "  \"requiresFreshPreprocessing\": true,\n  \"requiresFreshLinkResolution\": true,\n"
                          "  \"hermeticBuild\": false,\n  \"observationComplete\": ";
        out += captureFinished_ && reasons_.empty() ? "true" : "false";
        out += ",\n  \"knownInputsStable\": " + std::string(knownInputsStable_ ? "true" : "false");
        out += ",\n  \"requirementsSha256\": " + JsonQuote(config_.requirementsSha256);
        out += ",\n  \"artifactSha256\": " + JsonQuote(artifactSha256_);
        out += ",\n  \"wioVersion\": " + JsonQuote(config_.version);
        out += ",\n  \"target\": " + JsonQuote(config_.target);
        out += ",\n  \"workingDirectory\": " + JsonQuote(pathText(std::filesystem::current_path()));
        out += ",\n  \"compileArguments\": " + jsonArray(config_.compileArguments);
        out += ",\n  \"linkArguments\": " + jsonArray(config_.linkArguments);
        out += ",\n  \"preprocessedSha256\": " + jsonArray(preprocessed_);
        out += ",\n  \"linkerFormat\": " + JsonQuote(linkerFormat_);
        out += ",\n  \"missingLinkerInputs\": " + jsonArray(missing_);
        out += ",\n  \"incompleteReasons\": " + jsonArray(reasons_);
        const auto object = [&](std::string_view name, const auto& values)
        {
            out += ",\n  " + JsonQuote(name) + ": {";
            bool first = true;
            for (const auto& [key, value] : values)
            {
                if (!first)
                    out += ',';
                first = false;
                out += '\n' + std::string("    ") + JsonQuote(key) + ": " + JsonQuote(value);
            }
            out += "\n  }";
        };
        object("toolchainProbeSha256", probes_);
        object("environmentSha256", environment_);
        out += ",\n  \"inputs\": [";
        bool first = true;
        for (const auto& [key, input] : inputs_)
        {
            if (!first)
                out += ',';
            first = false;
            out += "\n    {\"path\": " + JsonQuote(input.path) +
                   ", \"resolvedPath\": " + JsonQuote(input.resolvedPath) + ", \"role\": " + JsonQuote(input.role) +
                   ", \"bytes\": " + std::to_string(input.bytes) + ", \"sha256\": " + JsonQuote(input.sha256) + '}';
        }
        return out + "\n  ]\n}\n";
    }

    bool NativeDependencySnapshot::parseMakeDependencies(std::string_view text, std::vector<std::string>& paths)
    {
        paths.clear();
        if (text.size() > DependencyLimit || text.find('\0') != std::string_view::npos)
            return false;
        std::string joined;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == '\\' && i + 1 < text.size())
            {
                if (text[i + 1] == '\n')
                {
                    joined += ' ';
                    ++i;
                    continue;
                }
                if (text[i + 1] == '\r' && i + 2 < text.size() && text[i + 2] == '\n')
                {
                    joined += ' ';
                    i += 2;
                    continue;
                }
            }
            joined += text[i];
        }
        std::istringstream lines(joined);
        std::string line;
        bool rule = false;
        while (std::getline(lines, line))
        {
            if (trim(line).empty())
                continue;
            std::size_t colon = std::string::npos;
            for (std::size_t i = 0; i < line.size(); ++i)
            {
                if (line[i] == '\\' && i + 1 < line.size() &&
                    (line[i + 1] == ':' || line[i + 1] == ' ' || line[i + 1] == '\\'))
                {
                    ++i;
                    continue;
                }
                // Drive-letter colons are not Make rule delimiters.
                if (line[i] == ':' &&
                    !(i > 0 && i + 1 < line.size() && std::isalpha(static_cast<unsigned char>(line[i - 1])) &&
                      (i == 1 || std::isspace(static_cast<unsigned char>(line[i - 2]))) &&
                      (line[i + 1] == '/' || line[i + 1] == '\\')))
                {
                    colon = i;
                    break;
                }
            }
            if (colon == std::string::npos || trim(line.substr(0, colon)).empty())
                return false;
            rule = true;
            std::string token;
            const auto flush = [&]()
            {
                if (!token.empty())
                {
                    paths.push_back(token);
                    token.clear();
                }
            };
            for (std::size_t i = colon + 1; i < line.size(); ++i)
            {
                const char c = line[i];
                if (std::isspace(static_cast<unsigned char>(c)))
                    flush();
                else if (c == '#')
                    break;
                else if (c == '$')
                {
                    if (i + 1 == line.size() || line[++i] != '$')
                        return false;
                    token += '$';
                }
                else if (c == '\\')
                {
                    if (i + 1 == line.size())
                        return false;
                    const char next = line[i + 1];
                    if (next == ' ' || next == '\t' || next == '#' || next == ':' || next == '\\')
                    {
                        token += next;
                        ++i;
                    }
                    else
                        token += c;
                }
                else
                    token += c;
            }
            flush();
            if (paths.size() > InputLimit)
                return false;
        }
        std::ranges::sort(paths);
        paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
        return rule && !paths.empty();
    }

    bool NativeDependencySnapshot::parseDarwinDependencies(std::string_view bytes, std::vector<std::string>& paths,
                                                           std::vector<std::string>& missing)
    {
        paths.clear();
        missing.clear();
        if (bytes.size() > DependencyLimit)
            return false;
        bool version = false;
        for (std::size_t i = 0; i < bytes.size();)
        {
            const auto opcode = static_cast<unsigned char>(bytes[i++]);
            const auto end = bytes.find('\0', i);
            if (end == std::string_view::npos)
                return false;
            const auto value = std::string(bytes.substr(i, end - i));
            i = end + 1;
            if (value.empty())
                return false;
            if (opcode == 0x00)
                version = true;
            else if (opcode == 0x10)
                paths.push_back(value);
            else if (opcode == 0x11)
                missing.push_back(value);
            else if (opcode != 0x40)
                return false;
            if (paths.size() + missing.size() > InputLimit)
                return false;
        }
        return version && !paths.empty();
    }
} // namespace wio::codegen
