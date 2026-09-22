#pragma once

#include <QWidget>

#include <ppp/pdf/util.hpp>

class PageHeader;
class PageImageContainer;
class GuidesOverlay;
class BordersOverlay;
class MarginsOverlay;

class PagePreviewViewModel;

class PagePreview : public QWidget
{
    Q_OBJECT

  public:
    PagePreview(PagePreviewViewModel* view_model,
                QObject* event_filter);

    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

  signals:
    void DragStarted();
    void DragFinished();

  private slots:
    void PageHeaderEnabledChanged(bool page_header_enabled);

  private:
    const PagePreviewViewModel& m_ViewModel;

    PageImageContainer* m_ImageContainer{ nullptr };
    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };
    MarginsOverlay* m_Margins{ nullptr };
    PageHeader* m_Header{ nullptr };
};
