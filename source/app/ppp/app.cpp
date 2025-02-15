#include <ppp/app.hpp>

#include <ranges>

#include <QFile>
#include <QKeyEvent>
#include <QMainWindow>
#include <QSettings>

#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

#include <ppp/ui/main_window.hpp>

PrintProxyPrepApplication::PrintProxyPrepApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    Load();
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

void PrintProxyPrepApplication::SetCube(std::string cube_name, cv::Mat cube)
{
    if (!Cubes.contains(cube_name))
    {
        Cubes[std::move(cube_name)] = std::move(cube);
    }
}
const cv::Mat* PrintProxyPrepApplication::GetCube(const std::string& cube_name) const
{
    if (Cubes.contains(cube_name))
    {
        return &Cubes.at(cube_name);
    }
    return nullptr;
}

bool PrintProxyPrepApplication::notify(QObject* object, QEvent* event)
{
    if (auto* key_event{ dynamic_cast<QKeyEvent*>(event) })
    {
        // Catch all F1 events ...
        if (key_event->key() == Qt::Key::Key_F1)
        {
            // ... but only open the About window on the release
            if (key_event->type() == QEvent::Type::KeyRelease)
            {
                static_cast<PrintProxyPrepMainWindow*>(MainWindow)->OpenAboutPopup();
            }
            return true;
        }
    }
    return QApplication::notify(object, event);
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
    settings.setValue("version", ToQString(ProxyPdfVersion()));
    settings.setValue("geometry", MainWindow->saveGeometry());
    settings.setValue("state", MainWindow->saveState());
    settings.setValue("json", ToQString(ProjectPath));
    settings.setValue("theme", ToQString(Theme));
}
