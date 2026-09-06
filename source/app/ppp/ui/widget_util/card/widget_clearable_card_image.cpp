#include <ppp/ui/widget_util/card/widget_clearable_card_image.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>
#include <ppp/ui/widget_util/card/widget_blank_card_image.hpp>
#include <ppp/ui/widget_util/card/widget_card_image.hpp>

#include <ppp/ui/view_models/view_model_card.hpp>

#include <ppp/profile/profile.hpp>

ClearableCardImage::ClearableCardImage(CardViewModel* card_view_model,
                                       BlankCardViewModel* blank_view_model,
                                       bool clear)
    : WidgetWithCardSize{ card_view_model->GetCardAspectRatio() }
    , m_CardViewModel{ card_view_model }
{
    TRACY_AUTO_SCOPE();

    m_CardImage = new CardImage{ card_view_model };
    m_ClearImage = new BlankCardImage{ blank_view_model };

    addWidget(m_CardImage);
    addWidget(m_ClearImage);
    setCurrentWidget(clear
                         ? static_cast<QLabel*>(m_ClearImage)
                         : static_cast<QLabel*>(m_CardImage));

    setMinimumWidth(c_MinimumWidth.value);
    setMinimumHeight(heightForWidth(c_MinimumWidth.value));
    setMaximumWidth(c_MaximumWidth.value);
    setMaximumHeight(heightForWidth(c_MaximumWidth.value));

    QSizePolicy pm(QSizePolicy::Preferred, QSizePolicy::Minimum);
    pm.setHeightForWidth(true);
    setSizePolicy(pm);
}

void ClearableCardImage::Clear()
{
    TRACY_AUTO_SCOPE();
    setCurrentWidget(m_ClearImage);
}
void ClearableCardImage::SetCardName(const fs::path& card_name)
{
    TRACY_AUTO_SCOPE();
    m_CardViewModel->SetCardName(card_name);
    setCurrentWidget(m_CardImage);
}
