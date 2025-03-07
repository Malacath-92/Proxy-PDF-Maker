#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <ppp/image.hpp>

class PrintProxyPrepApplication;

std::vector<std::string> GetCubeNames();
void PreloadCube(PrintProxyPrepApplication& application, std::string_view cube_name);
const ColorCube* GetCubeImage(PrintProxyPrepApplication& application, std::string_view cube_name);
