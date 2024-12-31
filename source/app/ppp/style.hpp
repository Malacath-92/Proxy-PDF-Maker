#pragma once

#include <array>
#include <string>
#include <string_view>

class QApplication;

std::vector<std::string> GetStyles();
void SetStyle(QApplication& application, std::string_view style);
