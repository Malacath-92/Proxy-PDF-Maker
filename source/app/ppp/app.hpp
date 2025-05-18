#pragma once

#include <mutex>

#include <QApplication>

#include <opencv2/core/mat.hpp>

#include <ppp/constants.hpp>
#include <ppp/util.hpp>

class QMainWindow;

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

  private:
    bool notify(QObject*, QEvent*) override;

    void Load();
    void Save() const;

    QMainWindow* m_MainWindow{ nullptr };

    fs::path m_ProjectPath{ cwd() / "proj.json" };
    std::string m_Theme{ "Default" };

    mutable std::mutex m_CubesMutex;
    std::unordered_map<std::string, cv::Mat> m_Cubes;

    std::optional<QByteArray> m_WindowGeometry{};
    std::optional<QByteArray> m_WindowState{};
};
