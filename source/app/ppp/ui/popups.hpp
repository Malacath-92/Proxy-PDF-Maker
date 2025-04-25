#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include <QDialog>

#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

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

class PopupBase : public QDialog
{
    Q_OBJECT

  public:
    PopupBase(QWidget* parent);

    void Show();

    virtual void showEvent(QShowEvent* event) override;

    virtual void resizeEvent(QResizeEvent* event) override;

  protected:
    void Recenter();
};

class GenericPopup : public PopupBase
{
    Q_OBJECT

  public:
    GenericPopup(QWidget* parent, std::string_view text);

    void ShowDuringWork(std::function<void()> work);

    struct UninstallLogHookAtScopeExit
    {
        ~UninstallLogHookAtScopeExit()
        {
            self->UninstallLogHook();
        }
        GenericPopup* self;
    };
    UninstallLogHookAtScopeExit InstallLogHook();
    void UninstallLogHook();

    void Sleep(dla::time_unit duration);

  private:
    void UpdateText(std::string_view text);

  private slots:
    void UpdateTextImpl(std::string_view text);

  private:
    QLabel* TextLabel;
    std::shared_ptr<void> WorkerThread;
    std::function<void(std::string_view)> Refresh;
    std::optional<uint32_t> LogHookId;
};

class AboutPopup : public PopupBase
{
    Q_OBJECT

  public:
    AboutPopup(QWidget* parent);

    void keyReleaseEvent(QKeyEvent* event) override;
};
