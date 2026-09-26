#pragma once

#include <QParallelAnimationGroup>
#include <QToolButton>
#include <QWidgetItem>

class QPropertyAnimation;

class CollapsibleLayoutItem : public QObject, public QWidgetItem
{
    Q_OBJECT
    Q_PROPERTY(float fadeOut READ fadeOut WRITE setFadeOut)

  public:
    CollapsibleLayoutItem(QWidget* widget);

    float fadeOut() const;
    void setFadeOut(float fade_out);

  private:
    virtual QSize sizeHint() const override;
    virtual QSize minimumSize() const override;
    virtual QSize maximumSize() const override;
    QSize faded(QSize size) const;

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

    virtual void setGeometry(const QRect& rect) override;

    float m_FadeOut{ 1.0f };
};

class CollapseButton : public QToolButton
{
    Q_OBJECT

  public:
    CollapseButton(QWidget* handled_widget, bool collapsed);

    QWidgetItem* GetLayoutItem();

    void Uncheck();
    void Check();

  signals:
    void SetObjectVisibility(bool visible);

  private:
    QWidget* m_HandledWidget;
    CollapsibleLayoutItem* m_LayoutItem;
    QPropertyAnimation* m_Animation;
    QParallelAnimationGroup m_Animator;
};
