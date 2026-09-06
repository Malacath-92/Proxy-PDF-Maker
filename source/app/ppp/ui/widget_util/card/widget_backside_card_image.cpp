#include <ppp/ui/widget_util/card/widget_backside_card_image.hpp>

#include <ppp/ui/view_models/view_model_card.hpp>

#include <ppp/profile/profile.hpp>

BacksideImage::BacksideImage(const fs::path& backside_name, const Project& project)
    : BacksideImage{ backside_name, CardImageWidgetParams{}.m_MinimumWidth, project }
{
    TRACY_AUTO_SCOPE();
}
BacksideImage::BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project)
    : CardImage{
        // TODO: Proper MVVM
        new CardViewModel{ backside_name, CardViewParams{ .m_Backside = true, .m_MinimumWidth{ minimum_width } }, project },
        backside_name,
        project,
        CardImageWidgetParams{ .m_Backside = true, .m_MinimumWidth{ minimum_width } }
    }
{
    TRACY_AUTO_SCOPE();
}

void BacksideImage::Refresh(const fs::path& backside_name, const Project& project)
{
    Refresh(backside_name, CardImageWidgetParams{}.m_MinimumWidth, project);
}
void BacksideImage::Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project)
{
    TRACY_AUTO_SCOPE();
    CardImage::Refresh(
        backside_name,
        project,
        CardImageWidgetParams{ .m_Backside = true, .m_MinimumWidth{ minimum_width } });
}
