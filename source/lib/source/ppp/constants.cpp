#include <ppp/constants.hpp>

#include <qdir.h>

fs::path cwd()
{
    thread_local fs::path s_CurrentWorkingDirectory{
        QDir::currentPath().toStdString()
    };
    return s_CurrentWorkingDirectory;
}
