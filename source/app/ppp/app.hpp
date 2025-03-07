#pragma once

#include <QApplication>

#include <ppp/constants.hpp>
#include <ppp/util.hpp>

class QMainWindow;

class Image;
using ColorCube = std::vector<Image>;

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

    void SetCube(std::string cube_name, ColorCube cube);
    const ColorCube* GetCube(const std::string& cube_name) const;

  private:
    bool notify(QObject*, QEvent*) override;

    void Load();
    void Save() const;

    QMainWindow* MainWindow{ nullptr };

    fs::path ProjectPath{ cwd() / "proj.json" };
    std::string Theme{ "Default" };

    std::unordered_map<std::string, ColorCube> Cubes;

    std::optional<QByteArray> WindowGeometry{};
    std::optional<QByteArray> WindowState{};
};
