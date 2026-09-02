#include <ppp/ui/widget_util/card/widget_blank_card_image.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>

#include <ppp/profile/profile.hpp>

BlankCardImage::BlankCardImage(const Project& project, CardImageWidgetParams params)
    : WidgetWithCardSize{ GetCardWidgetAspectRatio(project, params.m_Rotation, params.m_BleedEdge) }
{
    TRACY_AUTO_SCOPE();

    setStyleSheet("QLabel{ background-color: transparent; }");

    const auto card_size{ project.CardSizeWithBleed() };
    const auto bleed_edge{ project.m_Data.m_BleedEdge };

    const auto width{ 512_pix };
    const auto height{ heightForWidth(width / 1_pix) * 1_pix };
    const auto img{
        [&](const Image& img)
        {
            if (params.m_RoundedCorners && bleed_edge == 0_mm)
            {
                if (project.IsCardRoundedRect())
                {
                    return img
                        .RoundCorners(card_size, project.CardCornerRadius())
                        .Rotate(params.m_Rotation);
                }
                else if (project.IsCardSvg())
                {
                    return img
                        .ClipSvg(project.CardSvgData())
                        .Rotate(params.m_Rotation);
                }
            }
            return img
                .Rotate(params.m_Rotation);
        }(Image::PlainColor({ width, height }, ColorRGBA8{ 0xff, 0xff, 0xff, 0xff }))
    };
    setPixmap(StoreIntoQtPixmap(img));

    setScaledContents(true);

    setMinimumWidth(params.m_MinimumWidth.value);
}
