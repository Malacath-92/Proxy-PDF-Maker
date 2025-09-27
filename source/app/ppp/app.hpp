#pragma once

#include <mutex>

#include <QApplication>

#include <opencv2/core/mat.hpp>

#include <ppp/constants.hpp>
#include <ppp/util.hpp>

class QMainWindow;

namespace Ort
{
struct Session;
}

class PrintProxyPrepApplication : public QApplication
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

    void SetUpscaleModel(std::string model_name, std::unique_ptr<Ort::Session> model);
    std::unique_ptr<Ort::Session> ReleaseUpscaleModel(std::string model_name);
    Ort::Session* GetUpscaleModel(const std::string& model_name) const;

    bool GetObjectVisibility(const QString& object_name) const;
    void SetObjectVisibility(const QString& object_name, bool visible);

  private:
    bool notify(QObject*, QEvent*) override;

    void Load();
    void Save() const;

    QMainWindow* m_MainWindow{ nullptr };

    fs::path m_ProjectPath{ cwd() / "proj.json" };
    std::string m_Theme{ "Default" };

    mutable std::mutex m_CubesMutex;
    std::unordered_map<std::string, cv::Mat> m_Cubes;

    mutable std::mutex m_ModelsMutex;
    std::unordered_map<std::string, std::unique_ptr<Ort::Session>> m_Models;

    std::unordered_map<QString, bool> m_ObjectVisibilities;

    std::optional<QByteArray> m_WindowGeometry{};
    std::optional<QByteArray> m_WindowState{};
};
