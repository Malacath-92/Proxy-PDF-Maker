#pragma once

#include <QObject>

#include <ppp/util.hpp>

#include <ppp/ui/view_models/card_view_params.hpp>

class Project;

class BlankCardViewModel : public QObject
{
    Q_OBJECT

  public:
    BlankCardViewModel(CardViewParams params, const Project& project);

    float GetCardAspectRatio() const;

    void EmitDefaults(bool with_pixmap = true);

  public slots:
    void CardSizeChanged(Size card_size);

  signals:
    void CardAspectRatioChanged(float aspect_ratio);

    void MinimumWidthChanged(Pixel minimum_width);

    void PixmapChanged(const QPixmap& pixmap);

  private:
    CardViewParams m_ViewParams;

    const Project& m_Project;
};
