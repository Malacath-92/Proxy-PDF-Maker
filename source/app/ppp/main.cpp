#include <fmt/format.h>

#include <QApplication>
#include <QPushButton>
#include <qplugin.h>

#include <ppp/project/project.hpp>

#include <ppp/app.hpp>
#include <ppp/ui/main_window.hpp>

int main()
{
    constexpr auto print_fn{
        [](std::string_view str)
        { printf("%.*s\n", (int)str.size(), str.data()); }
    };

    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

    PrintProxyPrepApplication app;

    Project project{};
    project.Load(app.GetProjectPath(), print_fn);

    auto* tabs{ new MainTabs{} };
    auto* scroll{ new CardScrollArea{} };
    auto* preview{ new PrintPreview{} };
    auto* options{ new OptionsWidget{ app, project } };

    auto* main_window{ new PrintProxyPrepMainWindow{ tabs, scroll, preview, options } };
    app.SetMainWindow(main_window);

    main_window->show();

    const int return_code{ app.exec() };
    project.Dump(app.GetProjectPath(), print_fn);
    return return_code;
}
