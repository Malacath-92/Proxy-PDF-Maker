#pragma once

#include <mutex>
#include <optional>

#include <QApplication>

#include <opencv2/core/mat.hpp>

#include <nlohmann/json_fwd.hpp>

#include <ppp/constants.hpp>
#include <ppp/json_util.hpp>
#include <ppp/util.hpp>

#include <ppp/profile/profile.hpp>

class PrintProxyPrepMainWindow;

namespace Ort
{
struct Session;
}

class PrintProxyPrepApplication
    : public QApplication,
      public JsonProvider
{
    Q_OBJECT

  public:
    PrintProxyPrepApplication(int& argc, char** argv);
    ~PrintProxyPrepApplication();

    bool IsFirstStartup() const;
    void LoadState();

    fs::path GetConfigFolder() const;
    fs::path GetDataFolder() const;
    fs::path GetProjectsFolder() const;
    fs::path GetCacheFolder() const;

    fs::path GetCubesFolder() const;
    fs::path GetStylesFolder() const;
    fs::path GetBasePdfsFolder() const;
    fs::path GetCardSvgsFolder() const;
    fs::path GetUpscaleModelsFolder() const;

    void SetMainWindow(PrintProxyPrepMainWindow* main_window);
    PrintProxyPrepMainWindow* GetMainWindow() const;

    std::optional<QByteArray> LoadWindowGeometry(const QString& object_name) const;
    void SaveWindowGeometry(const QString& object_name, QByteArray geometry);

    void SetProjectName(std::string project_name);
    void SetProjectPath(fs::path project_path);
    const fs::path& GetProjectPath() const;

    void SetProjectsRoot(fs::path projects_root);

    void SetTheme(std::string theme);
    const std::string& GetTheme() const;

    void SetCube(std::string cube_name, cv::Mat cube);
    const cv::Mat* GetCube(const std::string& cube_name) const;

    void SetUpscaleModel(std::string model_name, std::unique_ptr<Ort::Session> model);
    std::unique_ptr<Ort::Session> ReleaseUpscaleModel(std::string model_name);
    Ort::Session* GetUpscaleModel(const std::string& model_name) const;

    bool GetObjectVisibility(const QString& object_name) const;
    void SetObjectVisibility(const QString& object_name, bool visible);

    nlohmann::json GetProjectDefault(std::string_view path) const;
    void SetProjectDefault(std::string_view path, nlohmann::json value);

    virtual nlohmann::json GetJsonValue(std::string_view path) const override;
    virtual void SetJsonValue(std::string_view path, nlohmann::json value) override;

  signals:
    void ProjectPathChanged(const fs::path& project_path);

  private:
    bool notify(QObject*, QEvent*) override;

    void Load();
    void Save() const;

    void MigrateOldStyleSettings();

    bool m_IsFirstStartup{ false };

    PrintProxyPrepMainWindow* m_MainWindow{ nullptr };
    std::unordered_map<QString, QByteArray> m_WindowGeometries;

    fs::path m_ProjectPath;
    std::string m_Theme{ "Default" };

    mutable TRACY_DECLARE_MUTEX(std::mutex, m_CubesMutex);
    std::unordered_map<std::string, cv::Mat> m_Cubes;

    mutable std::mutex m_ModelsMutex;
    std::unordered_map<std::string, std::unique_ptr<Ort::Session>> m_Models;

    std::unordered_map<QString, bool> m_ObjectVisibilities;

    std::unique_ptr<nlohmann::json> m_DefaultProjectData;

    std::optional<QByteArray> m_WindowGeometry{};
    std::optional<QByteArray> m_WindowState{};
};

#define ppApp static_cast<PrintProxyPrepApplication*>(qApp)
