#pragma once

#include <QLabel>

#include <ppp/ui/widget_util/card/card_widget_params.hpp>
#include <ppp/ui/widget_util/card/widget_with_card_size.hpp>

class Project;
class BlankCardViewModel;

class BlankCardImage : public WidgetWithCardSize<QLabel>
{
  public:
    BlankCardImage(BlankCardViewModel* view_model);

  private slots:
    void MinimumWidthChanged(Pixel minimum_width);

    void PixmapChanged(const QPixmap& pixmap);

  private:
    BlankCardViewModel& m_ViewModel;
};
