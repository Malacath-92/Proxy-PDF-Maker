#include <ppp/ui/popups.hpp>

#include <ranges>

#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include <fmt/ranges.h>

#include <ppp/project/image_ops.hpp>

#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/version.hpp>

std::optional<fs::path> OpenFolderDialog(const fs::path& root)
{
    const QString choice{
        QFileDialog::getExistingDirectory(
            nullptr,
            "Choose Folder",
            ToQString(root),
            QFileDialog::Option::ShowDirsOnly | QFileDialog::Option::DontResolveSymlinks)
    };

    if (choice.isEmpty())
    {
        return std::nullopt;
    }
    else
    {
        return fs::path{ choice.toStdString() }.filename();
    }
}

std::optional<fs::path> OpenFileDialog(std::string_view title, const fs::path& root, std::string_view filter, FileDialogType type)
{
    QString choice{};
    if (type == FileDialogType::Open)
    {
        choice = QFileDialog::getOpenFileName(
            nullptr,
            ToQString(title),
            ToQString(root),
            ToQString(filter));
    }
    else
    {
        choice = QFileDialog::getSaveFileName(
            nullptr,
            ToQString(title),
            ToQString(root),
            ToQString(filter));
    }

    if (choice.isEmpty())
    {
        return std::nullopt;
    }
    else
    {
        return fs::path{ choice.toStdString() }.filename();
    }
}

std::optional<fs::path> OpenImageDialog(const fs::path& root)
{
    std::string image_filters_str{ "Images (*" + ValidImageExtensions[0].string() };
    for (const fs::path& valid_extension : ValidImageExtensions | std::views::drop(1))
    {
        image_filters_str.append(" *");
        image_filters_str.append(valid_extension.string());
    }
    image_filters_str.append(")");

    return OpenFileDialog(
        "Open Image",
        root,
        image_filters_str,
        FileDialogType::Open);
}

std::optional<fs::path> OpenProjectDialog(FileDialogType type)
{
    return OpenFileDialog("Open Project", ".", "Json Files (*.json)", type);
}

PopupBase::PopupBase(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::WindowType::FramelessWindowHint |
                   Qt::WindowType::WindowStaysOnTopHint);

    QPalette old_palette{ palette() };
    old_palette.setColor(backgroundRole(), 0x111111);
    setPalette(old_palette);
    setAutoFillBackground(true);
}

void PopupBase::Show()
{
    open();
    exec();
}

void PopupBase::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    Recenter();
}

void PopupBase::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    // Three times???
    Recenter();
    Recenter();
    Recenter();
}

void PopupBase::Recenter()
{
    const QWidget* parent{ parentWidget() };
    const auto parent_rect{ parent != nullptr
                                ? parent->rect()
                                : QApplication::primaryScreen()->geometry() };
    const auto parent_half_size{ parent_rect.size() / 2 };
    const auto center{ rect().center() };
    const auto offset{ QPoint(parent_half_size.width(), parent_half_size.height()) - center };
    move(offset);
}

class WorkThread : public QThread
{
    Q_OBJECT

  public:
    WorkThread(std::function<void()> work)
        : Work{ std::move(work) }
    {
    }
    virtual void run() override
    {
        Work();
    }

  signals:
    void Refresh(std::string text);

  private:
    std::function<void()> Work;
};

GenericPopup::GenericPopup(QWidget* parent, std::string_view text)
    : PopupBase(parent)
{
    auto* text_widget{ new QLabel{ ToQString(text) } };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(text_widget);
    setLayout(layout);

    TextLabel = text_widget;

    UpdateTextImpl(text);
}

void GenericPopup::ShowDuringWork(std::function<void()> work)
{
    auto* work_thread{ new WorkThread{ std::move(work) } };
    work_thread->setObjectName("Work Thread");

    QObject::connect(work_thread,
                     &QThread::finished,
                     this,
                     &QDialog::close);
    QObject::connect(work_thread,
                     &WorkThread::Refresh,
                     this,
                     &GenericPopup::UpdateTextImpl);
    work_thread->start();

    Refresh = [work_thread](std::string_view text)
    {
        work_thread->Refresh(std::string{ text });
    };
    WorkerThread.reset(work_thread);

    PopupBase::Show();

    work_thread->quit();
    work_thread->wait();
    WorkerThread.reset();
    Refresh = nullptr;
}

GenericPopup::UninstallLogHookAtScopeExit GenericPopup::InstallLogHook()
{
    if (LogHookId.has_value())
    {
        UninstallLogHook();
    }

    LogHookId = Log::GetInstance(Log::m_MainLogName)->InstallHook([this](const Log::DetailInformation&, Log::LogLevel, std::string_view message)
                                                                  { UpdateText(message); });

    return UninstallLogHookAtScopeExit{ this };
}

void GenericPopup::UninstallLogHook()
{
    if (LogHookId.has_value())
    {
        Log::GetInstance(Log::m_MainLogName)->UninstallHook(LogHookId.value());
        LogHookId.reset();
    }
}

void GenericPopup::Sleep(dla::time_unit duration)
{
    const auto thread{ static_pointer_cast<WorkThread>(WorkerThread) };
    if (thread.get() == QThread::currentThread())
    {
        QThread::sleep(static_cast<unsigned long>(duration / 1_s));
    }
    else
    {
        thread->wait(static_cast<unsigned long>(duration / 0.001_s));
    }
}

void GenericPopup::UpdateText(std::string_view text)
{
    Refresh(text);
}

void GenericPopup::UpdateTextImpl(std::string_view text)
{
    adjustSize();
    TextLabel->setText(ToQString(text));
    if (text.starts_with("Failure"))
    {
        TextLabel->setStyleSheet("border: 3px solid red;");
    }
    else
    {
        TextLabel->setStyleSheet("border: 0px solid red;");
    }
    adjustSize();
    Recenter();
}

AboutPopup::AboutPopup(QWidget* parent)
    : PopupBase(parent)
{
    auto* app_text{ new QLabel{ ToQString("Proxy-PDF-Maker") } };
    auto* version_text{ new QLabel{ ToQString(fmt::format("Version: {}", ProxyPdfVersion())) } };
    auto* built_text{ new QLabel{ ToQString(fmt::format("Built: {}", ProxyPdfBuildTime())) } };
    auto* license_text{ new QLabel{ ToQString("Released under MIT License") } };

    auto* buttons{ new QWidget{} };
    {
        auto* close_button{ new QPushButton{ "Close" } };
        auto* issue_button{ new QPushButton{ "Report Issue" } };

        auto* layout{ new QHBoxLayout };
        layout->addWidget(close_button);
        layout->addWidget(issue_button);
        buttons->setLayout(layout);

        auto open_issues_page{
            [this]()
            {
                QDesktopServices::openUrl(ToQString("https://github.com/Malacath-92/Proxy-PDF-Maker/issues"));
                close();
            }
        };

        QObject::connect(close_button,
                         &QPushButton::clicked,
                         this,
                         &AboutPopup::close);
        QObject::connect(issue_button,
                         &QPushButton::clicked,
                         this,
                         open_issues_page);
    }

    auto* layout{ new QVBoxLayout };
    layout->addWidget(app_text);
    layout->addWidget(version_text);
    layout->addWidget(built_text);
    layout->addWidget(license_text);
    layout->addWidget(buttons);
    setLayout(layout);
}

void AboutPopup::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key::Key_Escape)
    {
        close();
    }
}

#include "popups.moc"
