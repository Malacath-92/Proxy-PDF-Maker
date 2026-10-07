#include <ppp/ui/view_models/popups/view_model_image_browse_popup.hpp>

#include <ranges>

#include <ppp/project/project.hpp>

#include <ppp/ui/view_models/view_model_selectable_card_grid.hpp>

#include <ppp/profile/profile.hpp>

ImageBrowseViewModel::ImageBrowseViewModel(const Project& project,
                                           std::span<const fs::path> ignored_images)
    : m_Project{ project }
    , m_IgnoredImages{ ignored_images }
{
}

bool ImageBrowseViewModel::HasCards() const
{
    const auto num_valid_ignored_images{
        std::ranges::count_if(m_IgnoredImages, [&](const auto& img)
                              { return m_Project.HasCard(img); })
    };

    const auto& cards{ m_Project.GetCards() };
    const auto num_valid_images{
        std::ranges::count_if(cards,
                              [&](const auto& img)
                              { return !img.m_Transient; })
    };

    return num_valid_images > num_valid_ignored_images;
}
bool ImageBrowseViewModel::HasIgnoredCards() const
{
    const auto num_valid_ignored_images{
        std::ranges::count_if(m_IgnoredImages, [&](const auto& img)
                              { return m_Project.HasCard(img); })
    };
    return num_valid_ignored_images > 0;
}

SelectableCardGridViewModel* ImageBrowseViewModel::MakeGridViewModel()
{
    return new SelectableCardGridViewModel{ m_Project, m_IgnoredImages };
}
