#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Image;
class PrintProxyPrepApplication;

std::vector<std::string> GetModelNames();

std::string GetModelFilename(std::string_view model_name);

bool ModelRequiresDownload(std::string_view model_name);
std::optional<std::string_view> GetModelUrl(std::string_view model_name);

void LoadModel(PrintProxyPrepApplication& application, std::string_view model_name);
void UnloadModel(PrintProxyPrepApplication& application, std::string_view model_name);
Image RunModel(PrintProxyPrepApplication& application,
               std::string_view model_name,
               const Image& image);
