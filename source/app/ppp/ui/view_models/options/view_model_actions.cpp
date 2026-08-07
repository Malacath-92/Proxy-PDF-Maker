#include <ppp/ui/view_models/options/view_model_actions.hpp>

#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

ActionsViewModel::ActionsViewModel(Project& project,
                                   const Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
}

void ActionsViewModel::RenderDocument() const
{
    TRACY_AUTO_SCOPE();

    const auto [frontside_path, backside_path]{ GeneratePdf(m_Project, m_Cfg) };
    OpenFile(frontside_path);
    if (backside_path.has_value())
    {
        OpenFile(backside_path.value());
    }

    if (m_Project.m_Data.m_ExportExactGuides)
    {
        GenerateCardsSvg(m_Project, m_Cfg.m_NoCropMode);
        GenerateCardsDxf(m_Project, m_Cfg.m_NoCropMode);
    }
}

void ActionsViewModel::SetImagesFolder(fs::path new_image_dir)
{
    TRACY_AUTO_SCOPE();
    m_Project.SetImageDir(std::move(new_image_dir));
}

void ActionsViewModel::OpenImagesFolder() const
{
    OpenFolder(m_Project.m_Data.m_ImageDir);
}

void ActionsViewModel::EmitDefaults()
{
    RenderBackendChanged(m_Cfg.m_Backend);
}
