#include <argonaut.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "Argonaut parser smoke failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void parse(Argonaut::Parser& parser, std::vector<std::string> args)
    {
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (std::string& arg : args)
            argv.push_back(arg.data());
        parser.Parse(static_cast<int>(argv.size()), argv.data());
    }
}

int main()
{
    {
        Argonaut::Parser parser;
        parser
            .Add(Argonaut::Argument("FLAG").AddAlias("--flag").Flag())
            .AutoHelp()
            .HelpOnEmpty(false);

        parse(parser, { "argonaut-smoke", "--flag" });
        require(parser.WasProvided("flag"), "explicit flag was not recorded");

        parse(parser, { "argonaut-smoke" });
        require(!parser.WasProvided("flag"), "parsed values leaked between Parse calls");
    }

    {
        Argonaut::Parser parser;
        parser.AutoVersion().SetVersion("1.2.3");

        bool requestedVersion = false;
        try
        {
            parse(parser, { "argonaut-smoke", "--version" });
        }
        catch (const Argonaut::VersionRequestedException& exception)
        {
            requestedVersion = true;
            require(std::string(exception.what()) == "1.2.3\n",
                    "version output included help text or the wrong value");
        }
        require(requestedVersion, "--version did not request version output");
    }

    {
        Argonaut::Parser parser;
        parser.HelpOnEmpty(false);

        bool rejectedExtraPositional = false;
        try
        {
            parse(parser, { "argonaut-smoke", "--", "extra" });
        }
        catch (const Argonaut::ParseException&)
        {
            rejectedExtraPositional = true;
        }
        require(rejectedExtraPositional, "'--' overflow was not rejected");
    }

    {
        Argonaut::Parser parser;
        parser
            .Add(Argonaut::Argument("NAME").AddAlias("--name"))
            .HelpOnEmpty(false);
        parse(parser, { "argonaut-smoke", "--name=" });

        const auto values = parser.GetValuesOf<std::string>("name");
        require(values.size() == 1 && values.front().empty(),
                "empty --option= value was not preserved");
    }

    {
        Argonaut::Parser parser;
        parser.Add(Argonaut::Argument("TAKEN").AddAlias("--taken"));

        bool duplicateRejected = false;
        try
        {
            parser.Add(
                Argonaut::Argument("BAD")
                    .AddAlias("--free")
                    .AddAlias("--taken")
            );
        }
        catch (const Argonaut::ParsePrepException&)
        {
            duplicateRejected = true;
        }
        require(duplicateRejected, "duplicate alias was not rejected");

        parser.Add(Argonaut::Argument("GOOD").AddAlias("--free"));
    }

    {
        bool emptyIdRejected = false;
        try
        {
            Argonaut::Parser parser;
            parser.Add(Argonaut::Argument(""));
        }
        catch (const Argonaut::ParsePrepException&)
        {
            emptyIdRejected = true;
        }
        require(emptyIdRejected, "empty argument ID was accepted");
    }

    {
        Argonaut::Argument flag("FLAG");
        flag.Flag().Required(false).MultiValue(false).Flag(false);

        Argonaut::Argument required("REQUIRED");
        required.Required().Flag(false);

        bool conflictRejected = false;
        try
        {
            required.Flag();
        }
        catch (const Argonaut::ParsePrepException&)
        {
            conflictRejected = true;
        }
        require(conflictRejected, "required/flag conflict was accepted");
    }

    {
        Argonaut::Parser parser;
        parser
            .Add(Argonaut::Argument("VALUE").AddAlias("--value"))
            .HelpOnEmpty(false);
        parse(parser, { "argonaut-smoke", "--value", "perhaps" });

        bool invalidBooleanRejected = false;
        try
        {
            (void)parser.GetValuesOf<bool>("value");
        }
        catch (const Argonaut::ParseException&)
        {
            invalidBooleanRejected = true;
        }
        require(invalidBooleanRejected, "invalid boolean silently converted to false");
    }

    {
        Argonaut::Parser parser;
        parser
            .AutoHelp()
            .HelpOnEmpty(false)
            .SetUsageSuffix(" [-- application args...]");
        parse(parser, { "argonaut-smoke" });
        require(parser.Help().find("[-- application args...]") != std::string::npos,
                "usage suffix was omitted from help");
    }

    return EXIT_SUCCESS;
}
