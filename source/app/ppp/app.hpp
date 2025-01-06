#pragma once

#include <QApplication>

#include <opencv2/core/mat.hpp>

#include <ppp/constants.hpp>
#include <ppp/util.hpp>

class QMainWindow;

class PrintProxyPrepApplication : public QApplication
{
  public:
    PrintProxyPrepApplication();
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
    void Load();
    void Save() const;

    QMainWindow* MainWindow{ nullptr };

    fs::path ProjectPath{ cwd() / "proj.json" };
    std::string Theme{ "Default" };

    std::unordered_map<std::string, cv::Mat> Cubes;

    std::optional<QByteArray> WindowGeometry{};
    std::optional<QByteArray> WindowState{};
};
