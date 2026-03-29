#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cv
{
class Mat;
}

std::vector<std::string> GetCubeNames();
void PreloadCube(std::string_view cube_name);
const cv::Mat* GetCubeImage(std::string_view cube_name);
