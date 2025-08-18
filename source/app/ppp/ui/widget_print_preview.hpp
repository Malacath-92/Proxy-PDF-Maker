#pragma once

#include <QScrollArea>

#include <ppp/pdf/util.hpp>

class Project;

class PrintPreview : public QScrollArea
{
  public:
    PrintPreview(const Project& project);

    void Refresh();

  private:
    class PagePreview;

    const Project& m_Project;

    PageImageTransforms m_FrontsideTransforms;
    PageImageTransforms m_BacksideTransforms;
};
