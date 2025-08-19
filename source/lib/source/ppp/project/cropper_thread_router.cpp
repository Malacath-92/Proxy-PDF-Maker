#include <ppp/project/cropper_thread_router.hpp>

#include <ranges>

CropperThreadRouter::CropperThreadRouter(const Project& project)
    : m_Project{ project }
{
}

void CropperThreadRouter::NewProjectOpened()
{
    NewProjectOpenedDiff(m_Project.m_Data);
}

void CropperThreadRouter::ImageDirChanged()
{
    ImageDirChangedDiff(m_Project.m_Data.m_ImageDir,
                        m_Project.m_Data.m_CropDir,
                        m_Project.m_Data.m_UncropDir,
                        m_Project.m_Data.m_Previews | std::views::keys | std::ranges::to<std::vector>());
}

void CropperThreadRouter::CardSizeChanged()
{
    CardSizeChangedDiff(m_Project.m_Data.m_CardSizeChoice);
}

void CropperThreadRouter::BleedChanged()
{
    BleedChangedDiff(m_Project.m_Data.m_BleedEdge);
}

void CropperThreadRouter::ColorCubeChanged()
{
    ColorCubeChangedDiff(g_Cfg.m_ColorCube);
}

void CropperThreadRouter::BasePreviewWidthChanged()
{
    BasePreviewWidthChangedDiff(g_Cfg.m_BasePreviewWidth);
}

void CropperThreadRouter::MaxDPIChanged()
{
    MaxDPIChangedDiff(g_Cfg.m_MaxDPI);
}
