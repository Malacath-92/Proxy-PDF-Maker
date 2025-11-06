#include <ppp/project/card_info.hpp>

#include <ppp/project/project.hpp>

fs::path CardInfo::GetSourcePath(const ProjectData& data) const
{
    return m_ExternalPath
        .or_else([&]() -> std::optional<fs::path>
                 { return data.m_ImageDir / m_Name; })
        .value();
}

fs::path CardInfo::GetSourceFolder(const ProjectData& data) const
{
    return GetSourcePath(data).parent_path();
}
