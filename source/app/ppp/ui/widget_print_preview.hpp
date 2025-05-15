#pragma once

#include <QScrollArea>

class Project;

class PrintPreview : public QScrollArea
{
  public:
    PrintPreview(const Project& project);

    void Refresh();

  private:
    class PagePreview;

    const Project& m_Project;
};
