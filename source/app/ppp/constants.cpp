#include <ppp/constants.hpp>

#include <qdir.h>

fs::path cwd()
{
    thread_local fs::path current_working_directory{
        QDir::currentPath().toStdString()
    };
    return current_working_directory;
}
