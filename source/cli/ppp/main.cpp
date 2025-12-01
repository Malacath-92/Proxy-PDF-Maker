#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <QCoreApplication>
#include <QThreadPool>

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/util/log.hpp>

#include <ppp/config.hpp>
#include <ppp/json_util.hpp>

#include <ppp/pdf/generate.hpp>

using ProjectOverrides = std::unordered_map<std::string, std::string>;

struct CommandLineOptions
{
    bool m_HelpDisplayed{ false };

    bool m_Render{ false };
    bool m_WaitForCropper{ false };

    bool m_Deterministic{ false };

    std::optional<std::string> m_ProjectFile{ std::nullopt };
    std::optional<std::string> m_ProjectJson{ std::nullopt };
    ProjectOverrides m_ProjectOverrides{};
};

constexpr const char c_HelpStr[]{
    R"(
Command Line Interface for Proxy-PDF-Maker

    --help              Display this information.
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

class OverridesProvider : public JsonProvider
{
  public:
    OverridesProvider(const ProjectOverrides& overrides)
        : m_Overrides{ overrides }
    {
    }

    virtual nlohmann::json GetJsonValue(std::string_view path_view) const override
    {
        const std::string path{ path_view };
        if (m_Overrides.contains(path))
        {
            const auto& value{ m_Overrides.at(path) };
            try
            {
                // Try parsing the override as a literal ...
                return nlohmann::json::parse(value);
            }
            catch (const nlohmann::json::parse_error&)
            {
                // ... and keep it as a string if that's not possible.
                return value;
            }
        }

        return nlohmann::json{};
    }

    virtual void SetJsonValue(std::string_view path, nlohmann::json /*value*/) override
    {
        LogError("SetJsonValue is not implemented for CLI, path {} is ignored...",
                 path);
    }

  private:
    const ProjectOverrides& m_Overrides;
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

    size_t i{ 1 };
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
        else if (arg == "--deterministic")
        {
            cli.m_Deterministic = true;
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
        else
        {
            LogError("Unknown command line option {}", arg);
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
        cli.m_ProjectOverrides[std::string{ arg.substr(2) }] = param;
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

    if (cli.m_Deterministic)
    {
        g_Cfg.m_DeterminsticPdfOutput = true;
    }

    OverridesProvider overrides_provider{
        cli.m_ProjectOverrides
    };

    Project project{};
    if (cli.m_ProjectFile.has_value())
    {
        project.Load(cli.m_ProjectFile.value(),
                     &overrides_provider);
    }
    else if (cli.m_ProjectJson.has_value())
    {
        if (!project.LoadFromJson(cli.m_ProjectJson.value(),
                                  &overrides_provider))
        {
            LogError("Failed loading project from json-blob...");
        }
    }
    else if (!cli.m_ProjectOverrides.empty())
    {
        LogInfo("Starting from a clean project with overrides...");
        const auto default_json{ project.DumpToJson() };
        project.LoadFromJson(default_json, &overrides_provider);
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

    QTimer idle_timer;
    idle_timer.setSingleShot(true);
    idle_timer.setInterval(500);
    QObject::connect(&cropper,
                     &Cropper::CropWorkStart,
                     &idle_timer,
                     &QTimer::stop);

    QCoreApplication app{ argc, argv };
    if (cli.m_WaitForCropper)
    {
        if (cli.m_Render)
        {
            const auto render_and_quit{
                [&]
                {
                    GeneratePdf(project);
                    app.quit();
                }
            };
            QObject::connect(&idle_timer,
                             &QTimer::timeout,
                             &project,
                             render_and_quit);
            QObject::connect(&cropper,
                             &Cropper::CropWorkDone,
                             &project,
                             render_and_quit);
        }
        else
        {
            QObject::connect(&idle_timer,
                             &QTimer::timeout,
                             &app,
                             &QCoreApplication::quit);
            QObject::connect(&cropper,
                             &Cropper::CropWorkDone,
                             &app,
                             &QCoreApplication::quit);
        }
    }

    {
    }

    cropper.Start();
    card_provider.Start();
    idle_timer.start();

    return app.exec();
}
