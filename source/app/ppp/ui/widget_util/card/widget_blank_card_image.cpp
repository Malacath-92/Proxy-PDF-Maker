#include <ppp/ui/widget_util/card/widget_blank_card_image.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>

#include <ppp/ui/view_models/util.hpp>
#include <ppp/ui/view_models/view_model_blank_card.hpp>

#include <ppp/profile/profile.hpp>

BlankCardImage::BlankCardImage(BlankCardViewModel* view_model)
    : WidgetWithCardSize{ view_model->GetCardAspectRatio() }
    , m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    m_ViewModel.setParent(this);

    setStyleSheet("QLabel{ background-color: transparent; }");
    setScaledContents(true);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(MinimumWidthChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PixmapChanged);

    m_ViewModel.EmitDefaults();
}

void BlankCardImage::MinimumWidthChanged(Pixel minimum_width)
{
    setMinimumWidth(minimum_width / 1_pix);
}

void BlankCardImage::PixmapChanged(const QPixmap& pixmap)
{
    setPixmap(pixmap);
}
