#pragma once

#include <QObject>

#include <ppp/util.hpp>

#include <ppp/ui/view_models/card_view_params.hpp>

class Project;

class BlankCardViewModel : public QObject
{
    Q_OBJECT

    friend class BlankCardImage;

  public:
    BlankCardViewModel(CardViewParams params, const Project& project);

    float GetCardAspectRatio() const;

  signals:
    void MinimumWidthChanged(Pixel minimum_width);

    void PixmapChanged(const QPixmap& pixmap);

  private:
    void EmitDefaults();

    CardViewParams m_ViewParams;

    const Project& m_Project;
};
