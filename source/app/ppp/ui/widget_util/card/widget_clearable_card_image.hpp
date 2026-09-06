#pragma once

#include <QStackedWidget>

#include <ppp/project/project_types.hpp>

#include <ppp/ui/widget_util/card/widget_with_card_size.hpp>

class CardViewModel;
class BlankCardViewModel;

class CardImage;
class BlankCardImage;

class ClearableCardImage : public WidgetWithCardSize<QStackedWidget>
{
  public:
    ClearableCardImage(CardViewModel* card_view_model,
                       BlankCardViewModel* blank_view_model,
                       bool clear);

    void Clear();
    void SetCardName(const fs::path& card_name);

  private:
    inline static constexpr auto c_MinimumWidth{ 60_pix };
    inline static constexpr auto c_MaximumWidth{ 120_pix };

    CardViewModel* m_CardViewModel{ nullptr };
    CardImage* m_CardImage{ nullptr };
    BlankCardImage* m_ClearImage{ nullptr };
};
