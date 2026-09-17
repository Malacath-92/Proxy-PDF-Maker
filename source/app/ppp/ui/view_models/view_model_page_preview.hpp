#pragma once

#include <optional>

#include <QObject>

#include <ppp/util.hpp>

class Project;
class Config;

class CardViewModel;
class GuidesOverlayViewModel;
class BordersOverlayViewModel;
class MarginsOverlayViewModel;

class PagePreviewViewModel : public QObject
{
    Q_OBJECT
  public:
    PagePreviewViewModel(Project& project,
                         const Config& config,
                         bool is_backside);

    CardViewModel* MakeCardViewModel(const fs::path& card_name,
                                     Rotation rotation) const;

    GuidesOverlayViewModel* MakeGuidesOverlayViewModel() const;
    BordersOverlayViewModel* MakeBordersOverlayViewModel(bool is_backside) const;
    MarginsOverlayViewModel* MakeMarginsOverlayViewModel(bool is_backside) const;

    std::optional<fs::path> GetBasePdfPath() const;

    Size GetPageSize() const;
    Length GetBleedEdge() const;
    bool HasRoundedCorners() const;
    bool IsBackside() const;

    bool ShowExactBorders() const;
    bool ShowMargins() const;

  public slots:
    void ReorderCards(size_t from, size_t to);
    void SkipSlot(size_t slot);

  private:
    Project& m_Project;
    const Config& m_Cfg;
    const bool m_IsBackside;
};
