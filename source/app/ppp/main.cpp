#include <fmt/format.h>

#include <QApplication>
#include <QPushButton>
#include <qplugin.h>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

int main(int argc, char** argv)
{
    Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

    QApplication app(argc, argv);

    QPushButton button("Hello world!");
    QObject::connect(&button,
                     &QPushButton::clicked,
                     &button,
                     [&]()
                     { app.closeAllWindows(); });
    button.show();

    Image::Init(argv[0]);

    const auto file{ "D:/Programs/print-proxy-prep-main/images/Everywhere (Hans Tseng).png"_p };
    if (const Image image{ Image::Read(file) })
    {
        const Image flipped{ image.Rotate(Image::Rotation::Degree180) };
        flipped.Write(fs::path{ file }.replace_extension(".flip.jpg"));
        const Image rotted{ image.Rotate(Image::Rotation::Degree90) };
        rotted.Write(fs::path{ file }.replace_extension(".rot.bmp"));
    }

    return app.exec();
}
