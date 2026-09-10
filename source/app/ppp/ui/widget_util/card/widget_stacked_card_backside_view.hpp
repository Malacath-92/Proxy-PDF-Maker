#pragma once

#include <QStackedWidget>

#include <ppp/project/project_types.hpp>

#include <ppp/ui/widget_util/card/widget_with_card_size.hpp>

class CardImage;
class ClearableCardImage;

class BlankCardViewModel;

class StackedCardBacksideView : public WidgetWithCardSize<QStackedWidget>
{
    Q_OBJECT

  public:
    StackedCardBacksideView(BlankCardViewModel* view_model,
                            CardImage* image,
                            ClearableCardImage* backside);

    void RefreshBackside(OptionalImageRef backside);

  signals:
    void BacksideClicked();

  private slots:
    void CardAspectRatioChanged(float aspect_ratio);

    void MinimumWidthChanged(Pixel minimum_width);

  private:
    void RefreshSizes(QSize size);

    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    BlankCardViewModel& m_ViewModel;

    CardImage* m_Image;

    QWidget* m_BacksideContainer;
    ClearableCardImage* m_Backside;
};
