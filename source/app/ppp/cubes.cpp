#include <ppp/cubes.hpp>

#include <ranges>

#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QStyleFactory>

#include <ppp/app.hpp>
#include <ppp/project/image_ops.hpp>

std::vector<std::string> GetCubeNames()
{
    std::vector<std::string> cubes_names{ "None" };

    Q_INIT_RESOURCE(resources);
    for (const auto style_dir : { ":/res/cubes", "./res/cubes" })
    {
        QDirIterator it(style_dir);
        while (it.hasNext())
        {
            const QFileInfo next{ it.nextFileInfo() };
            if (!next.isFile() || next.suffix().toLower() != "cube")
            {
                continue;
            }

            std::string base_name{ next.baseName().toStdString() };
            if (std::ranges::contains(cubes_names, base_name))
            {
                continue;
            }

            cubes_names.push_back(std::move(base_name));
        }
    }

    return cubes_names;
}

void PreloadCube(PrintProxyPrepApplication& application, std::string_view cube_name)
{
    if (cube_name == "None" || application.GetCube(std::string{ cube_name }) != nullptr)
    {
        return;
    }

    fs::path cube_path{ fmt::format("./res/cubes/{}.CUBE", cube_name) };
    if (!QFile::exists(cube_path))
    {
        Q_INIT_RESOURCE(resources);
        cube_path = fmt::format(":/res/cubes/{}.CUBE", cube_name);
    }
    application.SetCube(std::string{ cube_name }, LoadColorCube(cube_path));
}

const cv::Mat* GetCubeImage(PrintProxyPrepApplication& application, std::string_view cube_name)
{
    PreloadCube(application, cube_name);
    return application.GetCube(std::string{ cube_name });
}
