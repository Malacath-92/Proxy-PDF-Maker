#pragma once

#include <QStackedWidget>

#include <ppp/project/project_types.hpp>

#include <ppp/ui/widget_util/card/widget_with_card_size.hpp>

class Project;

class CardImage;
class BlankCardImage;

class ClearableCardImage : public WidgetWithCardSize<QStackedWidget>
{
  public:
    ClearableCardImage(const Project& project,
                       OptionalImageRef card_name,
                       bool backside);

    void Refresh(OptionalImageRef card_name,
                 bool backside);

  private:
    inline static constexpr auto c_MinimumWidth{ 60_pix };
    inline static constexpr auto c_MaximumWidth{ 120_pix };

    const Project& m_Project;

    CardImage* m_CardImage{ nullptr };
    BlankCardImage* m_ClearImage{ nullptr };
};
