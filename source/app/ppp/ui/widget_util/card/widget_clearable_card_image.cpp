#include <ppp/ui/widget_util/card/widget_clearable_card_image.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>
#include <ppp/ui/widget_util/card/widget_blank_card_image.hpp>
#include <ppp/ui/widget_util/card/widget_card_image.hpp>

#include <ppp/profile/profile.hpp>

ClearableCardImage::ClearableCardImage(const Project& project,
                                       OptionalImageRef card_name,
                                       bool backside)
    : WidgetWithCardSize{ GetCardWidgetAspectRatio(project, Rotation::None, 0_mm) }
    , m_Project{ project }
{
    TRACY_AUTO_SCOPE();

    const auto fallback_backside{ "__back.jpg"_p };
    const auto& default_card_name{ card_name.value_or(std::ref(fallback_backside)) };
    m_CardImage = new CardImage{
        default_card_name,
        project,
        CardImageWidgetParams{
            .m_Backside = backside,
            .m_MinimumWidth{ c_MinimumWidth },
        },
    };
    m_ClearImage = new BlankCardImage{
        m_Project,
        CardImageWidgetParams{
            .m_MinimumWidth{ c_MinimumWidth },
        },
    };

    addWidget(m_CardImage);
    addWidget(m_ClearImage);
    setCurrentWidget(card_name.has_value()
                         ? static_cast<QLabel*>(m_CardImage)
                         : static_cast<QLabel*>(m_ClearImage));

    setMinimumWidth(c_MinimumWidth.value);
    setMinimumHeight(heightForWidth(c_MinimumWidth.value));
    setMaximumWidth(c_MaximumWidth.value);
    setMaximumHeight(heightForWidth(c_MaximumWidth.value));

    QSizePolicy pm(QSizePolicy::Preferred, QSizePolicy::Minimum);
    pm.setHeightForWidth(true);
    setSizePolicy(pm);
}

void ClearableCardImage::Refresh(OptionalImageRef card_name,
                                 bool backside)
{
    TRACY_AUTO_SCOPE();

    const bool has_backside{ card_name.has_value() };
    if (has_backside)
    {
        const auto& default_card_name{ card_name.value() };
        m_CardImage->Refresh(default_card_name,
                             m_Project,
                             CardImageWidgetParams{
                                 .m_Backside = backside,
                                 .m_MinimumWidth{ c_MinimumWidth },
                             });
        setCurrentWidget(m_CardImage);
    }
    else
    {
        setCurrentWidget(m_ClearImage);
    }
}
