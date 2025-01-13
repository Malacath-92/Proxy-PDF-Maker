#pragma once

#include <ppp/util.hpp>

class QString;

QString ToQString(const char* c_string);
QString ToQString(const wchar_t* wc_string);
QString ToQString(const std::string& string);
QString ToQString(const std::string_view string_view);
QString ToQString(const fs::path& path);
