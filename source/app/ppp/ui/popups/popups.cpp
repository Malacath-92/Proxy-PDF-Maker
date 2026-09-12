#include <ppp/ui/popups/popups.hpp>

#include <ranges>

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QThreadPool>
#include <QVBoxLayout>

#include <fmt/ranges.h>

#include <ppp/project/image_ops.hpp>

#include <ppp/app.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>
#include <ppp/util/zip.hpp>
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
        return fs::path{ choice.toStdString() };
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
    std::string image_filters_str{ "Images (*" + g_ValidImageExtensions[0].string() };
    for (const fs::path& valid_extension : g_ValidImageExtensions | std::views::drop(1))
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
}

void PopupBase::Show()
{
    open();
    exec();
}

void PopupBase::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    if (m_AutoCenterOnShow)
    {
        Recenter();
    }

    if (m_PersistGeometry)
    {
        auto* app{ ppApp };
        if (auto geometry{ app->LoadWindowGeometry(objectName()) })
        {
            RestoreGeometry(std::move(geometry).value());
        }
    }
}

void PopupBase::closeEvent(QCloseEvent* event)
{
    if (m_PersistGeometry)
    {
        auto* app{ ppApp };
        app->SaveWindowGeometry(objectName(), GetGeometry());
    }

    QDialog::closeEvent(event);
}

void PopupBase::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    if (m_AutoCenter)
    {
        // Three times???
        Recenter();
        Recenter();
        Recenter();
    }
}

