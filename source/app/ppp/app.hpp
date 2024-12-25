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

    const cv::Mat* GetVibranceCube() const;

  private:
    void Load();
    void Save() const;

    QMainWindow* MainWindow{ nullptr };

    fs::path ProjectPath{ cwd() / "print.json" };
    std::string Theme{ "Default" };

    cv::Mat VibranceCube;

    std::optional<QByteArray> WindowGeometry{};
    std::optional<QByteArray> WindowState{};
};
