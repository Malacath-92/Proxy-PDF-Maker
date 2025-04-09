#include <fmt/format.h>

#include <QApplication>
#include <QPushButton>

#ifdef WIN32
#include <QtPlugin>
#endif

#include <fpdfview.h>

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/style.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_print_preview.hpp>
#include <ppp/ui/widget_scroll_area.hpp>

[[nodiscard]] auto InitFPDF()
{
    FPDF_LIBRARY_CONFIG config;
    config.version = 2;
    config.m_pUserFontPaths = NULL;
    config.m_pIsolate = NULL;
    config.m_v8EmbedderSlot = 0;

    FPDF_InitLibraryWithConfig(&config);

    struct FPDFDtor
    {
        ~FPDFDtor()
        {
            FPDF_DestroyLibrary();
        }
    };
    return FPDFDtor{};
}

int main(int argc, char** argv)
{
    auto fpdf{ InitFPDF() };

#ifdef WIN32
    {
        static constexpr char locale_name[]{ ".utf-8" };
        std::setlocale(LC_ALL, locale_name);
        std::locale::global(std::locale(locale_name));
    }

    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

    PrintProxyPrepApplication app{ argc, argv };
    SetStyle(app, app.GetTheme());

    Project project{};
    project.Load(app.GetProjectPath(), nullptr);

    Cropper cropper{ [&app](std::string_view cube_name)
                     {
                         return GetCubeImage(app, cube_name);
                     },
                     project };
    CardProvider card_provider{ project };

    QObject::connect(&card_provider, &CardProvider::CardAdded, &cropper, &Cropper::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &cropper, &Cropper::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardModified, &cropper, &Cropper::CardModified);

    QObject::connect(&card_provider, &CardProvider::CardAdded, &project, &Project::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &project, &Project::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &project, &Project::CardRenamed);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &cropper, &Cropper::CardRenamed);

    auto* scroll_area{ new CardScrollArea{ project } };
    auto* print_preview{ new PrintPreview{ project } };
    auto* tabs{ new MainTabs{ project, scroll_area, print_preview } };
    auto* options{ new OptionsWidget{ app, project } };

    auto* main_window{ new PrintProxyPrepMainWindow{ tabs, options } };

    // TODO: More fine-grained control
    QObject::connect(&card_provider, &CardProvider::CardAdded, scroll_area, std::bind_front(&CardScrollArea::Refresh, scroll_area, std::ref(project)));
    QObject::connect(&card_provider, &CardProvider::CardRemoved, scroll_area, std::bind_front(&CardScrollArea::Refresh, scroll_area, std::ref(project)));
    QObject::connect(&card_provider, &CardProvider::CardRenamed, scroll_area, std::bind_front(&CardScrollArea::Refresh, scroll_area, std::ref(project)));

    {
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpenedDiff, &cropper, &Cropper::ClearCropWork);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChangedDiff, &cropper, &Cropper::ClearCropWork);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChangedDiff, &cropper, &Cropper::ClearCropWork);

        QObject::connect(main_window, &PrintProxyPrepMainWindow::BasePreviewWidthChangedDiff, &cropper, &Cropper::ClearPreviewWork);

        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpenedDiff, &cropper, &Cropper::NewProjectOpenedDiff);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChangedDiff, &cropper, &Cropper::ImageDirChangedDiff);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChangedDiff, &cropper, &Cropper::BleedChangedDiff);

        QObject::connect(main_window, &PrintProxyPrepMainWindow::EnableUncropChangedDiff, &cropper, &Cropper::EnableUncropChangedDiff);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChangedDiff, &cropper, &Cropper::ColorCubeChangedDiff);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BasePreviewWidthChangedDiff, &cropper, &Cropper::BasePreviewWidthChangedDiff);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::MaxDPIChangedDiff, &cropper, &Cropper::MaxDPIChangedDiff);
    }

    {
        // Sequence refreshing of cards after cleanup of cropper
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, &card_provider, &CardProvider::NewProjectOpened);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, &card_provider, &CardProvider::ImageDirChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, &card_provider, &CardProvider::BleedChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::EnableUncropChanged, &card_provider, &CardProvider::EnableUncropChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChanged, &card_provider, &CardProvider::ColorCubeChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BasePreviewWidthChanged, &card_provider, &CardProvider::BasePreviewWidthChanged);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::MaxDPIChanged, &card_provider, &CardProvider::MaxDPIChanged);
    }

    {
        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideEnabledChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideDefaultChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::OversizedEnabledChanged, scroll_area, &CardScrollArea::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::DisplayColumnsChanged, scroll_area, &CardScrollArea::Refresh);
    }

    {
        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::PageSizeChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::CustomMarginsChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::CardLayoutChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::GuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::GuidesColorChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BleedChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::CornerWeightChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideGuidesEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideDefaultChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideOffsetChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::OversizedEnabledChanged, print_preview, &PrintPreview::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ColorCubeChanged, print_preview, &PrintPreview::Refresh);
    }

    {
        // TODO: Fine-tune these connections to reduce amount of pointless work
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, options, &OptionsWidget::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::NewProjectOpened, options, &OptionsWidget::RefreshWidgets);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, options, &OptionsWidget::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::ImageDirChanged, options, &OptionsWidget::RefreshWidgets);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideEnabledChanged, options, &OptionsWidget::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideEnabledChanged, options, &OptionsWidget::RefreshWidgets);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideDefaultChanged, options, &OptionsWidget::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::BacksideDefaultChanged, options, &OptionsWidget::RefreshWidgets);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::OversizedEnabledChanged, options, &OptionsWidget::Refresh);
        QObject::connect(main_window, &PrintProxyPrepMainWindow::OversizedEnabledChanged, options, &OptionsWidget::RefreshWidgets);
    }

    app.SetMainWindow(main_window);
    main_window->show();

    // Write preview to project and forward to widgets
    QObject::connect(&cropper, &Cropper::PreviewUpdated, &project, &Project::SetPreview);

    // Enable and disable Render button
    QObject::connect(&cropper, &Cropper::CropWorkStart, options, &OptionsWidget::CropperWorking);
    QObject::connect(&cropper, &Cropper::CropWorkDone, options, &OptionsWidget::CropperDone);
    QObject::connect(&cropper, &Cropper::CropProgress, options, &OptionsWidget::CropperProgress);

    // Write preview cache to file
    QObject::connect(&cropper, &Cropper::PreviewWorkDone, &project, &Project::CropperDone);

    cropper.Start();
    card_provider.Start();

    const int return_code{ app.exec() };
    project.Dump(app.GetProjectPath(), nullptr);
    return return_code;
}
