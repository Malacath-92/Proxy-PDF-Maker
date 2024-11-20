#include <ppp/app.hpp>

#include <QMainWindow>
#include <QSettings>

#include <ppp/project/image_ops.hpp>
#include <ppp/version.hpp>

PrintProxyPrepApplication::PrintProxyPrepApplication()
    : QApplication(__argc, __argv)
{
    InitImageSystem();
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

void PrintProxyPrepApplication::Load()
{
    QSettings settings{ "Proxy", "PDF Proxy Printer" };
    if (settings.contains("version"))
    {
        WindowGeometry.emplace() = settings.value("geometry").toByteArray();
        WindowState.emplace() = settings.value("state").toByteArray();
        ProjectPath = settings.value("json").toString().toStdString();
    }
}
void PrintProxyPrepApplication::Save() const
{
    QSettings settings{ "Proxy", "PDF Proxy Printer" };
    settings.setValue("version", QString::fromLatin1(ProxyPdfVersion()));
    settings.setValue("geometry", MainWindow->saveGeometry());
    settings.setValue("state", MainWindow->saveState());
    settings.setValue("json", QString::fromWCharArray(ProjectPath.c_str()));
}
