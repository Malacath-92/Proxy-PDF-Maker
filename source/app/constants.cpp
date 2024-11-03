#include "constants.hpp"

#include <qdir.h>

std::string_view cwd()
{
    static std::string current_working_directory{
        QDir::currentPath().toStdString()
    };
    return current_working_directory;
}
