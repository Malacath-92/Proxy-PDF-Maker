#pragma once

#include <QScrollArea>

class Project;

class PrintPreview : public QScrollArea
{
  public:
    PrintPreview(const Project& project);

    void Refresh(const Project& project);

  private:
    class PagePreview;
};
