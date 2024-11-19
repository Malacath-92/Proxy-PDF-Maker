#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include <QDialog>

#include <ppp/util.hpp>

class QLabel;

enum class FileDialogType
{
    Open,
    Save,
};

std::optional<fs::path> OpenFolderDialog(const fs::path& root);

std::optional<fs::path> OpenFileDialog(std::string_view title, const fs::path& root, std::string_view filter, FileDialogType type);
std::optional<fs::path> OpenImageDialog(const fs::path& root);
std::optional<fs::path> OpenProjectDialog(FileDialogType type);

class GenericPopup : public QDialog
{
    Q_OBJECT;

  public:
    GenericPopup(QWidget* parent, std::string_view text);

    void ShowDuringWork(std::function<void()> work);

    std::function<void(std::string_view)> MakePrintFn();

    virtual void showEvent(QShowEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    void UpdateText(std::string_view text);

  private slots:
    void UpdateTextImpl(std::string_view text);

  private:
    void Recenter();

    QLabel* TextLabel;
    std::shared_ptr<void> WorkerThread;
    std::function<void(std::string_view)> Refresh;
};
