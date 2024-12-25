#include <ppp/app.hpp>

#include <ranges>

#include <QFile>
#include <QMainWindow>
#include <QSettings>

#include <ppp/project/image_ops.hpp>
#include <ppp/version.hpp>

PrintProxyPrepApplication::PrintProxyPrepApplication()
    : QApplication(__argc, __argv)
{
    Load();

    VibranceCube = []()
    {
        Q_INIT_RESOURCE(resources);

        QFile vibrance_cube_file{ ":/res/vibrance.CUBE" };
        vibrance_cube_file.open(QFile::ReadOnly);
        const std::string vibrance_cube_raw{ QLatin1String{ vibrance_cube_file.readAll() }.toString().toStdString() };

        static constexpr auto to_string_views{ std::views::transform(
            [](auto str)
            { return std::string_view(str.data(), str.size()); }) };
        static constexpr auto to_float{ std::views::transform(
            [](std::string_view str)
            {
                float val;
                std::from_chars(str.data(), str.data() + str.size(), val);
                return val;
            }) };
        static constexpr auto float_color_to_byte{ std::views::transform(
            [](float val)
            { return static_cast<uint8_t>(val * 255); }) };
        static constexpr auto to_color{ std::views::transform(
            [](std::string_view str)
            {
                return std::views::split(str, ' ') |
                       to_string_views |
                       to_float |
                       float_color_to_byte |
                       std::ranges::to<std::vector>();
            }) };
        const std::vector vibrance_cube_data{
            std::views::split(vibrance_cube_raw, '\n') |
            std::views::drop(11) |
            to_string_views |
            to_color |
            std::views::join |
            std::ranges::to<std::vector>()
        };

        cv::Mat vibrance_cube;
        vibrance_cube.create(std::vector<int>{ 16, 16, 16 }, CV_8UC3);
        vibrance_cube.cols = 16;
        vibrance_cube.rows = 16;
        assert(vibrance_cube.dataend - vibrance_cube.datastart == 16 * 16 * 16 * 3);
        memcpy(vibrance_cube.data, vibrance_cube_data.data(), 16 * 16 * 16 * 3);

        return vibrance_cube;
    }();
}

PrintProxyPrepApplication::~PrintProxyPrepApplication()
{
    Save();
}

void PrintProxyPrepApplication::SetMainWindow(QMainWindow* main_window)
{
    MainWindow = main_window;

    if (WindowGeometry.has_value())
    {
        MainWindow->restoreGeometry(WindowGeometry.value());
        WindowGeometry.reset();
    }

    if (WindowState.has_value())
    {
        MainWindow->restoreState(WindowState.value());
        WindowState.reset();
    }
}
QMainWindow* PrintProxyPrepApplication::GetMainWindow() const
{
    return MainWindow;
}

void PrintProxyPrepApplication::SetProjectPath(fs::path project_path)
{
    ProjectPath = std::move(project_path);
}
const fs::path& PrintProxyPrepApplication::GetProjectPath() const
{
    return ProjectPath;
}

void PrintProxyPrepApplication::SetTheme(std::string theme)
{
    Theme = std::move(theme);
}
const std::string& PrintProxyPrepApplication::GetTheme() const
{
    return Theme;
}

const cv::Mat* PrintProxyPrepApplication::GetVibranceCube() const
{
    return CFG.VibranceBump ? &VibranceCube : nullptr;
}

void PrintProxyPrepApplication::Load()
{
    QSettings settings{ "Proxy", "PDF Proxy Printer" };
    if (settings.contains("version"))
    {
        WindowGeometry.emplace() = settings.value("geometry").toByteArray();
        WindowState.emplace() = settings.value("state").toByteArray();
        ProjectPath = settings.value("json").toString().toStdString();
        Theme = settings.value("theme", "Default").toString().toStdString();
    }
}
void PrintProxyPrepApplication::Save() const
{
    QSettings settings{ "Proxy", "PDF Proxy Printer" };
    settings.setValue("version", QString::fromLatin1(ProxyPdfVersion()));
    settings.setValue("geometry", MainWindow->saveGeometry());
    settings.setValue("state", MainWindow->saveState());
    settings.setValue("json", QString::fromWCharArray(ProjectPath.c_str()));
    settings.setValue("theme", QString::fromStdString(Theme));
}
