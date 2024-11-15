#pragma once

#include <QApplication>

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

  private:
    void Load();
    void Save() const;

    QMainWindow* MainWindow{ nullptr };

    fs::path ProjectPath{ cwd() / "print.json" };

    std::optional<QByteArray> WindowGeometry{};
    std::optional<QByteArray> WindowState{};
};
