#pragma once

#include <string_view>

#include <nlohmann/json_fwd.hpp>

const nlohmann::json& GetJsonValue(const nlohmann::json& root,
                                   std::string_view path);
nlohmann::json& GetJsonValue(nlohmann::json& root,
                             std::string_view path);

nlohmann::json& SetJsonValue(nlohmann::json& root,
                             std::string_view path,
                             nlohmann::json value);

class JsonProvider
{
  public:
    virtual nlohmann::json GetJsonValue(std::string_view path) const = 0;
    virtual void SetJsonValue(std::string_view path, nlohmann::json value) = 0;
};
