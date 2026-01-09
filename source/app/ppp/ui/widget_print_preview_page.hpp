#pragma once

#include <QWidget>

#include <ppp/pdf/util.hpp>

class Project;
class PrintPreviewCardImage;
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
    };
    PagePreview(Project& project,
                QObject* event_filter,
                const Page& page,
                const PageImageTransforms& transforms,
                Params params);

    virtual bool hasHeightForWidth() const override;

    virtual int heightForWidth(int width) const override;

    virtual void resizeEvent(QResizeEvent* event) override;

  signals:
    void DragStarted();
    void DragFinished();
    void ReorderCards(size_t form, size_t to);

  private:
    const PageImageTransforms& m_Transforms;

    Size m_PageSize;
    float m_PageRatio;

    QWidget* m_ImageContainer{ nullptr };
    std::vector<PrintPreviewCardImage*> m_Images;

    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };
    MarginsOverlay* m_Margins{ nullptr };
};
