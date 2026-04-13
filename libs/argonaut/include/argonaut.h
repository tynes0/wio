#pragma once

#ifndef ARGONAUT_H_DEFINED
#define ARGONAUT_H_DEFINED

#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <map>          // std::map
#include <set>          // std::set
#include <stdexcept>    // std::runtime_error
#include <algorithm> // std::transform
#include <sstream>  // std::stringstream
#include <iomanip>  // std::setw, std::left
#include <ranges> // views

/**
 * @brief Main namespace for the Argonaut parser library.
 */
namespace Argonaut
{
    /**
     * @brief Base exception class for all Argonaut errors.
     */
    class Exception : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };
    
    /**
     * @brief Exception thrown during the parser setup phase (e.g., adding arguments).
     */
    class ParsePrepException : public Exception {
    public:
        using Exception::Exception;
    };
    
    /**
     * @brief Exception thrown during the parsing phase (e.g., invalid user input).
     */
    class ParseException : public Exception {
    public:
        using Exception::Exception;
    };

    /**
     * @brief Thrown when parsing is successful but --help was requested.
     * The application should catch this, print the message, and exit gracefully.
     */
    class HelpRequestedException : public Exception {
    public:
        using Exception::Exception;
    };

    /**
     * @brief Thrown when parsing is successful but --version was requested.
     * The application should catch this, print the message, and exit gracefully.
     */
    class VersionRequestedException : public Exception {
    public:
        using Exception::Exception;
    };
    
    /**
     * @brief Defines a single command-line argument (option, flag, or positional).
     *
     * This struct uses a fluent API (method chaining) to allow for easy
     * and readable argument definition.
     */
    struct Argument
    {
        /**
         * @brief Grants Parser access to private members for registration and parsing.
         */
        friend class Parser;
        
        /**
         * @brief Constructs an Argument with a required ID.
         * @param arg_id The unique (case-insensitive) identifier for this argument.
         */
        explicit Argument(std::string arg_id)
            : Id_(std::move(arg_id))
        {
            // Normalize ID to uppercase for case-insensitive lookup
            for (auto& ch : Id_)
                ch = static_cast<char>(::toupper(ch));
        }

        /**
         * @brief Default constructor.
         */
        Argument() = default;
        /** @brief Default copy constructor. */
        Argument(const Argument&) = default;
        /** @brief Default move constructor. */
        Argument(Argument&&) = default;
        /** @brief Default copy assignment operator. */
        Argument& operator=(const Argument&) = default;
        /** @brief Default move assignment operator. */
        Argument& operator=(Argument&&) = default;
        /** @brief Default destructor. */
        ~Argument() = default;

        /**
         * @brief Sets the unique (case-insensitive) ID for the argument.
         * @param id The argument's ID.
         * @return A reference to this Argument for chaining.
         */
        Argument& SetId(const std::string& id)
        {
            Id_ = id;
            for (auto& ch : Id_)
                ch = static_cast<char>(::toupper(ch));
            return *this;
        }

        /**
         * @brief Sets the description text, used in the --help message.
         * @param desc The description string.
         * @return A reference to this Argument for chaining.
         */
        Argument& SetDescription(const std::string& desc)
        {
            Description_ = desc;
            return *this;
        }

        /**
         * @brief Adds an alias (e.g., "-f" or "--file") to trigger this argument.
         * @param alias The alias string.
         * @return A reference to this Argument for chaining.
         */
        Argument& AddAlias(const std::string& alias)
        {
            Aliases_.push_back(alias);
            return *this;
        }
        
        /**
         * @brief Marks this argument as required.
         * @note Flags (IsFlag_ == true) cannot be required.
         * @param value True to mark as required, false otherwise.
         * @return A reference to this Argument for chaining.
         * @throws ParsePrepException if the argument is already a flag.
         */
        Argument& Required(bool value = true)
        {
            if (IsFlag_)
                throw ParsePrepException("Argonaut Exception: Flag argument cannot be required."); // I am not sure about this actually
            Required_ = value;
            return *this;
        }

        /**
         * @brief Marks this argument as a "flag" (an option that takes no value).
         * @note Multi-value arguments cannot be flags.
         * @param value True to mark as a flag, false otherwise.
         * @return A reference to this Argument for chaining.
         * @throws ParsePrepException if the argument is already multi-value.
         */
        Argument& Flag(bool value = true)
        {
            if (IsMultiValue_)
                throw ParsePrepException("Argonaut Exception: Multi-value argument cannot be flag.");
            IsFlag_ = value;
            return *this;
        }
        
        /**
         * @brief Allows this argument to be specified multiple times.
         * @note Flags cannot be multi-value.
         * @param value True to allow multiple values, false otherwise.
         * @return A reference to this Argument for chaining.
         * @throws ParsePrepException if the argument is already a flag.
         */
        Argument& MultiValue(bool value = true)
        {
            if (IsFlag_)
                throw ParsePrepException("Argonaut Exception: Flag argument cannot be multi-value.");
            IsMultiValue_ = value;
            return *this;
        }
        
        /**
         * @brief Sets a default value if the argument is not provided.
         * @param value The default value string.
         * @return A reference to this Argument for chaining.
         */
        Argument& SetDefaultValue(const std::string& value)
        {
            DefaultValue_ = value;
            return *this;
        }

        /**
         * @brief Sets a callback function to be executed when the argument is parsed.
         * @param cb The callback function, which takes a `const std::string&` (the value).
         * @return A reference to this Argument for chaining.
         */
        Argument& SetCallback(const std::function<void(const std::string&)>& cb)
        {
            Callback_ = cb;
            return *this;
        }

    private:
        /// @brief The unique, case-insensitive identifier for the argument.
        std::string Id_;
        /// @brief The help-text description.
        std::string Description_;
        /// @brief A list of aliases that trigger this argument (e.g., "-f", "--file").
        std::vector<std::string> Aliases_;
        /// @brief The default value if not provided.
        std::optional<std::string> DefaultValue_;
        /// @brief An optional callback to run when the argument is found.
        std::function<void(const std::string& value)> Callback_;
        /// @brief True if this argument is a flag (takes no value).
        bool IsFlag_ = false;
        /// @brief True if this argument must be provided.
        bool Required_ = false;
        /// @brief True if this argument can be provided multiple times.
        bool IsMultiValue_ = false;
    };

    // --- Helper for static_assert ---
    template<typename T>
    struct AlwaysFalse : std::false_type {};

    /**
     * @brief Main converter template.
     * Users can customize it for their own type.
     */
    template<typename T>
    struct ArgumentConverter
    {
        static T Convert(const std::string& value)
        {
            // If this line compiles, the user called it with an unsupported T type.
            static_assert(AlwaysFalse<T>::value, 
                "Argonaut: No conversion defined for this type. Please specialize Argonaut::ArgumentConverter<T>.");
            return T{};
        }
    };

    // --- Specializations for Standard Types (Built-in Specializations) ---
    template<> struct ArgumentConverter<int>
    {
        static int Convert(const std::string& v)
        {
            return std::stoi(v);
        }
    };

    template<> struct ArgumentConverter<long>
    {
        static long Convert(const std::string& v)
        {
            return std::stol(v);
        }
    };

    template<> struct ArgumentConverter<long long>
    {
        static long long Convert(const std::string& v)
        {
            return std::stoll(v);
        }
    };

    template<> struct ArgumentConverter<unsigned long>
    {
        static unsigned long Convert(const std::string& v)
        {
            return std::stoul(v);
        }
    };

    template<> struct ArgumentConverter<unsigned long long>
    {
        static unsigned long long Convert(const std::string& v)
        {
            return std::stoull(v);
        }
    };

    template<> struct ArgumentConverter<float>
    {
        static float Convert(const std::string& v)
        {
            return std::stof(v);
        }
    };

    template<> struct ArgumentConverter<double>
    {
        static double Convert(const std::string& v)
        {
            return std::stod(v);
        }
    };

    template<> struct ArgumentConverter<std::string>
    {
        static std::string Convert(const std::string& v)
        {
            return v;
        }
    };

    template<> struct ArgumentConverter<bool>
    {
        static bool Convert(const std::string& v)
        {
            std::string tmp = v;
            for (auto& ch : tmp)
                ch = static_cast<char>(::tolower(ch));
            return (tmp == "true" || tmp == "1" || tmp == "yes" || tmp == "on");
        }
    };

    /**
     * @brief The main command-line argument parser class.
     *
     * This class manages argument definitions, runs the parsing logic,
     * and provides access to the parsed values.
     */
    class Parser
    {
    public:
        /**
         * @brief Parses command-line arguments from (argc, argv).
         * @param argc The argument count (from main).
         * @param argv The argument vector (from main).
         */
        void Parse(int argc, char* argv[])
        {
            std::vector<std::string> args;
            if (argc > 0)
            {
                args.reserve(argc);
                for(int i = 0; i < argc; ++i)
                {
                    // Use emplace_back for potentially more efficient string construction
                    args.emplace_back(argv[i]);
                }
            }
            Parse(args);
        }
        
        /**
         * @brief Adds a new argument definition to the parser.
         * @param arg The Argument object to add.
         * @return A reference to this Parser for chaining.
         * @throws ParsePrepException if the ID or an alias already exists.
         */
        Parser& Add(const Argument& arg)
        {
            std::string id = arg.Id_; // Id_ is already uppercase from Argument constructor

            if (ArgumentDefinitions_.contains(id)) {
                throw ParsePrepException("Argonaut Exception: Argument already exists: " + id);
            }
            
            // Register all aliases in the lookup map
            for (const auto& alias : arg.Aliases_)
            {
                if (AliasToIdMap_.contains(alias)) {
                    throw ParsePrepException("Argonaut Exception: Alias already exists: " + alias);
                }
                AliasToIdMap_[alias] = id;
            }
            
            // If it has no aliases, it's a positional argument
            if (arg.Aliases_.empty())
            {
                RequiredPositionalArgOrder_.push_back(id);
            }

            // Store the complete argument definition
            ArgumentDefinitions_[id] = arg;
            
            return *this;
        }

        /**
         * @brief Enables or disables automatic handling of --help and -h.
         * @param value True to enable (default), false to disable.
         * @return A reference to this Parser for chaining.
         */
        Parser& AutoHelp(bool value = true)
        {
            AutoHelp_ = value;

            if (AutoHelp_)
            {
                // Add the built-in "HELP" argument
                Argument HelpArgument("HELP");
                HelpArgument
                    .AddAlias("--help")
                    .AddAlias("-h")
                    .Flag()
                    .SetCallback([&](const std::string&){ throw HelpRequestedException(Help()); }) // Print help and exit
                    .SetDescription("Show this help menu.");

                Add(HelpArgument);
            }
            else
            {
                // Remove the built-in "HELP" argument if it exists
                if (ArgumentDefinitions_.contains("HELP"))
                    ArgumentDefinitions_.erase("HELP");

                if (AliasToIdMap_.contains("-h"))
                    AliasToIdMap_.erase("-h");

                if (AliasToIdMap_.contains("--help"))
                    AliasToIdMap_.erase("--help");
            }
            
            return *this;
        }

        /**
         * @brief Enables or disables automatic handling of --version and -v.
         * @param value True to enable (default), false to disable.
         * @return A reference to this Parser for chaining.
         */
        Parser& AutoVersion(bool value = true)
        {
            AutoVersion_ = value;

            if (AutoVersion_)
            {
                // Add the built-in "VERSION" argument
                Argument VersionArgument("VERSION");
                VersionArgument
                    .AddAlias("--version")
                    .AddAlias("-v")
                    .Flag()
                    .SetCallback([&](const std::string&){ throw VersionRequestedException(Help()); }) // Print version and exit
                    .SetDescription("Show version.");

                Add(VersionArgument);
            }
            else
            {
                // Remove the built-in "VERSION" argument if it exists
                if (ArgumentDefinitions_.contains("VERSION"))
                    ArgumentDefinitions_.erase("VERSION");

                if (AliasToIdMap_.contains("-v"))
                    AliasToIdMap_.erase("-v");

                if (AliasToIdMap_.contains("--version"))
                    AliasToIdMap_.erase("--version");
            }
 
            return *this;
        }
        
        /**
         * @brief Sets the program's version string, used by --version.
         * @param version The version string.
         * @return A reference to this Parser for chaining.
         */
        Parser& SetVersion(const std::string& version)
        {
            Version_ = version;
            return *this;
        }
        
        /**
         * @brief Gets the name of the program (from argv[0]).
         * @return The program name string.
         */
        std::string GetProgramName() const
        {
            return ProgramName_;
        }

        /**
         * @brief Gets all parsed values for a given argument ID.
         * @param ArgumentID The case-insensitive ID of the argument to retrieve.
         * @return A vector of strings. Returns the default value if not present,
         * or an empty vector if not present and no default.
         */
        template <typename T = std::string>
        std::vector<T> GetValuesOf(const std::string& ArgumentID)
        {
            // 1. ID Normalization and Search 
            std::string ID = ArgumentID;
            for (auto& ch : ID)
                ch = static_cast<char>(::toupper(ch));
            
            auto ArgIt = ArgumentDefinitions_.find(ID);
            if (ArgIt == ArgumentDefinitions_.end())
                return {}; 
            
            std::vector<std::string> StrValues;
            auto It = ParsedValues_.find(ID);
            
            if (It != ParsedValues_.end())
            {
                StrValues = It->second;
            } else
            {
                if (!ArgIt->second.DefaultValue_.has_value())
                    return {};
                
                StrValues.push_back(ArgIt->second.DefaultValue_.value());
            }

            // 2. Return directly if it is a String (Mini Optimization)
            if constexpr (std::is_same_v<T, std::string>)
            {
                return StrValues;
            }
            else 
            {
                // 3. Converting Using Converter
                std::vector<T> Result;
                Result.reserve(StrValues.size());

                for (const auto& strVal : StrValues)
                {
                    try 
                    {
                        // Call the Convert function of the corresponding type
                        Result.push_back(ArgumentConverter<T>::Convert(strVal));
                    }
                    catch (const std::exception& e)
                    {
                        throw ParseException("Argonaut Exception: Conversion failed for argument '" + ID + "'. Value: '" += strVal + "'. Error: " + e.what());
                    }
                }
                return Result;
            }
        }

        /**
         * @brief Gets the entire map of parsed values.
         * @return A const reference to the map (ID -> vector of values).
         */
        const std::map<std::string, std::vector<std::string>>& GetAllValues() const
        {
            return ParsedValues_;
        }

        /**
         * @brief Generates a formatted help string based on the defined arguments.
         * @return A string containing the full help message.
         */
        std::string Help() const
        {
            std::stringstream ss;

            // --- 1. Usage Line ---
            ss << "Usage: " << ProgramName_;

            // Separate positional args from options
            std::vector<Argument> positionals;
            std::vector<Argument> options;
            positionals.reserve(RequiredPositionalArgOrder_.size());
            
            for (const auto& id : RequiredPositionalArgOrder_)
            {
                positionals.push_back(ArgumentDefinitions_.at(id));
            }

            for (const auto& arg : std::views::values(ArgumentDefinitions_))
            {
                // An argument is an "option" if it has aliases (e.g., -f)
                if (!arg.Aliases_.empty())
                {
                    options.push_back(arg);
                }
            }

            if (!options.empty()) // Don't check AutoHelp/Version, they are in 'options'
            {
                ss << " [OPTIONS]";
            }

            // Add positional arguments to usage line
            for (const auto& arg : positionals)
            {
                ss << " ";
                std::string name = arg.Id_; // ID is already uppercase
                
                // Required = <NAME>, Optional = [NAME]
                if (arg.Required_)
                {
                    ss << "<" << name << ">";
                }
                else
                {
                    ss << "[" << name << "]";
                }
                
                if (arg.IsMultiValue_)
                {
                    ss << "..."; // Indicate multiple values
                }
            }
            ss << "\n\n";

            // --- 2. Positional Arguments Section ---
            if (!positionals.empty())
            {
                ss << "Positional Arguments:\n";
                // Find longest ID for alignment
                size_t max_len = 0;
                for (const auto& arg : positionals)
                {
                    max_len = std::max(max_len, arg.Id_.length());
                }
                max_len += 4; // Padding

                // Print each positional arg
                for (const auto& arg : positionals)
                {
                    ss << "  " << std::setw(static_cast<std::streamsize>(max_len)) << std::left << arg.Id_
                       << arg.Description_ << "\n";
                    
                    // Print extras like [REQUIRED] on a new line
                    std::string extras;
                    if (arg.Required_) extras += "[REQUIRED] ";
                    if (arg.IsMultiValue_) extras += "[Multi-value] ";
                    
                    if (!extras.empty()) {
                        ss << "  " << std::setw(static_cast<std::streamsize>(max_len)) << std::left << "" 
                           << extras << "\n";
                    }
                }
                ss << "\n";
            }

            // --- 3. Options Section ---
            if (!options.empty()) // Don't check AutoHelp/Version, they are in 'options'
            {
                ss << "Options:\n";
                
                size_t max_alias_len = 0;
                std::vector<std::string> alias_strings;
                alias_strings.reserve(options.size());

                // First pass: build alias strings and find max length for alignment
                for (const auto& arg : options)
                {
                    std::string alias_str;
                    // Add <value> placeholder if it's not a flag
                    std::string value_placeholder = arg.IsFlag_ ? "" : " <value>"; 
                    
                    for (size_t i = 0; i < arg.Aliases_.size(); ++i)
                    {
                        alias_str += arg.Aliases_[i] + value_placeholder;
                        if (i < arg.Aliases_.size() - 1) {
                            alias_str += ", "; // Comma-separate aliases
                        }
                    }
                    alias_strings.push_back(alias_str);
                    max_alias_len = std::max(max_alias_len, alias_str.length());
                }
                
                max_alias_len += 4; // Padding

                // Second pass: print aligned options
                for (size_t i = 0; i < options.size(); ++i)
                {
                    const auto& arg = options[i];
                    
                    ss << "  " << std::setw(static_cast<std::streamsize>(max_alias_len)) << std::left << alias_strings[i]
                       << arg.Description_ << "\n";
                    
                    // Print extras like [Default: "..."]
                    std::string extras;
                    if (arg.IsMultiValue_) extras += "[Multi-value] ";
                    if (arg.DefaultValue_.has_value()) {
                        extras += "[Default: \"" + arg.DefaultValue_.value() + "\"]";
                    }
                    
                    if (!extras.empty()) {
                        ss << "  " << std::setw(static_cast<std::streamsize>(max_alias_len)) << std::left << "" 
                           << extras << "\n";
                    }
                }
            }

            return ss.str();
        }

    private:
        /**
         * @brief Internal parsing function that processes a vector of string arguments.
         * @param args The vector of arguments, where args[0] is the program name.
         * @throws ParseException if any parsing errors occur.
         */
        void Parse(const std::vector<std::string>& args)
        {
            if (args.empty()) {
                throw ParseException("Argonaut Exception: Argument list is empty.");
            }

            ProgramName_ = args[0]; // Store program name
            
            // --- Parser State Machine ---
            bool NextIsPositional = false; // True after "--" is encountered
            bool NextIsValue = false;      // True after an option expecting a value is found
            std::string IdForValue;        // Stores the ID of the option expecting a value

            size_t PositionalArgumentIndex = 0; // Tracks which positional arg is next

            // Loop starts at 1 to skip the program name
            for (size_t i = 1; i < args.size(); ++i)
            {
                const std::string& Arg = args[i];
                
                if (Arg.empty())
                    throw ParseException("Argonaut Exception: Empty argument.");

                // --- State 1: Handling tokens after "--" ---
                if (NextIsPositional)
                {
                    //NextIsPositional = false; // Keep this true
                    
                    Argument& RealArg = ArgumentDefinitions_[RequiredPositionalArgOrder_[PositionalArgumentIndex]];
                    auto& ParsedList = ParsedValues_[RealArg.Id_];
                    
                    ParsedList.emplace_back(Arg);
                    CallbackIfValid(RealArg, Arg);
                    
                    if (!RealArg.IsMultiValue_)
                        PositionalArgumentIndex++;
                    continue;
                }

                // --- State 2: Handling a value for a previous option ---
                if (NextIsValue)
                {
                    NextIsValue = false; // Reset state
                    Argument& RealArg = ArgumentDefinitions_[IdForValue];
                    auto& ParsedList = ParsedValues_[IdForValue];
                    
                    // Check for duplicate if not multi-value
                    if (!ParsedList.empty() && !ArgumentDefinitions_[IdForValue].IsMultiValue_)
                        throw ParseException("Argonaut Exception: Argument is not multi-value: " + IdForValue);
                    
                    ParsedList.emplace_back(Arg);
                    CallbackIfValid(RealArg, Arg);
                    continue;
                }
                
                // --- State 3: Check for "--" separator ---
                if (Arg == "--")
                {
                    NextIsPositional = true;
                    continue;
                }

                // --- State 4: Check for long option (e.g., --file or --file=value) ---
                if (Arg.starts_with("--"))
                {
                    std::string::size_type pos = Arg.find('=');
                    std::string key = Arg.substr(0, (pos == std::string::npos) ? std::string::npos : pos);

                    // Find the argument ID from the alias
                    auto IdIt = AliasToIdMap_.find(key);
                    if (IdIt == AliasToIdMap_.end())
                        throw ParseException("Argonaut Exception: Undefined argument: " + key);
                    
                    std::string ID = IdIt->second;

                    auto ArgIt = ArgumentDefinitions_.find(ID);
                    if (ArgIt == ArgumentDefinitions_.end()) // Should not happen if AliasToIdMap_ is correct
                        throw ParseException("Argonaut Exception: Undefined argument: " + key);
                        
                    Argument& RealArg = ArgIt->second;
                    auto& ParsedList = ParsedValues_[ID];

                    if (RealArg.IsFlag_)
                    {
                        // Flags cannot have values (e.g., --verbose=true)
                        if (pos != std::string::npos)
                            throw ParseException("Argonaut Exception: Flag argument cannot have a value: " + key);

                        std::string Value = "true"; // Represent flag presence
                        
                        ParsedList.emplace_back(Value);
                        CallbackIfValid(RealArg, Value);
                        continue;
                    }
                    
                    // --- Handle long option with =value ---
                    if (pos != std::string::npos && pos != Arg.size() - 1)
                    {
                        // Check for duplicate if not multi-value
                        if (!ParsedList.empty() && !RealArg.IsMultiValue_)
                            throw ParseException("Argonaut Exception: Argument is not multi-value: " + key);

                        std::string Value = Arg.substr(pos + 1);
                        
                        ParsedList.emplace_back(Value);
                        CallbackIfValid(RealArg, Value);
                        continue;
                    }

                    // --- Handle long option expecting a value next (e.g., --file main.cpp) ---
                    NextIsValue = true;
                    IdForValue = ID;
                }
                // --- State 5: Check for short option (e.g., -f) ---
                // TODO: Add support for clustering (e.g., -xvf)
                else if (Arg.starts_with('-'))
                {
                    // Find the argument ID from the alias
                    auto IdIt = AliasToIdMap_.find(Arg);
                    if (IdIt == AliasToIdMap_.end())
                        throw ParseException("Argonaut Exception: Undefined argument: " + Arg);
                    
                    std::string ID = IdIt->second;

                    auto ArgIt = ArgumentDefinitions_.find(ID);
                    if (ArgIt == ArgumentDefinitions_.end()) // Should not happen
                        throw ParseException("Argonaut Exception: Undefined argument: " + Arg);

                    Argument& RealArg = ArgIt->second;
                    auto& ParsedList = ParsedValues_[ID];

                    if (RealArg.IsFlag_)
                    {
                        std::string Value = "true";
                        ParsedList.emplace_back(Value);
                        CallbackIfValid(RealArg, Value);
                        continue;
                    }

                    // --- Handle short option expecting a value next (e.g., -f main.cpp) ---
                    NextIsValue = true;
                    IdForValue = ID;
                }
                // --- State 6: Handle positional argument ---
                else
                {
                    if (RequiredPositionalArgOrder_.size() <= PositionalArgumentIndex) // Corrected logic
                        throw ParseException("Argonaut Exception: Too many positional arguments.");
                    
                    Argument& RealArg = ArgumentDefinitions_[RequiredPositionalArgOrder_[PositionalArgumentIndex]];
                    auto& ParsedList = ParsedValues_[RealArg.Id_];
                    
                    ParsedList.emplace_back(Arg);
                    CallbackIfValid(RealArg, Arg);
                    
                    if (!RealArg.IsMultiValue_)
                        PositionalArgumentIndex++;
                }
            }

            // --- Post-loop validation ---
            
            // This check seems wrong, NextIsPositional just means "--" was seen.
            // if (NextIsPositional)
            //    throw ParseException("Argonaut Exception: Positional arguments must be after '--'.");

            // Check if an option was waiting for a value that never came
            if (NextIsValue)
                throw ParseException("Argonaut Exception: Last argument should take value.");

            // Check if all required values are met
            for (auto& RealArg : std::views::values(ArgumentDefinitions_))
            {
                if (RealArg.Required_)
                {
                    auto it = ParsedValues_.find(RealArg.Id_);
                    if (it == ParsedValues_.end() && !RealArg.DefaultValue_.has_value())
                        throw ParseException("Argonaut Exception: Required value " + RealArg.Id_ + " not found.");
                }
            }
            // If no arguments were parsed AND auto-help is on, show help.
            if (ParsedValues_.empty() && AutoHelp_ && args.size() == 1)
                throw HelpRequestedException(Help());
        }

        /**
         * @brief A static helper to safely invoke an argument's callback.
         * @param Arg The argument definition.
         * @param Value The parsed value to pass to the callback.
         */
        static void CallbackIfValid(const Argument& Arg, const std::string& Value)
        {
            if (Arg.Callback_)
                Arg.Callback_(Value);
        }
        
        /// @brief The name of the executable (from argv[0]).
        std::string ProgramName_;
        
        /// @brief Main storage for all defined Argument structs, keyed by their uppercase ID.
        std::map<std::string, Argument> ArgumentDefinitions_;
        
        /// @brief Stores the IDs of positional arguments in the order they are expected.
        std::vector<std::string> RequiredPositionalArgOrder_;
        
        /// @brief A fast lookup map from an alias (e.g., "-f") to its uppercase ID (e.g., "FILE").
        std::map<std::string, std::string> AliasToIdMap_;
        
        /// @brief Stores the results of parsing. Maps uppercase ID to a vector of found values.
        std::map<std::string, std::vector<std::string>> ParsedValues_;
        
        /// @brief The program version string.
        std::string Version_;
        
        /// @brief Flag to control automatic --help handling.
        bool AutoHelp_ = false;
        
        /// @brief Flag to control automatic --version handling.
        bool AutoVersion_ = false;
    };
    
} // namespace Argonaut

#endif // ARGONAUT_H_DEFINED