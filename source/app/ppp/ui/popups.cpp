#include <ppp/ui/popups.hpp>

#include <ranges>

#include <QFileDialog>
#include <QLabel>
#include <QThread>
#include <QVBoxLayout>

#include <fmt/ranges.h>

#include <ppp/project/image_ops.hpp>

std::optional<fs::path> OpenFolderDialog(const fs::path& root)
{
    const QString choice{
        QFileDialog::getExistingDirectory(
            nullptr,
            "Choose Folder",
            QString::fromWCharArray(root.c_str()),
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
            QString::fromLatin1(title),
            QString::fromWCharArray(root.c_str()),
            QString::fromLatin1(filter));
    }
    else
    {
        choice = QFileDialog::getSaveFileName(
            nullptr,
            QString::fromLatin1(title),
            QString::fromWCharArray(root.c_str()),
            QString::fromLatin1(filter));
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

class WorkThread : public QThread
{
    Q_OBJECT;

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
    : QDialog(parent)
{
    auto* text_widget{ new QLabel{ QString::fromLatin1(text) } };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(text_widget);
    setLayout(layout);

    setWindowFlags(Qt::WindowType::FramelessWindowHint |
                   Qt::WindowType::WindowStaysOnTopHint);

    QPalette old_palette{ palette() };
    old_palette.setColor(backgroundRole(), 0x111111);
    setPalette(old_palette);
    setAutoFillBackground(true);

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

    WorkerThread.reset(work_thread);
    Refresh = [work_thread](std::string_view text)
    {
        work_thread->Refresh(std::string{ text });
    };

    open();
    exec();
    Refresh = nullptr;
    WorkerThread.reset();
}

std::function<void(std::string_view)> GenericPopup::MakePrintFn()
{
    return [this](std::string_view text)
    {
        fmt::print("{}", text);
        UpdateText(text);
    };
}

void GenericPopup::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    Recenter();
}

void GenericPopup::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    // Three times???
    Recenter();
    Recenter();
    Recenter();
}

void GenericPopup::UpdateText(std::string_view text)
{
    Refresh(text);
}

void GenericPopup::UpdateTextImpl(std::string_view text)
{
    adjustSize();
    TextLabel->setText(QString::fromLatin1(text));
    adjustSize();
    Recenter();
}

void GenericPopup::Recenter()
{
    const QWidget* parent{ parentWidget() };
    if (parent != nullptr)
    {
        const auto center{ rect().center() };
        const auto parent_half_size{ parent->rect().size() / 2 };
        const auto offset{ QPoint(parent_half_size.width(), parent_half_size.height()) - center };
        move(offset);
    }
}

#include "popups.moc"
