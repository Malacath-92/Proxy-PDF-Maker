#pragma once

#include <QLabel>

#include <ppp/ui/widget_util/card/card_widget_params.hpp>
#include <ppp/ui/widget_util/card/widget_with_card_size.hpp>

class Project;

class BlankCardImage : public WidgetWithCardSize<QLabel>
{
  public:
    BlankCardImage(const Project& project,
                   CardImageWidgetParams params = CardImageWidgetParams{});
};
