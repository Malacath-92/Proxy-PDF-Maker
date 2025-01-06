#include <ppp/style.hpp>

#include <ranges>

#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QStyleFactory>

#include <ppp/util.hpp>

using namespace std::string_view_literals;
inline constexpr std::array BuiltInStyles{
    "Default"sv,
    "Fusion"sv,
};

std::vector<std::string> GetStyles()
{
    std::vector<std::string> styles{};

    for (const auto& builtin_style : BuiltInStyles)
    {
        styles.push_back(std::string{ builtin_style });
    }

    Q_INIT_RESOURCE(resources);
    for (const auto style_dir : { ":/res/styles", "./res/styles" })
    {
        QDirIterator it(style_dir);
        while (it.hasNext())
        {
            const QFileInfo next{ it.nextFileInfo() };
            if (!next.isFile() || next.suffix().toLower() != "qss")
            {
                continue;
            }

            std::string base_name{ next.baseName().toStdString() };
            if (std::ranges::contains(styles, base_name))
            {
                continue;
            }

            styles.push_back(std::move(base_name));
        }
    }

    return styles;
}

void SetStyle(QApplication& application, std::string_view style)
{
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
        QString style_path{ QString::asprintf("./res/styles/%.*s.qss", static_cast<int>(style.size()), style.data()) };
        if (!QFile::exists(style_path))
        {
            Q_INIT_RESOURCE(resources);
            style_path = QString::asprintf(":/res/styles/%.*s.qss", static_cast<int>(style.size()), style.data());
        }

        QFile style_file{ style_path };
        if (style_file.open(QFile::ReadOnly))
        {
            application.setStyleSheet(QLatin1String{ style_file.readAll() });
        }
    }
}
