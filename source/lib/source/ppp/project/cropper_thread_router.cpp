#include <ppp/project/cropper_thread_router.hpp>

#include <ranges>

CropperThreadRouter::CropperThreadRouter(const Project& project)
    : m_Project{ project }
{
}

void CropperThreadRouter::NewProjectOpened()
{
    NewProjectOpenedDiff(m_Project.Data);
}

void CropperThreadRouter::ImageDirChanged()
{
    ImageDirChangedDiff(m_Project.Data.ImageDir,
                        m_Project.Data.CropDir,
                        m_Project.Data.Previews | std::views::keys | std::ranges::to<std::vector>());
}

void CropperThreadRouter::CardSizeChanged()
{
    CardSizeChangedDiff(m_Project.Data.CardSizeChoice);
}

void CropperThreadRouter::BleedChanged()
{
    BleedChangedDiff(m_Project.Data.BleedEdge);
}

void CropperThreadRouter::EnableUncropChanged()
{
    EnableUncropChangedDiff(g_Cfg.EnableUncrop);
}

void CropperThreadRouter::ColorCubeChanged()
{
    ColorCubeChangedDiff(g_Cfg.ColorCube);
}

void CropperThreadRouter::BasePreviewWidthChanged()
{
    BasePreviewWidthChangedDiff(g_Cfg.BasePreviewWidth);
}

void CropperThreadRouter::MaxDPIChanged()
{
    MaxDPIChangedDiff(g_Cfg.MaxDPI);
}
