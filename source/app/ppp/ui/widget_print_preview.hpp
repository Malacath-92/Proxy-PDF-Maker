#pragma once

#include <optional>

#include <QScrollArea>
#include <QTimer>

#include <ppp/pdf/util.hpp>

class Project;
class PagePreview;

class PrintPreview : public QScrollArea
{
    Q_OBJECT

  public:
    PrintPreview(Project& project);

    void Refresh();
    void RequestRefresh();

    void CardOrderChanged();
    void CardOrderDirectionChanged();

    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    virtual void dragEnterEvent(QDragEnterEvent* event) override;
    virtual void dragMoveEvent(QDragMoveEvent* event) override;

  signals:
    void RestoreCardsOrder();
    void ReorderCards(size_t from, size_t to);

  private:
    void GoToPage(uint32_t page);

    const PagePreview* GetNthPage(uint32_t n) const;

    int ComputeDragScrollDiff() const;

    Project& m_Project;

    bool m_Dragging{ false };
    bool m_DraggingStarted{ false };
    QTimer m_DragScrollTimer{};
    float m_DragScrollAlpha{ 0.5f };
    static inline constexpr int c_DragScrollSpeed{ 15 };

    PageImageTransforms m_FrontsideTransforms;
    PageImageTransforms m_BacksideTransforms;

    // We use a timer whenever we do a full refresh
    // to avoid cases where we get multiple requests
    // in quick succession
    QTimer m_RefreshTimer;

    std::optional<uint32_t> m_TargetPage{ std::nullopt };
    QTimer m_NumberTypeTimer;
};
