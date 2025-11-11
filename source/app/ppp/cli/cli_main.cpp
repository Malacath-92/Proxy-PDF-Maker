#include <ppp/cli/cli_main.hpp>

#include <ranges>
#include <span>
#include <string_view>

#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

constexpr const char c_HelpStr[]{
    R"(
Command Line Interface for Proxy-PDF-Maker

    --help              Display this information.
    --no-ui             Do not create a window.
    --cropper           Wait for the cropper, then quit.
    --render            Wait for the cropper, render the pdf, then quit.
    --project <file>    Load the project from this file.
    --project <json>    Load the project from this json blob.
    --project           Take all following commands and override 
                        project settings with them.
    
Project Overrides are formatted as follows:
    --<name> <value>    Will override the property <name> with the
                        value <value> as if parsed as json, where
                        <name> can be a nested name and refers to
                        the names seen in proj.json files.
                        For example:
                            --file_name output_file
                            --spacing.height 0.0
                            --spacing.with 1.5
)"
};

CommandLineOptions ParseCommandLine(int argc, char** raw_argv)
{
    using namespace std::string_view_literals;

    std::span argv{ raw_argv, static_cast<size_t>(argc) };

    CommandLineOptions cli;

    if (std::ranges::contains(argv, "--help"sv))
    {
        fmt::print("{}", c_HelpStr);
        cli.m_HelpDisplayed = true;
        return cli;
    }


    size_t i{ 0 };
    for (; i < argv.size(); i++)
    {
        const std::string_view arg{ argv[i] };
        if (arg == "--no-ui")
        {
            cli.m_NoUI = true;
        }
        else if (arg == "--cropper")
        {
            cli.m_WaitForCropper = true;
        }
        else if (arg == "--render")
        {
            cli.m_WaitForCropper = true;
            cli.m_Render= true;
        }
        else if (arg == "--project")
        {
            if (i + 1 < argv.size() &&
                std::string_view{ argv[i + 1] }.starts_with("--"))
            {
                // Parse overrides from now on out
                ++i;
                break;
            }
            else
            {
                ++i;
                std::string param{ argv[i] };
                if (fs::exists(param))
                {
                    cli.m_ProjectFile = std::move(param);
                }
                else
                {
                    cli.m_ProjectJson = std::move(param);
                }
            }
        }
    }

    for (; i < argv.size() - 1; i += 2)
    {
        const std::string_view arg{ argv[i] };
        if (!arg.starts_with("--"))
        {
            LogError("Error while parsing project overrides. Expected --<value> but got {}", argv[i]);
            return {};
        }

        const std::string_view param{ argv[i + 1] };
        cli.m_ProjectOverrides[std::string{ arg }] = param;
    }

    return cli;
}
