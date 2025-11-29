#pragma once

#include <mutex>

#include <QApplication>

#include <opencv2/core/mat.hpp>

#include <nlohmann/json_fwd.hpp>

#include <ppp/constants.hpp>
#include <ppp/json_util.hpp>
#include <ppp/util.hpp>

class QMainWindow;

class PrintProxyPrepApplication
    : public QApplication,
      public JsonProvider
{
  public:
    PrintProxyPrepApplication(int& argc, char** argv);
    ~PrintProxyPrepApplication();

    void SetMainWindow(QMainWindow* main_window);
    QMainWindow* GetMainWindow() const;

    void SetProjectPath(fs::path project_path);
    const fs::path& GetProjectPath() const;

    void SetTheme(std::string theme);
    const std::string& GetTheme() const;

    void SetCube(std::string cube_name, cv::Mat cube);
    const cv::Mat* GetCube(const std::string& cube_name) const;

    bool GetObjectVisibility(const QString& object_name) const;
    void SetObjectVisibility(const QString& object_name, bool visible);

    nlohmann::json GetProjectDefault(std::string_view path) const;
    void SetProjectDefault(std::string_view path, nlohmann::json value);

    virtual nlohmann::json GetJsonValue(std::string_view path) const override;
    virtual void SetJsonValue(std::string_view path, nlohmann::json value) override;

  private:
    bool notify(QObject*, QEvent*) override;

    void Load();
    void Save() const;

    QMainWindow* m_MainWindow{ nullptr };

    fs::path m_ProjectPath{ cwd() / "proj.json" };
    std::string m_Theme{ "Default" };

    mutable std::mutex m_CubesMutex;
    std::unordered_map<std::string, cv::Mat> m_Cubes;

    std::unordered_map<QString, bool> m_ObjectVisibilities;

    std::unique_ptr<nlohmann::json> m_DefaultProjectData;

    std::optional<QByteArray> m_WindowGeometry{};
    std::optional<QByteArray> m_WindowState{};
};
