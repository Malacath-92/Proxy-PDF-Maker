#include <ppp/ui/view_models/view_model_blank_card.hpp>

#include <QPixmap>

#include <ppp/config.hpp>
#include <ppp/image.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>

#include <ppp/profile/profile.hpp>

BlankCardViewModel::BlankCardViewModel(CardViewParams params,
                                       const Project& project)
    : m_ViewParams{ params }
    , m_Project{ const_cast<Project&>(project) }
{
}

float BlankCardViewModel::GetCardAspectRatio() const
{
    return GetCardWidgetAspectRatio(m_Project, m_ViewParams.m_Rotation, m_ViewParams.m_BleedEdge);
}

void BlankCardViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    MinimumWidthChanged(m_ViewParams.m_MinimumWidth);

    const auto card_size{ m_Project.CardSizeWithBleed() };
    const auto bleed_edge{ m_Project.m_Data.m_BleedEdge };

    const auto width{ 512_pix };
    const auto height{ width / GetCardAspectRatio() };
    const auto image{
        [&, this](const Image& img)
        {
            if (m_ViewParams.m_RoundedCorners && bleed_edge == 0_mm)
            {
                if (m_Project.IsCardRoundedRect())
                {
                    return img
                        .RoundCorners(card_size, m_Project.CardCornerRadius())
                        .Rotate(m_ViewParams.m_Rotation);
                }
                else if (m_Project.IsCardSvg())
                {
                    return img
                        .ClipSvg(m_Project.CardSvgData())
                        .Rotate(m_ViewParams.m_Rotation);
                }
            }
            return img
                .Rotate(m_ViewParams.m_Rotation);
        }(Image::PlainColor({ width, height }, ColorRGBA8{ 0xff, 0xff, 0xff, 0xff }))
    };

    PixmapChanged(StoreIntoQtPixmap(image));
}
