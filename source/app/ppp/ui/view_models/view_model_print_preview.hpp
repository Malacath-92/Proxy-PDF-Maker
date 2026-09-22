#pragma once

#include <QObject>
#include <QPixmap>
#include <QTimer>

#include <ppp/pdf/util.hpp>

class Project;
class Config;

class PagePreviewViewModel;

class PrintPreviewViewModel : public QObject
{
    Q_OBJECT
  public:
    PrintPreviewViewModel(Project& project,
                          const Config& config);

    PagePreviewViewModel* MakePagePreviewViewModel(
        Page page,
        const PageImageTransforms& transforms,
        size_t page_index,
        size_t total_pages,
        bool is_backside) const;

    void ImmediateRefresh();
    void QueueRefresh();

    bool HasBacksides() const;

    std::vector<Page> GetFrontsidePages() const;
    std::vector<PageImageTransform> GetFrontsideTransforms() const;

    std::vector<Page> GetBacksidePages(const std::vector<Page>& frontside_pages) const;
    std::vector<PageImageTransform> GetBacksideTransforms(const std::vector<PageImageTransform>& frontside_transforms) const;

    void RestoreCardsOrder();
    void ReorderCards(size_t from, size_t to);

    void RestoreAllSlots();

    void EmitDefaults();

  signals:
    // forward

    void RequestRefresh();

    void CardsManuallySortedChanged(bool is_manually_sorted);
    void SlotsSkippedChanged(bool slots_skipped);

    void PageBackgroundChanged(const QPixmap& background);

  public slots:
    void CardOrderChanged();
    void CardOrderDirectionChanged();

    void RenderSortingChanged();

    void SkippedSlotsChanged(std::span<const size_t> skipped_slots);

    void PageSizeChanged();
    void BasePdfChanged();
    void UnderlayPdfChanged();

  private:
    void RenderPageBackground();

    Project& m_Project;
    const Config& m_Cfg;

    QPixmap m_PageBackground;
    QTimer m_RefreshTimer;
};
