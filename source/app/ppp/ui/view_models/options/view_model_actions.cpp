#include <ppp/ui/view_models/options/view_model_actions.hpp>

#include <QMessageBox>

#include <ppp/app.hpp>
#include <ppp/util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>

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

    const auto& application{ *ppApp };
    const auto& outputs_folder{ application.GetOutputsFolder() };
    const auto [frontside_path, backside_path]{ GeneratePdf(m_Project, m_Cfg, outputs_folder) };
    OpenFolder(outputs_folder);
    OpenFile(frontside_path);
    if (backside_path.has_value())
    {
        OpenFile(backside_path.value());
    }

    if (m_Project.m_Data.m_ExportExactGuides)
    {
        GenerateCardsSvg(m_Project, m_Cfg.m_NoCropMode, outputs_folder);
        GenerateCardsDxf(m_Project, m_Cfg.m_NoCropMode, outputs_folder);
    }
}

fs::path ActionsViewModel::GetImageFolderBase() const
{
    const auto& application{ *ppApp };
    return application.GetProjectsFolder();
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
    PdfBackendChanged(m_Cfg.m_Backend);
}

bool ActionsViewModel::VerifyProject() const
{
    QString warnings;
    for (const auto& [card_name, preview] : m_Project.m_Data.m_Previews)
    {
        const bool bad_aspect_ratio{ preview.m_BadAspectRatio };
        const bool bad_rotation{ preview.m_BadRotation };
        if ((bad_rotation || bad_aspect_ratio) && m_Project.IsCardRendered(card_name))
        {
            const char* warning{ GetCardWarning(bad_aspect_ratio, bad_rotation, false) };
            warnings += QString{ "    %1: %2\n" }.arg(card_name.string()).arg(warning);
        }
    }

    if (!warnings.isEmpty())
    {
        warnings.removeLast();

        const auto response{
            QMessageBox::question(
                nullptr,
                "Image Warnings",
                QString{ "Project contains the following warnings:\n"
                         "%1\n"
                         "Are you sure you want to render it?" }
                    .arg(warnings))
        };
        if (response == QMessageBox::StandardButton::No)
        {
            return false;
        }
    }

    return true;
}
