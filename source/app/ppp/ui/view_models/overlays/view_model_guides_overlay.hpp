#pragma once

#include <QObject>

#include <ppp/project/project_types.hpp>

class QPainterPath;

class Project;
struct PageImageTransform;

class GuidesOverlayViewModel : public QObject
{
    Q_OBJECT

  public:
    GuidesOverlayViewModel(const Project& project);

    bool ShouldDrawGuides() const;
    bool ShouldDrawCornerGuides() const;
    bool ShouldDrawCrossGuides() const;
    bool ShouldDrawExtendedGuides() const;

    Size GetPageSize() const;
    Length GetGuidesThickness() const;
    Length GetGuidesLength() const;
    Length GetGuidesOffset() const;
    Length GetBleedEdge() const;
    Length GetEnvelopeBleedEdge() const;

    void EmitDefaults();

  signals:
    void Redraw();
    void GuidesColorsChanged(ColorRGB8 color_a, ColorRGB8 color_b);

  private slots:
    void SomeGuidesColorChanged();

  private:
    const Project& m_Project;
};
