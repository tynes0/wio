#include "binding_cli.h"
#include "cli_common.h"

#include <argonaut.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace wio::tooling::binding
{
    namespace
    {
        struct JsonValue
        {
            enum class Kind
            {
                Null,
                Bool,
                Number,
                String,
                Array,
                Object
            };

            Kind kind = Kind::Null;
            bool boolValue = false;
            std::string textValue;
            std::vector<JsonValue> arrayValue;
            std::map<std::string, JsonValue> objectValue;
        };

        struct ImportedConstantSpec
        {
            std::string name;
            std::string type;
            std::string value;
        };

        struct ImportedEnumMember
        {
            std::string name;
            std::optional<std::string> value;
        };

        struct ImportedEnumSpec
        {
            std::string name;
            std::string cppName;
            std::string underlyingType;
            std::vector<ImportedEnumMember> members;
            bool isFlagset = false;
        };

        struct ImportedFieldSpec
        {
            std::string name;
            std::string type;
        };

        struct ImportedStructSpec
        {
            std::string name;
            std::string cppName;
            std::vector<ImportedFieldSpec> fields;
        };

        struct ImportedFunctionSpec
        {
            std::string name;
            std::string cppName;
            std::string returnType;
            std::vector<std::string> parameters;
        };

        struct ImportedDeclarations
        {
            std::vector<ImportedConstantSpec> constants;
            std::vector<ImportedEnumSpec> enums;
            std::vector<ImportedStructSpec> structs;
            std::vector<ImportedFunctionSpec> functions;
        };

        class JsonParser
        {
        public:
            explicit JsonParser(std::string_view text) :
                m_text(text)
            {
            }

            JsonValue parse()
            {
                skipWhitespace();
                JsonValue value = parseValue();
                skipWhitespace();
                if (!isAtEnd())
                    throw std::runtime_error("Unexpected trailing characters in JSON manifest.");
                return value;
            }

        private:
            std::string_view m_text;
            size_t m_index = 0;

            bool isAtEnd() const
            {
                return m_index >= m_text.size();
            }

            char peek() const
            {
                return isAtEnd() ? '\0' : m_text[m_index];
            }

            char consume()
            {
                if (isAtEnd())
                    throw std::runtime_error("Unexpected end of JSON input.");
                return m_text[m_index++];
            }

            void expect(char expected)
            {
                const char actual = consume();
                if (actual != expected)
                {
                    std::ostringstream stream;
                    stream << "Expected '" << expected << "' in JSON input, but got '" << actual << "'.";
                    throw std::runtime_error(stream.str());
                }
            }

            void skipWhitespace()
            {
                while (!isAtEnd() && std::isspace(static_cast<unsigned char>(m_text[m_index])) != 0)
                    ++m_index;
            }

            JsonValue parseValue()
            {
                skipWhitespace();
                switch (peek())
                {
                case '{':
                    return parseObject();
                case '[':
                    return parseArray();
                case '"':
                    return parseString();
                case 't':
                    return parseLiteral("true", JsonValue::Kind::Bool, true);
                case 'f':
                    return parseLiteral("false", JsonValue::Kind::Bool, false);
                case 'n':
                    return parseLiteral("null", JsonValue::Kind::Null, false);
                default:
                    if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek())) != 0)
                        return parseNumber();
                    throw std::runtime_error("Unsupported JSON token while parsing binding manifest.");
                }
            }

            JsonValue parseLiteral(const std::string& literal, JsonValue::Kind kind, bool boolValue)
            {
                if (m_text.substr(m_index, literal.size()) != literal)
                    throw std::runtime_error("Invalid JSON literal encountered.");

                m_index += literal.size();

                JsonValue value;
                value.kind = kind;
                value.boolValue = boolValue;
                return value;
            }

            JsonValue parseNumber()
            {
                const size_t start = m_index;
                if (peek() == '-')
                    ++m_index;

                while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0)
                    ++m_index;

                if (!isAtEnd() && peek() == '.')
                {
                    ++m_index;
                    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0)
                        ++m_index;
                }

                if (!isAtEnd() && (peek() == 'e' || peek() == 'E'))
                {
                    ++m_index;
                    if (!isAtEnd() && (peek() == '+' || peek() == '-'))
                        ++m_index;
                    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0)
                        ++m_index;
                }

                JsonValue value;
                value.kind = JsonValue::Kind::Number;
                value.textValue = std::string(m_text.substr(start, m_index - start));
                return value;
            }

            JsonValue parseString()
            {
                expect('"');

                JsonValue value;
                value.kind = JsonValue::Kind::String;

                while (!isAtEnd())
                {
                    const char current = consume();
                    if (current == '"')
                        return value;

                    if (current != '\\')
                    {
                        value.textValue.push_back(current);
                        continue;
                    }

                    const char escaped = consume();
                    switch (escaped)
                    {
                    case '"':
                    case '\\':
                    case '/':
                        value.textValue.push_back(escaped);
                        break;
                    case 'b':
                        value.textValue.push_back('\b');
                        break;
                    case 'f':
                        value.textValue.push_back('\f');
                        break;
                    case 'n':
                        value.textValue.push_back('\n');
                        break;
                    case 'r':
                        value.textValue.push_back('\r');
                        break;
                    case 't':
                        value.textValue.push_back('\t');
                        break;
                    default:
                        throw std::runtime_error("Unsupported JSON string escape sequence in binding manifest.");
                    }
                }

                throw std::runtime_error("Unterminated JSON string in binding manifest.");
            }

            JsonValue parseArray()
            {
                expect('[');

                JsonValue value;
                value.kind = JsonValue::Kind::Array;

                skipWhitespace();
                if (peek() == ']')
                {
                    consume();
                    return value;
                }

                while (true)
                {
                    value.arrayValue.push_back(parseValue());
                    skipWhitespace();

                    if (peek() == ']')
                    {
                        consume();
                        return value;
                    }

                    expect(',');
                    skipWhitespace();
                }
            }

            JsonValue parseObject()
            {
                expect('{');

                JsonValue value;
                value.kind = JsonValue::Kind::Object;

                skipWhitespace();
                if (peek() == '}')
                {
                    consume();
                    return value;
                }

                while (true)
                {
                    JsonValue key = parseString();
                    skipWhitespace();
                    expect(':');
                    skipWhitespace();
                    value.objectValue.emplace(key.textValue, parseValue());
                    skipWhitespace();

                    if (peek() == '}')
                    {
                        consume();
                        return value;
                    }

                    expect(',');
                    skipWhitespace();
                }
            }
        };

        std::string readUtf8File(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream.is_open())
                throw std::runtime_error("Could not open file: " + path.string());

            std::ostringstream buffer;
            buffer << stream.rdbuf();
            return buffer.str();
        }

        void writeUtf8File(const std::filesystem::path& path, const std::string& content)
        {
            std::error_code ec;
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path(), ec);

            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
                throw std::runtime_error("Could not open file for writing: " + path.string());

            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!stream.good())
                throw std::runtime_error("Could not write file: " + path.string());
        }

        void appendLine(std::vector<std::string>& lines, const std::string& text = {})
        {
            lines.push_back(text);
        }

        std::string quoteWioStringLiteral(const std::string& value)
        {
            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for (const char ch : value)
            {
                switch (ch)
                {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped.push_back(ch);
                    break;
                }
            }
            escaped.push_back('"');
            return escaped;
        }

        std::string joinLines(const std::vector<std::string>& lines)
        {
            std::ostringstream stream;
            for (size_t i = 0; i < lines.size(); ++i)
            {
                stream << lines[i];
                if (i + 1 < lines.size())
                    stream << '\n';
            }
            stream << '\n';
            return stream.str();
        }

        std::string jsonKindName(const JsonValue& value)
        {
            switch (value.kind)
            {
            case JsonValue::Kind::Null:
                return "null";
            case JsonValue::Kind::Bool:
                return "bool";
            case JsonValue::Kind::Number:
                return "number";
            case JsonValue::Kind::String:
                return "string";
            case JsonValue::Kind::Array:
                return "array";
            case JsonValue::Kind::Object:
                return "object";
            }

            return "unknown";
        }

        const JsonValue& requireJsonObject(const JsonValue& value, const std::string& context)
        {
            if (value.kind != JsonValue::Kind::Object)
                throw std::runtime_error(context + " must be a JSON object, but got " + jsonKindName(value) + ".");
            return value;
        }

        const JsonValue& requireJsonArray(const JsonValue& value, const std::string& context)
        {
            if (value.kind != JsonValue::Kind::Array)
                throw std::runtime_error(context + " must be a JSON array, but got " + jsonKindName(value) + ".");
            return value;
        }

        const JsonValue* tryGetObjectField(const JsonValue& objectValue, const std::string& name)
        {
            if (objectValue.kind != JsonValue::Kind::Object)
                return nullptr;

            const auto it = objectValue.objectValue.find(name);
            if (it == objectValue.objectValue.end())
                return nullptr;

            return &it->second;
        }

        std::string getRequiredStringField(const JsonValue& objectValue,
                                           const std::string& name,
                                           const std::string& context)
        {
            const JsonValue* field = tryGetObjectField(objectValue, name);
            if (field == nullptr)
                throw std::runtime_error(context + " is missing required string field '" + name + "'.");
            if (field->kind != JsonValue::Kind::String)
                throw std::runtime_error(context + " field '" + name + "' must be a string.");
            return field->textValue;
        }

        std::optional<std::string> getOptionalStringField(const JsonValue& objectValue, const std::string& name)
        {
            const JsonValue* field = tryGetObjectField(objectValue, name);
            if (field == nullptr || field->kind == JsonValue::Kind::Null)
                return std::nullopt;
            if (field->kind != JsonValue::Kind::String)
                throw std::runtime_error("JSON field '" + name + "' must be a string when present.");
            return field->textValue;
        }

        std::string renderJsonScalar(const JsonValue& value)
        {
            switch (value.kind)
            {
            case JsonValue::Kind::String:
            case JsonValue::Kind::Number:
                return value.textValue;
            case JsonValue::Kind::Bool:
                return value.boolValue ? "true" : "false";
            case JsonValue::Kind::Null:
                return "null";
            default:
                throw std::runtime_error("Expected a JSON scalar value, but got " + jsonKindName(value) + ".");
            }
        }

        std::vector<const JsonValue*> getOptionalArrayField(const JsonValue& objectValue, const std::string& name)
        {
            const JsonValue* field = tryGetObjectField(objectValue, name);
            if (field == nullptr || field->kind == JsonValue::Kind::Null)
                return {};

            const JsonValue& arrayValue = requireJsonArray(*field, "JSON field '" + name + "'");
            std::vector<const JsonValue*> values;
            values.reserve(arrayValue.arrayValue.size());
            for (const JsonValue& item : arrayValue.arrayValue)
                values.push_back(&item);
            return values;
        }

        std::string trim(const std::string& value)
        {
            size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
                ++start;

            size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
                --end;

            return value.substr(start, end - start);
        }

        std::string collapseWhitespace(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());

            bool previousWasWhitespace = false;
            for (const unsigned char ch : value)
            {
                if (std::isspace(ch) != 0)
                {
                    if (!previousWasWhitespace)
                        result.push_back(' ');
                    previousWasWhitespace = true;
                }
                else
                {
                    result.push_back(static_cast<char>(ch));
                    previousWasWhitespace = false;
                }
            }

            return trim(result);
        }

        std::string getBindingCppName(const JsonValue& realmSpec, const JsonValue& itemSpec)
        {
            if (const auto itemCppName = getOptionalStringField(itemSpec, "cppName"); itemCppName.has_value() && !itemCppName->empty())
                return *itemCppName;

            const std::string name = getRequiredStringField(itemSpec, "name", "Binding item");
            const auto realmCppNamespace = getOptionalStringField(realmSpec, "cppNamespace");
            if (!realmCppNamespace.has_value() || realmCppNamespace->empty())
                return name;

            return *realmCppNamespace + "::" + name;
        }

        void renderBindingFieldList(std::vector<std::string>& lines,
                                    const std::vector<const JsonValue*>& fields,
                                    const std::string& indent)
        {
            for (const JsonValue* fieldValue : fields)
            {
                const JsonValue& field = requireJsonObject(*fieldValue, "Field definition");
                appendLine(lines, indent + getRequiredStringField(field, "name", "Field definition") +
                                     ": " +
                                     getRequiredStringField(field, "type", "Field definition") + ";");
            }
        }

        std::string renderBindingParameterList(const JsonValue* parametersValue)
        {
            if (parametersValue == nullptr || parametersValue->kind == JsonValue::Kind::Null)
                return {};

            const JsonValue& arrayValue = requireJsonArray(*parametersValue, "Function parameter list");
            std::ostringstream stream;
            for (size_t i = 0; i < arrayValue.arrayValue.size(); ++i)
            {
                const JsonValue& parameter = requireJsonObject(arrayValue.arrayValue[i], "Function parameter");
                if (i > 0)
                    stream << ", ";
                stream << getRequiredStringField(parameter, "name", "Function parameter")
                       << ": "
                       << getRequiredStringField(parameter, "type", "Function parameter");
            }
            return stream.str();
        }

        Argonaut::Parser makeBindNewParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("MANIFEST")
                        .AddAlias("--manifest")
                        .Required()
                        .SetDescription("Path to the JSON binding manifest.")
                )
                .Add(
                    Argonaut::Argument("OUTPUT")
                        .AddAlias("--output")
                        .SetDefaultValue("")
                        .SetDescription("Optional output .wio file override.")
                )
                .AutoHelp()
                .AutoVersion()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        Argonaut::Parser makeBindImportParser()
        {
            Argonaut::Parser parser;
            parser
                .Add(
                    Argonaut::Argument("HEADER")
                        .AddAlias("--header")
                        .Required()
                        .SetDescription("Path to the C/C++ header to import.")
                )
                .Add(
                    Argonaut::Argument("REALM")
                        .AddAlias("--realm")
                        .Required()
                        .SetDescription("Target Wio realm name for the generated bindings.")
                )
                .Add(
                    Argonaut::Argument("OUTPUT")
                        .AddAlias("--output")
                        .SetDefaultValue("")
                        .SetDescription("Optional output .wio file override.")
                )
                .Add(
                    Argonaut::Argument("HEADER-INCLUDE")
                        .AddAlias("--header-include")
                        .SetDefaultValue("")
                        .SetDescription("Header path string to place inside @CppHeader(...). Defaults to the header file name.")
                )
                .Add(
                    Argonaut::Argument("PREFER-FLAGSET")
                        .AddAlias("--prefer-flagset")
                        .Flag()
                        .SetDescription("Treat imported enums as flagsets when possible.")
                )
                .AutoHelp()
                .AutoVersion()
                .SetVersion(WIO_VERSION);

            return parser;
        }

        std::vector<char*> buildArgvView(std::vector<std::string>& args)
        {
            std::vector<char*> argvView;
            argvView.reserve(args.size());
            for (std::string& arg : args)
                argvView.push_back(arg.data());
            return argvView;
        }

        std::optional<int> parseWithHandling(Argonaut::Parser& parser, std::vector<std::string>& args)
        {
            std::vector<char*> argvView = buildArgvView(args);

            try
            {
                parser.Parse(static_cast<int>(argvView.size()), argvView.data());
                return std::nullopt;
            }
            catch (const Argonaut::HelpRequestedException& e)
            {
                std::cout << e.what();
                return EXIT_SUCCESS;
            }
            catch (const Argonaut::VersionRequestedException& e)
            {
                std::cout << e.what();
                return EXIT_SUCCESS;
            }
            catch (const Argonaut::ParsePrepException& e)
            {
                std::cerr << "Binding CLI setup failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const Argonaut::ParseException& e)
            {
                std::cerr << "Binding argument parsing failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Unhandled binding CLI error: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        bool getFlagValue(Argonaut::Parser& parser, const std::string& id)
        {
            auto values = parser.GetValuesOf<bool>(id);
            return !values.empty() && values.front();
        }

        std::vector<std::string> collectCommandArgs(const std::string& programName, int argc, char* argv[], int firstArgumentIndex)
        {
            std::vector<std::string> args;
            args.reserve(static_cast<size_t>(argc - firstArgumentIndex + 1));
            args.push_back(programName);

            for (int i = firstArgumentIndex; i < argc; ++i)
            {
                if (argv[i] != nullptr)
                    args.emplace_back(argv[i]);
            }

            return args;
        }

        int handleBindNewCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeBindNewParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::filesystem::path manifestPath = std::filesystem::absolute(parser.GetValuesOf<std::string>("MANIFEST").front()).make_preferred();
                const std::filesystem::path manifestDir = manifestPath.parent_path();
                const JsonValue manifest = JsonParser(readUtf8File(manifestPath)).parse();
                const JsonValue& root = requireJsonObject(manifest, "Binding manifest");

                std::string outputArgument = parser.GetValuesOf<std::string>("OUTPUT").front();
                if (outputArgument.empty())
                {
                    if (const auto manifestOutput = getOptionalStringField(root, "outputPath"); manifestOutput.has_value() && !manifestOutput->empty())
                        outputArgument = *manifestOutput;
                    else
                        throw std::runtime_error("Binding manifest must define 'outputPath' or --output must be provided.");
                }

                std::filesystem::path outputPath = std::filesystem::path(outputArgument);
                if (!outputPath.is_absolute())
                    outputPath = std::filesystem::absolute(manifestDir / outputPath).make_preferred();

                std::vector<std::string> lines;
                appendLine(lines, "// Generated by wio bind new");
                appendLine(lines, "// Manifest: " + manifestPath.string());
                appendLine(lines);

                const std::vector<const JsonValue*> realms = getOptionalArrayField(root, "realms");
                if (realms.empty())
                    throw std::runtime_error("Binding manifest must contain at least one realm in 'realms'.");

                for (const JsonValue* realmValue : realms)
                {
                    const JsonValue& realm = requireJsonObject(*realmValue, "Realm specification");
                    appendLine(lines, "realm " + getRequiredStringField(realm, "name", "Realm specification") + " {");

                    std::vector<std::string> headers;
                    if (const JsonValue* realmHeaders = tryGetObjectField(realm, "headers"))
                    {
                        for (const JsonValue& header : requireJsonArray(*realmHeaders, "Realm headers").arrayValue)
                            headers.push_back(header.textValue);
                    }
                    else if (const JsonValue* specHeaders = tryGetObjectField(root, "headers"))
                    {
                        for (const JsonValue& header : requireJsonArray(*specHeaders, "Manifest headers").arrayValue)
                            headers.push_back(header.textValue);
                    }
                    else if (const auto specHeader = getOptionalStringField(root, "header"); specHeader.has_value())
                    {
                        headers.push_back(*specHeader);
                    }

                    for (const std::string& header : headers)
                        appendLine(lines, "    use @CppHeader(" + quoteWioStringLiteral(header) + ");");

                    if (!headers.empty())
                        appendLine(lines);

                    const std::vector<const JsonValue*> consts = getOptionalArrayField(realm, "consts");
                    for (const JsonValue* constValue : consts)
                    {
                        const JsonValue& constant = requireJsonObject(*constValue, "Constant specification");
                        appendLine(lines,
                                   "    const " + getRequiredStringField(constant, "name", "Constant specification") +
                                       ": " + getRequiredStringField(constant, "type", "Constant specification") +
                                       " = " + renderJsonScalar(*tryGetObjectField(constant, "value")) + ";");
                    }

                    if (!consts.empty())
                        appendLine(lines);

                    const std::vector<const JsonValue*> enums = getOptionalArrayField(realm, "enums");
                    for (const JsonValue* enumValue : enums)
                    {
                        const JsonValue& enumSpec = requireJsonObject(*enumValue, "Enum specification");
                        if (const auto backingType = getOptionalStringField(enumSpec, "backingType"); backingType.has_value() && !backingType->empty())
                            appendLine(lines, "    @Type(" + *backingType + ")");

                        appendLine(lines, "    enum " + getRequiredStringField(enumSpec, "name", "Enum specification") + " {");
                        for (const JsonValue* memberValue : getOptionalArrayField(enumSpec, "members"))
                        {
                            const JsonValue& member = requireJsonObject(*memberValue, "Enum member");
                            if (const JsonValue* value = tryGetObjectField(member, "value"))
                            {
                                appendLine(lines, "        " + getRequiredStringField(member, "name", "Enum member") +
                                                     " = " + renderJsonScalar(*value) + ",");
                            }
                            else
                            {
                                appendLine(lines, "        " + getRequiredStringField(member, "name", "Enum member") + ",");
                            }
                        }
                        appendLine(lines, "    };");
                        appendLine(lines);
                    }

                    const std::vector<const JsonValue*> flagsets = getOptionalArrayField(realm, "flagsets");
                    for (const JsonValue* flagsetValue : flagsets)
                    {
                        const JsonValue& flagsetSpec = requireJsonObject(*flagsetValue, "Flagset specification");
                        if (const auto backingType = getOptionalStringField(flagsetSpec, "backingType"); backingType.has_value() && !backingType->empty())
                            appendLine(lines, "    @Type(" + *backingType + ")");

                        appendLine(lines, "    flagset " + getRequiredStringField(flagsetSpec, "name", "Flagset specification") + " {");
                        for (const JsonValue* memberValue : getOptionalArrayField(flagsetSpec, "members"))
                        {
                            const JsonValue& member = requireJsonObject(*memberValue, "Flagset member");
                            if (const JsonValue* value = tryGetObjectField(member, "value"))
                            {
                                appendLine(lines, "        " + getRequiredStringField(member, "name", "Flagset member") +
                                                     " = " + renderJsonScalar(*value) + ",");
                            }
                            else
                            {
                                appendLine(lines, "        " + getRequiredStringField(member, "name", "Flagset member") + ",");
                            }
                        }
                        appendLine(lines, "    };");
                        appendLine(lines);
                    }

                    const std::vector<const JsonValue*> components = getOptionalArrayField(realm, "components");
                    for (const JsonValue* componentValue : components)
                    {
                        const JsonValue& component = requireJsonObject(*componentValue, "Component specification");
                        appendLine(lines, "    @Native");
                        appendLine(lines, "    @CppName(" + getBindingCppName(realm, component) + ")");
                        appendLine(lines, "    component " + getRequiredStringField(component, "name", "Component specification") + " {");
                        renderBindingFieldList(lines, getOptionalArrayField(component, "fields"), "        ");
                        appendLine(lines, "    }");
                        appendLine(lines);
                    }

                    const std::vector<const JsonValue*> functions = getOptionalArrayField(realm, "functions");
                    for (const JsonValue* functionValue : functions)
                    {
                        const JsonValue& function = requireJsonObject(*functionValue, "Function specification");
                        appendLine(lines, "    @Native");
                        appendLine(lines, "    @CppName(" + getBindingCppName(realm, function) + ")");

                        for (const JsonValue* instantiateArg : getOptionalArrayField(function, "instantiate"))
                            appendLine(lines, "    @Instantiate(" + renderJsonScalar(*instantiateArg) + ")");

                        for (const JsonValue* applyArg : getOptionalArrayField(function, "apply"))
                            appendLine(lines, "    @Apply(" + renderJsonScalar(*applyArg) + ")");

                        const JsonValue* parameters = tryGetObjectField(function, "parameters");
                        const std::string parameterList = renderBindingParameterList(parameters);
                        if (const auto returnType = getOptionalStringField(function, "returnType"); returnType.has_value() && !returnType->empty())
                            appendLine(lines, "    fn " + getRequiredStringField(function, "name", "Function specification") + "(" + parameterList + ") -> " + *returnType + ";");
                        else
                            appendLine(lines, "    fn " + getRequiredStringField(function, "name", "Function specification") + "(" + parameterList + ");");

                        appendLine(lines);
                    }

                    appendLine(lines, "}");
                    appendLine(lines);
                }

                writeUtf8File(outputPath, joinLines(lines));
                std::cout << "Generated Wio binding module: " << outputPath.string() << '\n';
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Binding generation failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }

        std::string removeCComments(const std::string& text)
        {
            std::string result;
            result.reserve(text.size());

            bool inLineComment = false;
            bool inBlockComment = false;

            for (size_t i = 0; i < text.size(); ++i)
            {
                const char current = text[i];
                const char next = (i + 1 < text.size()) ? text[i + 1] : '\0';

                if (inLineComment)
                {
                    if (current == '\n')
                    {
                        inLineComment = false;
                        result.push_back(current);
                    }
                    continue;
                }

                if (inBlockComment)
                {
                    if (current == '*' && next == '/')
                    {
                        inBlockComment = false;
                        ++i;
                    }
                    continue;
                }

                if (current == '/' && next == '/')
                {
                    inLineComment = true;
                    ++i;
                    continue;
                }

                if (current == '/' && next == '*')
                {
                    inBlockComment = true;
                    ++i;
                    continue;
                }

                result.push_back(current);
            }

            return result;
        }

        size_t getMatchingBraceIndex(const std::string& text, size_t openBraceIndex)
        {
            int depth = 0;
            for (size_t index = openBraceIndex; index < text.size(); ++index)
            {
                if (text[index] == '{')
                    ++depth;
                else if (text[index] == '}')
                {
                    --depth;
                    if (depth == 0)
                        return index;
                }
            }

            throw std::runtime_error("Unbalanced braces while parsing header.");
        }

        std::vector<std::string> splitTopLevelCommaList(const std::string& text)
        {
            std::vector<std::string> items;
            int angleDepth = 0;
            int parenDepth = 0;
            size_t start = 0;

            for (size_t index = 0; index < text.size(); ++index)
            {
                switch (text[index])
                {
                case '<':
                    ++angleDepth;
                    break;
                case '>':
                    if (angleDepth > 0)
                        --angleDepth;
                    break;
                case '(':
                    ++parenDepth;
                    break;
                case ')':
                    if (parenDepth > 0)
                        --parenDepth;
                    break;
                case ',':
                    if (angleDepth == 0 && parenDepth == 0)
                    {
                        items.push_back(collapseWhitespace(text.substr(start, index - start)));
                        start = index + 1;
                    }
                    break;
                default:
                    break;
                }
            }

            if (start < text.size())
                items.push_back(collapseWhitespace(text.substr(start)));

            std::vector<std::string> filtered;
            for (const std::string& item : items)
            {
                if (!item.empty())
                    filtered.push_back(item);
            }
            return filtered;
        }

        std::string convertCppTypeToWioType(std::string typeText)
        {
            typeText = collapseWhitespace(typeText);

            auto eraseKeyword = [&typeText](const std::string& keyword)
            {
                const std::regex pattern("\\b" + keyword + "\\s+");
                typeText = std::regex_replace(typeText, pattern, "");
            };

            eraseKeyword("struct");
            eraseKeyword("class");
            typeText = std::regex_replace(typeText, std::regex("\\benum\\s+class\\s+"), "");
            eraseKeyword("enum");

            if (std::regex_match(typeText, std::regex(R"(^const\s+char\s*\*$)")))
                return "string";
            if (std::regex_match(typeText, std::regex(R"(^char\s*\*$)")))
                return "string";
            if (std::regex_match(typeText, std::regex(R"(^const\s+void\s*\*$)")))
                return "opaque";
            if (std::regex_match(typeText, std::regex(R"(^void\s*\*$)")))
                return "opaque";

            static const std::map<std::string, std::string> primitiveMap{
                { "void", "void" },
                { "bool", "bool" },
                { "char", "char" },
                { "signed char", "i8" },
                { "unsigned char", "u8" },
                { "short", "i16" },
                { "unsigned short", "u16" },
                { "int", "i32" },
                { "unsigned int", "u32" },
                { "long long", "i64" },
                { "unsigned long long", "u64" },
                { "float", "f32" },
                { "double", "f64" },
                { "std::int8_t", "i8" },
                { "int8_t", "i8" },
                { "std::int16_t", "i16" },
                { "int16_t", "i16" },
                { "std::int32_t", "i32" },
                { "int32_t", "i32" },
                { "std::int64_t", "i64" },
                { "int64_t", "i64" },
                { "std::uint8_t", "u8" },
                { "uint8_t", "u8" },
                { "std::uint16_t", "u16" },
                { "uint16_t", "u16" },
                { "std::uint32_t", "u32" },
                { "uint32_t", "u32" },
                { "std::uint64_t", "u64" },
                { "uint64_t", "u64" },
                { "std::size_t", "usize" },
                { "size_t", "usize" }
            };

            const auto it = primitiveMap.find(typeText);
            if (it != primitiveMap.end())
                return it->second;

            return typeText;
        }

        std::optional<ImportedFieldSpec> convertCppFieldDeclarationToWioType(const std::string& fieldText)
        {
            const std::string collapsed = collapseWhitespace(fieldText);
            std::smatch match;

            if (std::regex_match(collapsed, match, std::regex(R"(^(.*?)\s+([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]$)")))
            {
                return ImportedFieldSpec{
                    match[2].str(),
                    "[" + convertCppTypeToWioType(match[1].str()) + "; " + match[3].str() + "]"
                };
            }

            if (std::regex_match(collapsed, match, std::regex(R"(^(.*?)\s+([A-Za-z_]\w*)$)")))
            {
                return ImportedFieldSpec{
                    match[2].str(),
                    convertCppTypeToWioType(match[1].str())
                };
            }

            return std::nullopt;
        }

        std::optional<std::string> convertCppDeclarationParameter(const std::string& parameterText)
        {
            const std::string collapsed = collapseWhitespace(parameterText);
            if (collapsed.empty() || collapsed == "void")
                return std::nullopt;

            std::smatch match;
            if (!std::regex_match(collapsed, match, std::regex(R"(^(.*?)\s+([A-Za-z_]\w*)$)")))
                throw std::runtime_error("Unsupported parameter declaration: '" + collapsed + "'.");

            std::string typePart = collapseWhitespace(match[1].str());
            const std::string namePart = match[2].str();

            const bool isReference = !typePart.empty() && typePart.back() == '&';
            const bool isPointer = !typePart.empty() && typePart.back() == '*';
            const bool isConst = typePart.rfind("const ", 0) == 0;

            if (isReference || isPointer)
            {
                std::string baseType = typePart.substr(0, typePart.size() - 1);
                baseType = collapseWhitespace(std::regex_replace(baseType, std::regex(R"(^const\s+)"), ""));
                const std::string wioBaseType = convertCppTypeToWioType(baseType);

                if (wioBaseType == "string" || wioBaseType == "opaque")
                    return namePart + ": " + wioBaseType;

                if (isConst)
                    return namePart + ": view " + wioBaseType;

                return namePart + ": ref " + wioBaseType;
            }

            return namePart + ": " + convertCppTypeToWioType(typePart);
        }

        std::string convertCppReturnTypeToWioType(std::string returnTypeText)
        {
            returnTypeText = collapseWhitespace(returnTypeText);
            if (!returnTypeText.empty() && (returnTypeText.back() == '&' || returnTypeText.back() == '*'))
            {
                std::string baseType = returnTypeText.substr(0, returnTypeText.size() - 1);
                baseType = collapseWhitespace(std::regex_replace(baseType, std::regex(R"(^const\s+)"), ""));
                return convertCppTypeToWioType(baseType);
            }

            return convertCppTypeToWioType(returnTypeText);
        }

        std::optional<std::string> convertUnderlyingTypeToAttribute(const std::string& underlyingType)
        {
            if (underlyingType.empty())
                return std::nullopt;

            const std::string mapped = convertCppTypeToWioType(underlyingType);
            if (mapped == "void")
                return std::nullopt;

            return "@Type(" + mapped + ")";
        }

        std::vector<ImportedEnumMember> parseEnumMembers(const std::string& body)
        {
            std::vector<ImportedEnumMember> members;
            std::stringstream stream(body);
            std::string rawEntry;
            while (std::getline(stream, rawEntry, ','))
            {
                const std::string entry = collapseWhitespace(rawEntry);
                if (entry.empty())
                    continue;

                std::smatch match;
                if (std::regex_match(entry, match, std::regex(R"(^([A-Za-z_]\w*)\s*=\s*(.+)$)")))
                    members.push_back({ match[1].str(), collapseWhitespace(match[2].str()) });
                else if (std::regex_match(entry, match, std::regex(R"(^([A-Za-z_]\w*)$)")))
                    members.push_back({ match[1].str(), std::nullopt });
            }

            return members;
        }

        bool isLikelyFlagset(const std::string& name,
                             const std::vector<ImportedEnumMember>& members,
                             bool preferFlagset)
        {
            if (preferFlagset)
                return true;

            if (std::regex_search(name, std::regex(R"((Flags|Bits|Mask|Modifiers|Options)$)")))
                return true;

            for (const ImportedEnumMember& member : members)
            {
                if (!member.value.has_value())
                    continue;

                if (std::regex_search(*member.value, std::regex(R"((<<|\||&|~))")))
                    return true;
            }

            return false;
        }

        void mergeDeclarations(ImportedDeclarations& target, ImportedDeclarations source)
        {
            target.constants.insert(target.constants.end(), source.constants.begin(), source.constants.end());
            target.enums.insert(target.enums.end(), source.enums.begin(), source.enums.end());
            target.structs.insert(target.structs.end(), source.structs.begin(), source.structs.end());
            target.functions.insert(target.functions.end(), source.functions.begin(), source.functions.end());
        }

        ImportedDeclarations parseFlatDeclarations(const std::string& text,
                                                   const std::vector<std::string>& namespaceStack,
                                                   bool preferFlagset)
        {
            ImportedDeclarations results;
            std::string working = text;

            const std::regex enumPattern(R"(enum(?:\s+class)?\s+([A-Za-z_]\w*)\s*(?::\s*([^{};]+))?\s*\{([\s\S]*?)\}\s*;)");
            for (std::sregex_iterator it(working.begin(), working.end(), enumPattern), end; it != end; ++it)
            {
                const std::smatch& match = *it;
                std::vector<ImportedEnumMember> members = parseEnumMembers(match[3].str());
                ImportedEnumSpec spec;
                spec.name = match[1].str();
                spec.cppName = spec.name;
                if (!namespaceStack.empty())
                {
                    spec.cppName.clear();
                    for (size_t i = 0; i < namespaceStack.size(); ++i)
                    {
                        if (i > 0)
                            spec.cppName += "::";
                        spec.cppName += namespaceStack[i];
                    }
                    spec.cppName += "::" + spec.name;
                }
                spec.underlyingType = collapseWhitespace(match[2].str());
                spec.members = std::move(members);
                spec.isFlagset = isLikelyFlagset(spec.name, spec.members, preferFlagset);
                results.enums.push_back(std::move(spec));
            }
            working = std::regex_replace(working, enumPattern, " ");

            const std::regex constexprPattern(R"((?:(?:inline|static)\s+)*constexpr\s+([A-Za-z_:\s\*&<>\d,]+?)\s+([A-Za-z_]\w*)\s*=\s*([^;{}]+)\s*;)");
            for (std::sregex_iterator it(working.begin(), working.end(), constexprPattern), end; it != end; ++it)
            {
                const std::smatch& match = *it;
                results.constants.push_back({
                    match[2].str(),
                    convertCppTypeToWioType(match[1].str()),
                    collapseWhitespace(match[3].str())
                });
            }
            working = std::regex_replace(working, constexprPattern, " ");

            const std::regex structPattern(R"(struct\s+([A-Za-z_]\w*)\s*\{([\s\S]*?)\}\s*;)");
            for (std::sregex_iterator it(working.begin(), working.end(), structPattern), end; it != end; ++it)
            {
                const std::smatch& match = *it;
                ImportedStructSpec spec;
                spec.name = match[1].str();
                spec.cppName = spec.name;
                if (!namespaceStack.empty())
                {
                    spec.cppName.clear();
                    for (size_t i = 0; i < namespaceStack.size(); ++i)
                    {
                        if (i > 0)
                            spec.cppName += "::";
                        spec.cppName += namespaceStack[i];
                    }
                    spec.cppName += "::" + spec.name;
                }

                std::stringstream bodyStream(match[2].str());
                std::string fieldText;
                while (std::getline(bodyStream, fieldText, ';'))
                {
                    if (const auto field = convertCppFieldDeclarationToWioType(fieldText); field.has_value())
                        spec.fields.push_back(*field);
                }

                results.structs.push_back(std::move(spec));
            }
            working = std::regex_replace(working, structPattern, " ");

            const std::regex functionPattern(R"(([A-Za-z_:\s\*&<>\d,]+?)\s+([A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*;)");
            for (std::sregex_iterator it(working.begin(), working.end(), functionPattern), end; it != end; ++it)
            {
                const std::smatch& match = *it;
                const std::string name = match[2].str();
                if (name == "if" || name == "for" || name == "while" || name == "switch" || name == "return")
                    continue;

                ImportedFunctionSpec spec;
                spec.name = name;
                spec.cppName = name;
                if (!namespaceStack.empty())
                {
                    spec.cppName.clear();
                    for (size_t i = 0; i < namespaceStack.size(); ++i)
                    {
                        if (i > 0)
                            spec.cppName += "::";
                        spec.cppName += namespaceStack[i];
                    }
                    spec.cppName += "::" + spec.name;
                }
                spec.returnType = convertCppReturnTypeToWioType(match[1].str());

                for (const std::string& parameter : splitTopLevelCommaList(match[3].str()))
                {
                    if (const auto rendered = convertCppDeclarationParameter(parameter); rendered.has_value())
                        spec.parameters.push_back(*rendered);
                }

                results.functions.push_back(std::move(spec));
            }

            return results;
        }

        ImportedDeclarations parseNamespaceAwareDeclarations(const std::string& text,
                                                             bool preferFlagset,
                                                             const std::vector<std::string>& namespaceStack = {})
        {
            ImportedDeclarations aggregate;
            const std::regex namespacePattern(R"(namespace\s+([A-Za-z_]\w*)\s*\{)");

            size_t cursor = 0;
            while (cursor < text.size())
            {
                std::smatch match;
                const std::string remaining = text.substr(cursor);
                if (!std::regex_search(remaining, match, namespacePattern))
                    break;

                const size_t matchIndex = cursor + static_cast<size_t>(match.position());
                mergeDeclarations(
                    aggregate,
                    parseFlatDeclarations(text.substr(cursor, matchIndex - cursor), namespaceStack, preferFlagset));

                const size_t openBraceIndex = matchIndex + static_cast<size_t>(match.length()) - 1;
                const size_t closeBraceIndex = getMatchingBraceIndex(text, openBraceIndex);
                std::vector<std::string> childNamespaceStack = namespaceStack;
                childNamespaceStack.push_back(match[1].str());
                mergeDeclarations(
                    aggregate,
                    parseNamespaceAwareDeclarations(text.substr(openBraceIndex + 1, closeBraceIndex - openBraceIndex - 1),
                                                    preferFlagset,
                                                    childNamespaceStack));

                cursor = closeBraceIndex + 1;
            }

            if (cursor < text.size())
                mergeDeclarations(aggregate, parseFlatDeclarations(text.substr(cursor), namespaceStack, preferFlagset));

            return aggregate;
        }

        int handleBindImportCommand(std::vector<std::string> args)
        {
            Argonaut::Parser parser = makeBindImportParser();
            if (const auto parseResult = parseWithHandling(parser, args); parseResult.has_value())
                return *parseResult;

            try
            {
                const std::filesystem::path headerPath = std::filesystem::absolute(parser.GetValuesOf<std::string>("HEADER").front()).make_preferred();
                const std::string realmName = parser.GetValuesOf<std::string>("REALM").front();
                const std::string outputArgument = parser.GetValuesOf<std::string>("OUTPUT").front();
                const std::string explicitHeaderInclude = parser.GetValuesOf<std::string>("HEADER-INCLUDE").front();
                const bool preferFlagset = getFlagValue(parser, "PREFER-FLAGSET");

                std::filesystem::path outputPath;
                if (outputArgument.empty())
                {
                    outputPath = headerPath.parent_path() / (headerPath.stem().string() + ".wio");
                }
                else
                {
                    outputPath = std::filesystem::path(outputArgument);
                    if (!outputPath.is_absolute())
                        outputPath = std::filesystem::absolute(outputPath).make_preferred();
                }

                const std::string headerInclude = explicitHeaderInclude.empty() ? headerPath.filename().string() : explicitHeaderInclude;
                const std::string cleanText = removeCComments(readUtf8File(headerPath));
                const ImportedDeclarations declarations = parseNamespaceAwareDeclarations(cleanText, preferFlagset);

                std::vector<std::string> lines;
                appendLine(lines, "// Generated by wio bind import");
                appendLine(lines, "// Source header: " + headerPath.string());
                appendLine(lines);
                appendLine(lines, "realm " + realmName + " {");
                appendLine(lines, "    use @CppHeader(" + quoteWioStringLiteral(headerInclude) + ");");
                appendLine(lines);

                for (const ImportedConstantSpec& constant : declarations.constants)
                    appendLine(lines, "    const " + constant.name + ": " + constant.type + " = " + constant.value + ";");

                if (!declarations.constants.empty())
                    appendLine(lines);

                for (const ImportedEnumSpec& enumSpec : declarations.enums)
                {
                    appendLine(lines, "    @Native");
                    if (const auto attribute = convertUnderlyingTypeToAttribute(enumSpec.underlyingType); attribute.has_value())
                        appendLine(lines, "    " + *attribute);
                    appendLine(lines, "    @CppName(" + enumSpec.cppName + ")");
                    appendLine(lines, "    " + std::string(enumSpec.isFlagset ? "flagset " : "enum ") + enumSpec.name + " {");
                    for (const ImportedEnumMember& member : enumSpec.members)
                    {
                        if (member.value.has_value())
                            appendLine(lines, "        " + member.name + " = " + *member.value + ",");
                        else
                            appendLine(lines, "        " + member.name + ",");
                    }
                    appendLine(lines, "    };");
                    appendLine(lines);
                }

                for (const ImportedStructSpec& structSpec : declarations.structs)
                {
                    appendLine(lines, "    @Native");
                    appendLine(lines, "    @CppName(" + structSpec.cppName + ")");
                    appendLine(lines, "    component " + structSpec.name + " {");
                    for (const ImportedFieldSpec& field : structSpec.fields)
                        appendLine(lines, "        " + field.name + ": " + field.type + ";");
                    appendLine(lines, "    }");
                    appendLine(lines);
                }

                for (const ImportedFunctionSpec& functionSpec : declarations.functions)
                {
                    appendLine(lines, "    @Native");
                    appendLine(lines, "    @CppName(" + functionSpec.cppName + ")");

                    std::ostringstream parameterList;
                    for (size_t i = 0; i < functionSpec.parameters.size(); ++i)
                    {
                        if (i > 0)
                            parameterList << ", ";
                        parameterList << functionSpec.parameters[i];
                    }

                    if (functionSpec.returnType == "void")
                        appendLine(lines, "    fn " + functionSpec.name + "(" + parameterList.str() + ");");
                    else
                        appendLine(lines, "    fn " + functionSpec.name + "(" + parameterList.str() + ") -> " + functionSpec.returnType + ";");

                    appendLine(lines);
                }

                appendLine(lines, "}");

                writeUtf8File(outputPath, joinLines(lines));
                std::cout << "Generated Wio binding module: " << outputPath.string() << '\n';
                return EXIT_SUCCESS;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Binding import failed: " << e.what() << '\n';
                return EXIT_FAILURE;
            }
        }
    }

    std::optional<int> tryHandleBindCommand(int argc, char* argv[])
    {
        if (argc < 2 || argv == nullptr || argv[1] == nullptr)
            return std::nullopt;

        const std::string_view command = argv[1];
        if (command != "bind")
            return std::nullopt;

        if (argc < 3 || argv[2] == nullptr)
        {
            std::cout
                << "Wio binding commands\n\n"
                << "Usage:\n"
                << "  wio bind new    --manifest FILE [--output FILE]\n"
                << "  wio bind import --header FILE --realm NAME [--output FILE] [--header-include FILE] [--prefer-flagset]\n";
            return EXIT_SUCCESS;
        }

        const std::string_view subcommand = argv[2];
        if (cli::IsHelpToken(subcommand))
        {
            std::cout
                << "Wio binding commands\n\n"
                << "Usage:\n"
                << "  wio bind new    --manifest FILE [--output FILE]\n"
                << "  wio bind import --header FILE --realm NAME [--output FILE] [--header-include FILE] [--prefer-flagset]\n";
            return EXIT_SUCCESS;
        }
        if (subcommand == "new")
            return handleBindNewCommand(collectCommandArgs("wio bind new", argc, argv, 3));
        if (subcommand == "import")
            return handleBindImportCommand(collectCommandArgs("wio bind import", argc, argv, 3));

        std::cerr << "Unknown bind subcommand: " << subcommand << '\n';
        if (const auto suggestion = cli::SuggestCommand(subcommand, { "new", "import" });
            suggestion.has_value())
        {
            std::cerr << "Did you mean 'wio bind " << *suggestion << "'?\n";
        }
        std::cerr << "Run 'wio bind --help' to list available binding commands.\n";
        return EXIT_FAILURE;
    }
}