PrintProxyPrepMainWindow* PopupBase::GetMainWindow() const
{
    return ppApp->GetMainWindow();
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

QByteArray PopupBase::GetGeometry()
{
    return saveGeometry();
}

void PopupBase::RestoreGeometry(const QByteArray& geometry)
{
    restoreGeometry(geometry);
}

class WorkThread : public QThread
{
    Q_OBJECT

  public:
    WorkThread(std::function<void()> work)
        : m_Work{ std::move(work) }
    {
    }
    virtual void run() override
    {
        m_Work();
    }

  signals:
    void Refresh(std::string text);

  private:
    std::function<void()> m_Work;
};

GenericPopup::GenericPopup(QWidget* parent, std::string_view text)
    : PopupBase(parent)
{
    auto* text_widget{ new QLabel{ ToQString(text) } };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(text_widget);
    setLayout(layout);

    m_TextLabel = text_widget;

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

    m_Refresh = [work_thread](std::string_view text)
    {
        work_thread->Refresh(std::string{ text });
    };
    m_WorkerThread.reset(work_thread);

    PopupBase::Show();

    work_thread->quit();
    work_thread->wait();
    m_WorkerThread.reset();
    m_Refresh = nullptr;
}

GenericPopup::UninstallLogHookAtScopeExit GenericPopup::InstallLogHook()
{
    if (m_LogHookId.has_value())
    {
        UninstallLogHook();
    }

    m_LogHookId = Log::GetInstance(Log::c_MainLogName)->InstallHook([this](const Log::DetailInformation&, Log::LogLevel, std::string_view message)
                                                                    { UpdateText(message); });

    return UninstallLogHookAtScopeExit{ this };
}

void GenericPopup::UninstallLogHook()
{
    if (m_LogHookId.has_value())
    {
        Log::GetInstance(Log::c_MainLogName)->UninstallHook(m_LogHookId.value());
        m_LogHookId.reset();
    }
}

void GenericPopup::Sleep(dla::time_unit duration)
{
    const auto thread{ static_pointer_cast<WorkThread>(m_WorkerThread) };
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
    m_Refresh(text);
}

void GenericPopup::UpdateTextImpl(std::string_view text)
{
    adjustSize();
    m_TextLabel->setText(ToQString(text));
    if (text.starts_with("Failure"))
    {
        m_TextLabel->setStyleSheet("border: 3px solid red;");
    }
    else
    {
        m_TextLabel->setStyleSheet("");
    }
    adjustSize();
    Recenter();
}

AboutPopup::AboutPopup(QWidget* parent,
                       const Project& project)
    : PopupBase(parent)
{
    const auto build_time{ QDateTime::fromString(ToQString(ProxyPdfBuildTime()), Qt::ISODate) };
    const auto build_time_str{ QLocale::system().toString(build_time, QLocale::LongFormat) };

    auto* app_text{ new QLabel{ ToQString("Proxy-PDF-Maker") } };
    auto* version_text{ new QLabel{ ToQString(fmt::format("Version: {}", ProxyPdfVersion())) } };
    auto* built_text{ new QLabel{ QString{ "Built: %1" }.arg(build_time_str) } };
    auto* license_text{ new QLabel{ ToQString("Released under MIT License") } };

    auto* buttons{ new QWidget{} };
    {
        auto* close_button{ new QPushButton{ "Close" } };
        auto* issue_button{ new QPushButton{ "Report Issue" } };
        auto* package_bug_button{ new QPushButton{ "Package Bug Data" } };
        auto* package_progress{ new QProgressBar{} };
        package_progress->setVisible(false);

        auto* layout{ new QHBoxLayout };
        layout->addWidget(close_button);
        layout->addWidget(issue_button);
        layout->addWidget(package_bug_button);
        layout->addWidget(package_progress);
        buttons->setLayout(layout);

        auto open_issues_page{
            [this]()
            {
                QDesktopServices::openUrl(ToQString("https://github.com/Malacath-92/Proxy-PDF-Maker/issues"));
                close();
            }
        };
        auto package_bug{
            [=, &project]()
            {
                package_bug_button->setVisible(false);
                package_progress->setVisible(true);

                const auto& application{ *ppApp };
                const auto cache_folder{ application.GetCacheFolder() };
                const auto log_path{ cache_folder / "logs" };
                const auto temp_project_path{ cache_folder / "proj.json" };
                const auto config_path{ application.GetConfigFolder() / "config.ini" };
                const auto state_path{ application.GetConfigFolder() / "state.ini" };
                auto files{
                    project.GetCards() |
                    std::views::transform(&CardInfo::m_Name) |
                    std::views::filter(std::bind_front(&Project::IsCardRendered, std::cref(project))) |
                    std::views::transform(std::bind_front(&Project::GetCardImagePath, std::cref(project))) |
                    std::views::transform([](const auto& p)
                                          { return std::pair{ p, "images" / p.filename() }; }) |
                    std::ranges::to<std::vector>()
                };
                files.push_back(std::pair{ log_path, "logs" });
                files.push_back(std::pair{ temp_project_path, application.GetProjectPath().filename() });
                files.push_back(std::pair{ config_path, "config.ini" });
                files.push_back(std::pair{ state_path, "state.ini" });

                project.Dump(temp_project_path);

                auto* zip_worker{ new ZipWorker{ files, cache_folder / "bug.tar.gz" } };
                zip_worker->setAutoDelete(false);
                QObject::connect(zip_worker,
                                 &ZipWorker::Progress,
                                 package_progress,
                                 &QProgressBar::setValue);

                QThreadPool::globalInstance()->start(zip_worker, 100);

                {
                    QEventLoop loop;
                    QObject::connect(zip_worker, &ZipWorker::Done, &loop, &QEventLoop::quit);
                    loop.exec();
                }

                package_bug_button->setVisible(true);
                package_bug_button->setEnabled(false);
                package_progress->setVisible(false);

                zip_worker->deleteLater();
                switch (zip_worker->GetConclusion())
                {
                case ZipWorker::Conclusion::Success:
                    OpenFolder(cache_folder);
                    return;
                default:
                    if (zip_worker->HasError())
                    {
                        LogFatal("Unzip Error: {}", zip_worker->GetError());
                    }
                    return;
                }

                fs::remove(temp_project_path);
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
        QObject::connect(package_bug_button,
                         &QPushButton::clicked,
                         this,
                         package_bug);
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
