#pragma once

#include <QParallelAnimationGroup>
#include <QToolButton>

class QPropertyAnimation;

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
    QPropertyAnimation* m_Animation;
    QParallelAnimationGroup m_Animator;
};
