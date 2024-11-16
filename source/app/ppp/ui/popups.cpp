#include <ppp/ui/popups.hpp>

#include <ranges>

#include <QFileDialog>
#include <QLabel>
#include <QThread>
#include <QVBoxLayout>

#include <fmt/ranges.h>

#include <ppp/project/image_ops.hpp>

std::optional<fs::path> OpenFolderDialog(fs::path root)
{
    const QString choice{
        QFileDialog::getExistingDirectory(
            nullptr,
            "Choose Folder",
            ".",
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

std::optional<fs::path> OpenFileDialog(std::string_view title, fs::path root, std::string_view filter, FileDialogType type)
{
    QString choice{};
    if (type == FileDialogType::Open)
    {
        choice = QFileDialog::getOpenFileName(
            nullptr,
            QString::fromStdString(std::string{ title }),
            QString::fromStdString(root.string()),
            QString::fromStdString(std::string{ filter }));
    }
    else
    {
        choice = QFileDialog::getSaveFileName(
            nullptr,
            QString::fromStdString(std::string{ title }),
            QString::fromStdString(root.string()),
            QString::fromStdString(std::string{ filter }));
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

std::optional<fs::path> OpenImageDialog(fs::path root)
{
    const auto image_filters{ ValidImageExtensions |
                              std::views::transform([](const fs::path& ext)
                                                    { return "*" + ext.string(); }) |
                              std::ranges::to<std::vector>() };
    return OpenFileDialog(
        "Open Image",
        root,
        fmt::format("Image Files ({})", image_filters),
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
    void Run()
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
    auto* text_widget{ new QLabel{ QString::fromStdString(std::string{ text }) } };

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

    open();
    QObject::connect(work_thread,
                     &QThread::finished,
                     this,
                     &QDialog::close);
    QObject::connect(work_thread,
                     &WorkThread::Refresh,
                     this,
                     &GenericPopup::UpdateTextImpl);

    WorkerThread.reset(work_thread);
    Refresh = [work_thread](std::string_view text)
    {
        work_thread->Refresh(std::string{ text });
    };
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
    TextLabel->setText(QString::fromStdString(std::string{ text }));
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
