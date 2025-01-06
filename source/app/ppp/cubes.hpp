#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <opencv2/core/mat.hpp>

class PrintProxyPrepApplication;

std::vector<std::string> GetCubeNames();
void PreloadCube(PrintProxyPrepApplication& application, std::string_view cube_name);
const cv::Mat* GetCubeImage(PrintProxyPrepApplication& application, std::string_view cube_name);
