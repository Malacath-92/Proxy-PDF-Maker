#pragma once

#include <optional>

#include <QScrollArea>
#include <QTimer>

#include <ppp/pdf/util.hpp>

class Project;

class PrintPreview : public QScrollArea
{
    Q_OBJECT

  public:
    PrintPreview(Project& project);

    void Refresh();
    void RequestRefresh();

    void CardOrderChanged();
    void CardOrderDirectionChanged();

    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;

  signals:
    void RestoreCardsOrder();
    void ReorderCards(size_t from, size_t to);

  private:
    void GoToPage(uint32_t page);

    class PagePreview;
    const PagePreview* GetNthPage(uint32_t n) const;

    Project& m_Project;

    QWidget* m_ScrollUpWidget{ nullptr };
    QWidget* m_ScrollDownWidget{ nullptr };

    QTimer m_ScrollTimer{};
    int m_ScrollSpeed{ 40 };

    PageImageTransforms m_FrontsideTransforms;
    PageImageTransforms m_BacksideTransforms;

    // We use a timer whenever we do a full refresh
    // to avoid cases where we get multiple requests
    // in quick succession
    QTimer m_RefreshTimer;

    std::optional<uint32_t> m_TargetPage{ std::nullopt };
    QTimer m_NumberTypeTimer;
};
