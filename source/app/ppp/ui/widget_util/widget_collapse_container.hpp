#pragma once

#include <QWidget>

class CollapseContainer : public QWidget
{
    Q_OBJECT

  public:
    CollapseContainer(QWidget* handled_widget, bool collapsed);

    QWidget* GetHandledWidget() const;
    void ReleaseHandledWidget();

  signals:
    void SetObjectVisibility(bool visible);

  private:
    QWidget* m_HandledWidget;
    class CollapseButton* m_CollapseButton;
};
