#include <fmt/format.h>

#include <QApplication>
#include <QPushButton>
#include <qplugin.h>

int main(int argc, char** argv)
{
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

    QApplication app(argc, argv);

    QPushButton button("Hello world!");
    QObject::connect(&button,
                     &QPushButton::clicked,
                     &button,
                     [&](bool){ app.closeAllWindows(); });
    button.show();

    return app.exec();
}
