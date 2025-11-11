#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include <QCoreApplication>
#include <QThreadPool>

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/util/log.hpp>

#include <ppp/config.hpp>

#include <ppp/pdf/generate.hpp>

struct CommandLineOptions
{
    bool m_HelpDisplayed{ false };

    bool m_Render{ false };
    bool m_WaitForCropper{ false };

    std::optional<std::string> m_ProjectFile{ std::nullopt };
    std::optional<std::string> m_ProjectJson{ std::nullopt };
    std::unordered_map<std::string, std::string> m_ProjectOverrides{};
};

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
        if (arg == "--cropper")
        {
            cli.m_WaitForCropper = true;
        }
        else if (arg == "--render")
        {
            cli.m_WaitForCropper = true;
            cli.m_Render = true;
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

int main(int argc, char** argv)
{
#ifdef WIN32
    {
        static constexpr char c_LocaleName[]{ ".utf-8" };
        std::setlocale(LC_ALL, c_LocaleName);
        std::locale::global(std::locale(c_LocaleName));
    }
#endif

    Log::RegisterThreadName("MainThread");

    LogFlags log_flags{
        LogFlags::Console |
        LogFlags::File |
        LogFlags::FatalQuit |
        LogFlags::DetailFile |
        LogFlags::DetailLine |
        LogFlags::DetailColumn |
        LogFlags::DetailThread |
        LogFlags::DetailStacktrace
    };
    Log main_log{ log_flags, Log::c_MainLogName };

    QThreadPool::globalInstance()->setMaxThreadCount(g_Cfg.m_MaxWorkerThreads);

    CommandLineOptions cli{ ParseCommandLine(argc, argv) };
    if (cli.m_HelpDisplayed)
    {
        return 0;
    }

    Project project{};
    if (cli.m_ProjectFile.has_value())
    {
        project.Load(cli.m_ProjectFile.value(),
                     cli.m_ProjectOverrides);
    }
    else if (cli.m_ProjectJson.has_value())
    {
        if (!project.LoadFromJson(cli.m_ProjectJson.value(),
                                  cli.m_ProjectOverrides))
        {
            LogError("Failed loading project from json-blob...");
        }
    }
    else
    {
        LogInfo("Starting from a clean project...");
    }

    std::mutex color_cubes_mutex;
    std::unordered_map<std::string, cv::Mat> color_cubes;
    auto get_color_cube{ [&](std::string_view cube_name) -> cv::Mat*
                         {
                             if (cube_name == "None")
                             {
                                 return nullptr;
                             }

                             std::string cube_name_str{ cube_name };
                             std::lock_guard lock{ color_cubes_mutex };
                             if (!color_cubes.contains(cube_name_str))
                             {
                                 color_cubes[cube_name_str] = LoadColorCube(cube_name_str);
                             }
                             return &color_cubes.at(cube_name_str);
                         } };

    Cropper cropper{ get_color_cube, project };
    CardProvider card_provider{ project };

    QObject::connect(&card_provider, &CardProvider::CardAdded, &project, &Project::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &project, &Project::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &project, &Project::CardRenamed);
    QObject::connect(&card_provider, &CardProvider::CardModified, &project, &Project::CardModified);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &cropper, &Cropper::CardRenamed);

    QObject::connect(&card_provider, &CardProvider::CardAdded, &cropper, &Cropper::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &cropper, &Cropper::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardModified, &cropper, &Cropper::CardModified);

    QObject::connect(&project, &Project::CardRotationChanged, &cropper, &Cropper::CardModified);
    QObject::connect(&project, &Project::CardBleedTypeChanged, &cropper, &Cropper::CardModified);
    QObject::connect(&project, &Project::CardBadAspectRatioHandlingChanged, &cropper, &Cropper::CardModified);

    // Write preview to project and forward to widgets
    QObject::connect(&cropper, &Cropper::PreviewUpdated, &project, &Project::SetPreview);

    // Write preview cache to file
    QObject::connect(&cropper, &Cropper::PreviewWorkDone, &project, &Project::CropperDone);

    QCoreApplication app{ argc, argv };
    if (cli.m_WaitForCropper)
    {
        if (cli.m_Render)
        {
            QObject::connect(&cropper,
                             &Cropper::CropWorkDone,
                             &project,
                             [&]
                             {
                                 GeneratePdf(project);
                                 app.quit();
                             });
        }
        else
        {
            QObject::connect(&cropper,
                             &Cropper::CropWorkDone,
                             &app,
                             &QCoreApplication::quit);
        }
    }

    cropper.Start();
    card_provider.Start();

    return app.exec();
}
