#include <ppp/app.hpp>

#include <ranges>

#include <QFile>
#include <QKeyEvent>
#include <QMainWindow>
#include <QSettings>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <ppp/qt_util.hpp>
#include <ppp/version.hpp>

#include <ppp/ui/main_window.hpp>

PrintProxyPrepApplication::PrintProxyPrepApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // Create folders for user-content
    for (const auto& folder : { "res/cubes", "res/styles", "res/base_pdfs", "res/models" })
    {
        if (!fs::exists(folder))
        {
            fs::create_directories(folder);
        }
    }

    Load();
}

PrintProxyPrepApplication::~PrintProxyPrepApplication()
{
    Save();
}

void PrintProxyPrepApplication::SetMainWindow(QMainWindow* main_window)
{
    m_MainWindow = main_window;

    if (m_WindowGeometry.has_value())
    {
        m_MainWindow->restoreGeometry(m_WindowGeometry.value());
        m_WindowGeometry.reset();
    }

    if (m_WindowState.has_value())
    {
        m_MainWindow->restoreState(m_WindowState.value());
        m_WindowState.reset();
    }
}
QMainWindow* PrintProxyPrepApplication::GetMainWindow() const
{
    return m_MainWindow;
}

void PrintProxyPrepApplication::SetProjectPath(fs::path project_path)
{
    m_ProjectPath = std::move(project_path);
}
const fs::path& PrintProxyPrepApplication::GetProjectPath() const
{
    return m_ProjectPath;
}

void PrintProxyPrepApplication::SetTheme(std::string theme)
{
    m_Theme = std::move(theme);
}
const std::string& PrintProxyPrepApplication::GetTheme() const
{
    return m_Theme;
}

void PrintProxyPrepApplication::SetCube(std::string cube_name, cv::Mat cube)
{
    std::lock_guard lock{ m_CubesMutex };
    if (!m_Cubes.contains(cube_name))
    {
        m_Cubes[std::move(cube_name)] = std::move(cube);
    }
}
const cv::Mat* PrintProxyPrepApplication::GetCube(const std::string& cube_name) const
{
    std::lock_guard lock{ m_CubesMutex };
    if (m_Cubes.contains(cube_name))
    {
        return &m_Cubes.at(cube_name);
    }
    return nullptr;
}

void PrintProxyPrepApplication::SetUpscaleModel(std::string model_name, std::unique_ptr<Ort::Session> model)
{
    std::lock_guard lock{ m_ModelsMutex };
    if (!m_Models.contains(model_name))
    {
        m_Models[std::move(model_name)] = std::move(model);
    }
}
std::unique_ptr<Ort::Session> PrintProxyPrepApplication::ReleaseUpscaleModel(std::string model_name)
{
    std::lock_guard lock{ m_ModelsMutex };
    if (!m_Models.contains(model_name))
    {
        auto model{ std::move(m_Models[model_name]) };
        m_Models.erase(model_name);
        return model;
    }
    return nullptr;
}
Ort::Session* PrintProxyPrepApplication::GetUpscaleModel(const std::string& model_name) const
{
    std::lock_guard lock{ m_ModelsMutex };
    if (m_Models.contains(model_name))
    {
        return m_Models.at(model_name).get();
    }
    return nullptr;
}

bool PrintProxyPrepApplication::GetObjectVisibility(const QString& object_name) const
{
    if (m_ObjectVisibilities.contains(object_name))
    {
        return m_ObjectVisibilities.at(object_name);
    }
    return true;
}
void PrintProxyPrepApplication::SetObjectVisibility(const QString& object_name, bool visible)
{
    m_ObjectVisibilities[object_name] = visible;
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
                static_cast<PrintProxyPrepMainWindow*>(m_MainWindow)->OpenAboutPopup();
            }
            return true;
        }
    }
    return QApplication::notify(object, event);
}

void PrintProxyPrepApplication::Load()
{
    QSettings settings{ "Proxy", "Proxy PDF Maker" };
    if (settings.contains("version"))
    {
        m_WindowGeometry.emplace() = settings.value("geometry").toByteArray();
        m_WindowState.emplace() = settings.value("state").toByteArray();
        m_ProjectPath = settings.value("json").toString().toStdString();
        m_Theme = settings.value("theme", "Default").toString().toStdString();

        if (settings.childGroups().contains("ObjectVisibility", Qt::CaseInsensitive))
        {
            settings.beginGroup("ObjectVisibility");
            for (const auto& key : settings.allKeys())
            {
                m_ObjectVisibilities[key] = settings.value(key).toBool();
            }
            settings.endGroup();
        }
        else
        {
            m_ObjectVisibilities = {
                { "Guides Options", false },
                { "Global Config", false },
            };
        }
    }
}
void PrintProxyPrepApplication::Save() const
{
    QSettings settings{ "Proxy", "Proxy PDF Maker" };
    settings.setValue("version", ToQString(ProxyPdfVersion()));
    settings.setValue("geometry", m_MainWindow->saveGeometry());
    settings.setValue("state", m_MainWindow->saveState());
    settings.setValue("json", ToQString(m_ProjectPath));
    settings.setValue("theme", ToQString(m_Theme));

    {
        settings.beginGroup("ObjectVisibility");
        for (const auto& [object_name, visible] : m_ObjectVisibilities)
        {
            settings.setValue(object_name, visible);
        }
        settings.endGroup();
    }
}
