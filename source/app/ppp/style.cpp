#include <ppp/style.hpp>

#include <ranges>
#include <unordered_map>

#include <QApplication>
#include <QFile>
#include <QStyleFactory>

inline constexpr std::array BuiltInStyles{
    "Default"sv,
    "Fusion"sv,
};

void SetStyle(QApplication& application, std::string_view style)
{
    if (!std::ranges::contains(AvailableStyles, style))
    {
        return;
    }

    if (std::ranges::contains(BuiltInStyles, style))
    {
        application.setStyleSheet("");

        if (style == "Default")
        {
            application.setStyle(QStyleFactory::create(QStyleFactory::keys()[0]));
        }
        else if (style == "Fusion")
        {
            application.setStyle("fusion");
        }
    }
    else
    {
        Q_INIT_RESOURCE(resources);

        QFile style_file{ QString::asprintf(":/res/styles/%.*s.qss", static_cast<int>(style.size()), style.data()) };
        style_file.open(QFile::ReadOnly);
        application.setStyleSheet(QLatin1String{ style_file.readAll() });
    }
}
