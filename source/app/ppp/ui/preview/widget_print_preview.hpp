#pragma once

#include <optional>

#include <QScrollArea>
#include <QTimer>

#include <ppp/pdf/util.hpp>

class QPushButton;

class PagePreview;

class PrintPreviewViewModel;

class PrintPreview : public QScrollArea
{
    Q_OBJECT

  public:
    PrintPreview(PrintPreviewViewModel* view_model);

    void Refresh();

    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;

  private slots:
    void RequestRefresh();

    void CardsManuallySortedChanged(bool is_manually_sorted);
    void SlotsSkippedChanged(bool slots_skipped);

  private:
    void GoToPage(uint32_t page);

    const PagePreview* GetNthPage(uint32_t n) const;

    int ComputeDragScrollDiff() const;

    PrintPreviewViewModel& m_ViewModel;

    bool m_Dragging{ false };
    bool m_DraggingStarted{ false };
    QTimer m_DragScrollTimer{};
    float m_DragScrollAlpha{ 0.5f };
    static inline constexpr int c_DragScrollSpeed{ 15 };

    PageImageTransforms m_FrontsideTransforms;
    PageImageTransforms m_BacksideTransforms;

    std::optional<uint32_t> m_TargetPage{ std::nullopt };
    QTimer m_NumberTypeTimer;

    QPushButton* m_RestoreCardOrder;
    QPushButton* m_RestoreAllSlots;
};
