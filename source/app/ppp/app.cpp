#include <ppp/app.hpp>

#include <ranges>

#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QMainWindow>
#include <QSettings>
#include <QStandardPaths>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <nlohmann/json.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

#include <ppp/ui/main_window.hpp>

PrintProxyPrepApplication::PrintProxyPrepApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    TRACY_AUTO_SCOPE();

    QCoreApplication::setOrganizationName("Proxy");
    QCoreApplication::setApplicationName("Proxy PDF Maker");

    constexpr auto c_EnsureExists{
        [](const fs::path& p)
        {
            if (!fs::exists(p))
            {
                fs::create_directories(p);
            }
        }
    };

    // Create folders for user-content
    for (const auto& folder : { "./res/cubes",
                                "./res/styles",
                                "./res/base_pdfs",
                                "./res/card_svgs",
                                "./res/models" })
    {
        c_EnsureExists(folder);
    }

    c_EnsureExists(GetConfigFolder());
    c_EnsureExists(GetDataFolder());
    c_EnsureExists(GetProjectsFolder());
    c_EnsureExists(GetCacheFolder());

    m_ProjectPath = GetProjectsFolder() / "proj.json";

    MigrateOldStyleSettings();
    Load();
}

PrintProxyPrepApplication::~PrintProxyPrepApplication()
{
    TRACY_AUTO_SCOPE();

    Save();
}

fs::path PrintProxyPrepApplication::GetConfigFolder() const
{
    const auto config_dir{ QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) };
    return QDir{ config_dir }.filesystemPath();
}
fs::path PrintProxyPrepApplication::GetDataFolder() const
{
    const auto data_dir{ QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) };
    return QDir{ data_dir }.filesystemPath();
}
fs::path PrintProxyPrepApplication::GetProjectsFolder() const
{
    const auto data_dir{ QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) };
    return QDir{ data_dir }.filesystemPath() / "Proxy Projects";
}
fs::path PrintProxyPrepApplication::GetCacheFolder() const
{
    const auto cache_dir{ QStandardPaths::writableLocation(QStandardPaths::CacheLocation) };
    return QDir{ cache_dir }.filesystemPath();
}

void PrintProxyPrepApplication::SetMainWindow(PrintProxyPrepMainWindow* main_window)
{
    TRACY_AUTO_SCOPE();

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
PrintProxyPrepMainWindow* PrintProxyPrepApplication::GetMainWindow() const
{
    return m_MainWindow;
}

std::optional<QByteArray> PrintProxyPrepApplication::LoadWindowGeometry(const QString& object_name) const
{
    TRACY_AUTO_SCOPE();

    if (m_WindowGeometries.contains(object_name))
    {
        return m_WindowGeometries.at(object_name);
    }
    return std::nullopt;
}
void PrintProxyPrepApplication::SaveWindowGeometry(const QString& object_name, QByteArray geometry)
{
    TRACY_AUTO_SCOPE();
    m_WindowGeometries[object_name] = std::move(geometry);
}

void PrintProxyPrepApplication::SetProjectName(std::string project_name)
{
    const auto project_ext{ m_ProjectPath.extension() };
    m_ProjectPath = m_ProjectPath.parent_path() / project_name;
    m_ProjectPath += project_ext;
    ProjectPathChanged(m_ProjectPath);
}
void PrintProxyPrepApplication::SetProjectPath(fs::path project_path)
{
    m_ProjectPath = std::move(project_path);
    ProjectPathChanged(m_ProjectPath);
}
const fs::path& PrintProxyPrepApplication::GetProjectPath() const
{
    return m_ProjectPath;
}

void PrintProxyPrepApplication::SetProjectsRoot(fs::path projects_root)
{
    m_ProjectPath = projects_root / m_ProjectPath.filename();
    ProjectPathChanged(m_ProjectPath);
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
    TRACY_AUTO_SCOPE();
    TRACY_SCOPED_LOCK(m_CubesMutex);

    if (!m_Cubes.contains(cube_name))
    {
        m_Cubes[std::move(cube_name)] = std::move(cube);
    }
}
const cv::Mat* PrintProxyPrepApplication::GetCube(const std::string& cube_name) const
{
    TRACY_AUTO_SCOPE();
    TRACY_SCOPED_LOCK(m_CubesMutex);

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

nlohmann::json PrintProxyPrepApplication::GetProjectDefault(std::string_view path) const
{
    TRACY_AUTO_SCOPE();
    return GetJsonValue(path);
}
void PrintProxyPrepApplication::SetProjectDefault(std::string_view path, nlohmann::json value)
{
    TRACY_AUTO_SCOPE();
    SetJsonValue(path, std::move(value));
}

nlohmann::json PrintProxyPrepApplication::GetJsonValue(std::string_view path) const
{
    if (m_DefaultProjectData == nullptr)
    {
        return nlohmann::json{};
    }

    TRACY_AUTO_SCOPE();

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
    TRACY_AUTO_SCOPE();

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
    TRACY_AUTO_SCOPE();

    const auto ini_path{ ToQString(GetConfigFolder() / "state.ini") };
    QSettings settings{ ini_path, QSettings::IniFormat };
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
    TRACY_AUTO_SCOPE();

    const auto ini_path{ ToQString(GetConfigFolder() / "state.ini") };
    QSettings settings{ ini_path, QSettings::IniFormat };
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

void PrintProxyPrepApplication::MigrateOldStyleSettings()
{
    const auto ini_path{ ToQString(GetConfigFolder() / "state.ini") };
    QSettings ini_settings{ ini_path, QSettings::IniFormat };
    QSettings native_settings{ "Proxy", "Proxy PDF Maker" };

    if (!native_settings.allKeys().isEmpty() && ini_settings.allKeys().isEmpty())
    {
        LogInfo("Migrating old native app state to new ini app state...");

        for (const auto& key : native_settings.allKeys())
        {
            const auto value{ native_settings.value(key) };
            ini_settings.setValue(key, value);
        }

        ini_settings.sync();

        native_settings.clear();
        native_settings.sync();

        LogInfo("Migration completed...");
    }
}
