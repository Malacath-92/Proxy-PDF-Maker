#pragma once

#include <QWidget>

#include <ppp/pdf/util.hpp>

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
                QObject* event_filter,
                const Page& page,
                const PageImageTransforms& transforms);

    virtual void resizeEvent(QResizeEvent* event) override;

  signals:
    void DragStarted();
    void DragFinished();

  private:
    PageImageContainer* m_ImageContainer{ nullptr };
    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };
    MarginsOverlay* m_Margins{ nullptr };
};
