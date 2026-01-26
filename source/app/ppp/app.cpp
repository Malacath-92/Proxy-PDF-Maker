#include <ppp/app.hpp>

#include <ranges>

#include <QFile>
#include <QKeyEvent>
#include <QMainWindow>
#include <QSettings>

#include <nlohmann/json.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

#include <ppp/ui/main_window.hpp>

PrintProxyPrepApplication::PrintProxyPrepApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // Create folders for user-content
    for (const auto& folder : { "res/cubes", "res/styles", "res/base_pdfs" })
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

std::optional<QByteArray> PrintProxyPrepApplication::LoadWindowGeometry(const QString& object_name) const
{
    if (m_WindowGeometries.contains(object_name))
    {
        return m_WindowGeometries.at(object_name);
    }
    return std::nullopt;
}
void PrintProxyPrepApplication::SaveWindowGeometry(const QString& object_name, QByteArray geometry)
{
    m_WindowGeometries[object_name] = std::move(geometry);
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

nlohmann::json PrintProxyPrepApplication::GetProjectDefault(std::string_view path) const
{
    return GetJsonValue(path);
}
void PrintProxyPrepApplication::SetProjectDefault(std::string_view path, nlohmann::json value)
{
    SetJsonValue(path, std::move(value));
}

nlohmann::json PrintProxyPrepApplication::GetJsonValue(std::string_view path) const
{
    if (m_DefaultProjectData == nullptr)
    {
        return nlohmann::json{};
    }

    try
    {
        return ::GetJsonValue(*m_DefaultProjectData, path);
    }
    catch (...)
    {
        return nlohmann::json{};
    }
}
void PrintProxyPrepApplication::SetJsonValue(std::string_view path, nlohmann::json value)
{
    if (m_DefaultProjectData == nullptr)
    {
        m_DefaultProjectData = std::make_unique<nlohmann::json>(nlohmann::json::value_t::object);
    }

    try
    {
        ::SetJsonValue(*m_DefaultProjectData, path, std::move(value));
    }
    catch (...)
    {
        LogError("Invalid path {} when setting default project value.", path);
    }
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

        if (settings.childGroups().contains("Windows", Qt::CaseInsensitive))
        {
            settings.beginGroup("Windows");
            for (const auto& key : settings.allKeys())
            {
                m_WindowGeometries[key] = settings.value(key).toByteArray();
            }
            settings.endGroup();
        }

        if (settings.contains("project_defaults"))
        {
            try
            {
                const auto json_blob{ settings.value("project_defaults").toString().toStdString() };
                m_DefaultProjectData = std::make_unique<nlohmann::json>(nlohmann::json::parse(json_blob));
            }
            catch (const std::exception& e)
            {
                LogError("Failed loading project defaults, continuing with original defaults: {}", e.what());

                // Shouldn't be set, but better safe than sorry'
                m_DefaultProjectData.reset();
            }
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

    if (!m_WindowGeometries.empty())
    {
        settings.beginGroup("Windows");
        for (const auto& [window_name, geometry] : m_WindowGeometries)
        {
            settings.setValue(window_name, geometry);
        }
        settings.endGroup();
    }

    if (m_DefaultProjectData != nullptr)
    {
        try
        {
            const auto json_blob{ m_DefaultProjectData->dump() };
            settings.setValue("project_defaults", ToQString(json_blob));
        }
        catch (const std::exception& e)
        {
            LogError("Failed wring project defaults, they will be rest on next load: {}", e.what());
        }
    }
}
