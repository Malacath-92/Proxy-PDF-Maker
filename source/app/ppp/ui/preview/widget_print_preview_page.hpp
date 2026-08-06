#pragma once

#include <QWidget>

#include <ppp/pdf/util.hpp>

class Project;
class PageImageContainer;
class GuidesOverlay;
class BordersOverlay;
class MarginsOverlay;

class PagePreview : public QWidget
{
    Q_OBJECT

  public:
    struct Params
    {
        Size m_PageSize;
        bool m_IsBackside;
        bool m_NoCropMode;
    };
    PagePreview(Project& project,
                QObject* event_filter,
                const Page& page,
                const PageImageTransforms& transforms,
                Params params);

    virtual void resizeEvent(QResizeEvent* event) override;

  signals:
    void DragStarted();
    void DragFinished();
    void ReorderCards(size_t form, size_t to);
    void RequestRefresh();

  private:
    PageImageContainer* m_ImageContainer{ nullptr };
    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };
    MarginsOverlay* m_Margins{ nullptr };
};
