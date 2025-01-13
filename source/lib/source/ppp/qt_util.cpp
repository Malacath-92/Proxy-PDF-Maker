#include <ppp/qt_util.hpp>

#include <QString>

QString ToQString(const char* c_string)
{
    return QString::fromUtf8(c_string);
}

QString ToQString(const wchar_t* wc_string)
{
    return QString::fromWCharArray(wc_string);
}

QString ToQString(const std::string& string)
{
    return QString::fromStdString(string);
}

QString ToQString(const std::string_view string_view)
{
    return QString::fromLatin1(string_view);
}

QString ToQString(const fs::path& path)
{
    return ToQString(path.c_str());
}
