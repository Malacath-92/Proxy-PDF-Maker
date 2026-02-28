#pragma once

#include <optional>

#include <ppp/pdf/util.hpp>

#include <ppp/ui/widget_card.hpp>

class PrintPreviewCardImage : public CardImage
{
    Q_OBJECT

  public:
    PrintPreviewCardImage(const fs::path& card_name,
                          const Project& project,
                          CardImageWidgetParams params,
                          size_t idx,
                          QWidget* companion,
                          std::optional<ClipRect> clip_rect,
                          Size widget_size);

    virtual void mousePressEvent(QMouseEvent* event) override;

    virtual void dropEvent(QDropEvent* event) override;

    virtual void enterEvent(QEnterEvent* event) override;
    virtual void dragEnterEvent(QDragEnterEvent* event) override;

    virtual void leaveEvent(QEvent* event) override;
    virtual void dragLeaveEvent(QDragLeaveEvent* event) override;

    virtual void paintEvent(QPaintEvent* event) override;

  signals:
    void DragStarted();
    void DragFinished();
    void ReorderCards(size_t form, size_t to);

  private:
    QRect GetClippedRect() const;

    void OnEnter();
    void OnLeave();

    size_t m_Index{ 0 };

    static inline constexpr int c_CompanionSizeDelta{ 4 };
    QWidget* m_Companion{ nullptr };

    std::optional<ClipRect> m_ClipRect{ std::nullopt };
    std::optional<QPixmap> m_ScaledPixmap;

    Size m_WidgetSize;
};
