#pragma once

#include <QScrollArea>

#include <ppp/pdf/util.hpp>

class Project;

class PrintPreview : public QScrollArea
{
    Q_OBJECT

  public:
    PrintPreview(const Project& project);

    void Refresh();

  signals:
    void ReorderCards(size_t from, size_t to);

  private:
    class PagePreview;

    const Project& m_Project;

    PageImageTransforms m_FrontsideTransforms;
    PageImageTransforms m_BacksideTransforms;
};
