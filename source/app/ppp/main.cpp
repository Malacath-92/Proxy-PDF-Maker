#include <fmt/format.h>

#include <QApplication>
#include <QPushButton>

#ifdef WIN32
#include <QtPlugin>
#endif

#include <ppp/project/card_provider.hpp>
#include <ppp/project/cropper.hpp>
#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/style.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups.hpp>

int main(int argc, char** argv)
{
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

    CardProvider card_provider{
        project.Data.ImageDir,
        project.Data.CropDir,
    };

    Cropper cropper{ project };
    QObject::connect(&card_provider, &CardProvider::CardAdded, &cropper, &Cropper::CardAdded);
    QObject::connect(&card_provider, &CardProvider::CardRemoved, &cropper, &Cropper::CardRemoved);
    QObject::connect(&card_provider, &CardProvider::CardModified, &cropper, &Cropper::CardModified);

    auto card_renamed{
        [&project](const fs::path& old_card_name, const fs::path& new_card_name)
        {
            project.CardRenamed(old_card_name, new_card_name);
        }
    };
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &app, card_renamed);
    QObject::connect(&card_provider, &CardProvider::CardRenamed, &cropper, &Cropper::CardRenamed);

    auto* scroll_area{ new CardScrollArea{ project } };
    auto* print_preview{ new PrintPreview{ project } };
    auto* tabs{ new MainTabs{ project, scroll_area, print_preview } };
    auto* options{ new OptionsWidget{ app, project } };

    auto* main_window{ new PrintProxyPrepMainWindow{ project, tabs, scroll_area, print_preview, options } };
    app.SetMainWindow(main_window);

    main_window->show();

    // Write preview to project and forward to widgets
    QObject::connect(&cropper, &Cropper::PreviewUpdated, &project, &Project::SetPreview);

    // Enable and disable Render button
    QObject::connect(&cropper, &Cropper::CropWorkStart, options, &OptionsWidget::CropperWorking);
    QObject::connect(&cropper, &Cropper::CropWorkDone, options, &OptionsWidget::CropperDone);

    // Write preview cache to file
    QObject::connect(&cropper, &Cropper::PreviewWorkDone, &project, &Project::CropperDone);

    card_provider.Start();
    cropper.Start();

    const int return_code{ app.exec() };
    project.Dump(app.GetProjectPath(), nullptr);
    return return_code;
}
