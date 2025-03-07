#include <fmt/format.h>

#include <vips/vips.h>

#include <QApplication>
#include <QPushButton>

#ifdef WIN32
#include <QtPlugin>
#endif

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

    VIPS_INIT(argv[0]);

    PrintProxyPrepApplication app{ argc, argv };
    SetStyle(app, app.GetTheme());

    Project project{};
    {
        GenericPopup startup_window{ nullptr, "Rendering PDF..." };
        const auto render_work{
            [&]()
            {
                project.Load(app.GetProjectPath(), GetCubeImage(app, CFG.ColorCube), startup_window.MakePrintFn());
            }
        };
        startup_window.ShowDuringWork(render_work);
    }

    auto* scroll_area{ new CardScrollArea{ project } };
    auto* print_preview{ new PrintPreview{ project } };
    auto* tabs{ new MainTabs{ project, scroll_area, print_preview } };
    auto* options{ new OptionsWidget{ app, project } };

    auto* main_window{ new PrintProxyPrepMainWindow{ project, tabs, scroll_area, print_preview, options } };
    app.SetMainWindow(main_window);

    main_window->show();

    const int return_code{ app.exec() };
    project.Dump(app.GetProjectPath(), nullptr);
    return return_code;
}
