#pragma once

#include <optional>

#include <QObject>

#include <ppp/pdf/util.hpp>
#include <ppp/util.hpp>

class Project;
class Config;

class CardViewModel;
class GuidesOverlayViewModel;
class BordersOverlayViewModel;
class MarginsOverlayViewModel;

struct PagePreviewData
{
    Page m_Page;
    const PageImageTransforms& m_Transforms;
    size_t m_PageIndex;
    size_t m_TotalPages;
    bool m_IsBackside;
};

class PagePreviewViewModel : public QObject
{
    Q_OBJECT
  public:
    PagePreviewViewModel(Project& project,
                         const Config& config,
                         PagePreviewData page);

    CardViewModel* MakeCardViewModel(const fs::path& card_name,
                                     Rotation rotation) const;

    GuidesOverlayViewModel* MakeGuidesOverlayViewModel() const;
    BordersOverlayViewModel* MakeBordersOverlayViewModel(bool is_backside) const;
    MarginsOverlayViewModel* MakeMarginsOverlayViewModel(bool is_backside) const;

    QString GetPageName() const;
    Length GetHeaderSpace() const;

    const PageImageTransforms& GetTransforms() const;
    std::span<const PageImage> GetImages() const;

    Size GetPageSize() const;
    Length GetBleedEdge() const;
    bool HasRoundedCorners() const;
    bool IsBackside() const;

    void EmitDefaults();

  signals:
    // forward

    // TODO: Shift the background
    void PageBackgroundChanged(const QPixmap& background);

    void OutputFilenameChanged(const fs::path& output_filename);
    void PageHeaderEnabledChanged(bool page_header_enabled);

  public slots:
    void ReorderCards(size_t from, size_t to);
    void SkipSlot(size_t slot);

  private:
    Project& m_Project;
    const Config& m_Cfg;

    PagePreviewData m_Page;
};
