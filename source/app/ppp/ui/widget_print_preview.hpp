#pragma once

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

  signals:
    void RestoreCardsOrder();
    void ReorderCards(size_t from, size_t to);

  private:
    class PagePreview;

    Project& m_Project;

    QWidget* m_ScrollUpWidget{ nullptr };
    QWidget* m_ScrollDownWidget{ nullptr };

    QTimer m_ScrollTimer{};
    int m_ScrollSpeed{ 4 };

    PageImageTransforms m_FrontsideTransforms;
    PageImageTransforms m_BacksideTransforms;

    // We use a timer whenever we do a full refresh
    // to avoid cases where we get multiple requests
    // in quick succession
    QTimer m_RefreshTimer;
};
