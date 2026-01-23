#pragma once

#include <string>
#include <string_view>
#include <vector>

class QApplication;

std::vector<std::string> GetStyles();
void SetStyle(std::string_view style);
