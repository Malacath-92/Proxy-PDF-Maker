#include <ppp/json_util.hpp>

#include <exception>
#include <ranges>

#include <nlohmann/json.hpp>

auto split_path(std::string_view path)
{
    static constexpr auto c_ToStringViews{ std::views::transform(
        [](auto str)
        { return std::string_view(str.data(), str.size()); }) };
    return path | std::views::split('.') | c_ToStringViews;
}

const nlohmann::json& GetJsonValue(const nlohmann::json& root,
                                   std::string_view path)
{
    std::reference_wrapper target_json{ root };
    for (const auto& path_part : split_path(path))
    {
        if (!target_json.get().contains(path_part))
        {
            throw std::logic_error{
                fmt::format("Path {} is not part of json object", path)
            };
        }

        target_json = target_json.get()[path_part];
    }
    return target_json;
}
nlohmann::json& GetJsonValue(nlohmann::json& root,
                             std::string_view path)
{
    std::reference_wrapper target_json{ root };
    for (const auto& path_part : split_path(path))
    {
        if (!target_json.get().contains(path_part))
        {
            throw std::logic_error{
                fmt::format("Path {} is not part of json object", path)
            };
        }

        target_json = target_json.get()[path_part];
    }
    return target_json;
}

nlohmann::json& SetJsonValue(nlohmann::json& root,
                             std::string_view path,
                             nlohmann::json value)
{
    std::reference_wrapper target_json{ root };
    for (const auto& path_part : split_path(path))
    {
        if (target_json.get().type() != nlohmann::json::value_t::object)
        {
            throw std::logic_error{
                fmt::format("Path {} is not part of json object", path)
            };
        }
        else if (!target_json.get().contains(path_part))
        {
            target_json.get()[path_part] = nlohmann::json{
                nlohmann::json::value_t::object
            };
        }

        target_json = target_json.get()[path_part];
    }

    target_json.get() = std::move(value);
    return target_json.get();
}
