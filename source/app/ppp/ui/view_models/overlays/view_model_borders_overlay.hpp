#pragma once

#include <QObject>

#include <ppp/project/project_types.hpp>

class QPainterPath;

class Project;
struct PageImageTransform;

class BordersOverlayViewModel : public QObject
{
  public:
    BordersOverlayViewModel(const Project& project,
                            bool is_backside);

    bool ShouldDrawOuterBorder() const;
    bool ShouldDrawRoundedRect() const;
    bool ShouldDrawCardSvg() const;

    Size GetPageSize() const;
    Size GetCardsSize() const;
    Length GetCardCornerRadius() const;
    Margins GetPageMargins() const;

    void DrawCardSvg(QPainterPath& painter_path,
                     const PageImageTransform& transform,
                     Size size) const;

  private:
    const Project& m_Project;
    const bool m_IsBackside;
};
