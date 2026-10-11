#pragma once

#include <QWidget>

#include <ppp/util.hpp>

class CollapseContainer : public QWidget
{
    Q_OBJECT

  public:
    CollapseContainer(QWidget* handled_widget, bool collapsed);

    QWidget* GetHandledWidget() const;
    void ReleaseHandledWidget();

    using RelativeRate = decltype(1_pix / 1_s);
    void SetRelativeRate(RelativeRate rate);
    void SetRate(dla::time_unit rate);

  signals:
    void SetObjectVisibility(bool visible);

  private:
    QWidget* m_HandledWidget;
    class CollapseButton* m_CollapseButton;
};
