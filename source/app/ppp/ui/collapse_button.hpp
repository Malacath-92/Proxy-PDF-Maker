#pragma once

#include <QParallelAnimationGroup>
#include <QToolButton>

class CollapseButton : public QToolButton
{
    Q_OBJECT

  public:
    CollapseButton(QWidget* handled_widget, bool collapsed);

    void Uncheck();
    void Check();

  signals:
    void SetObjectVisibility(bool visible);

  private:
    QWidget* m_HandledWidget;
    QParallelAnimationGroup m_Animator;
};
