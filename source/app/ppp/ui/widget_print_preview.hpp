#pragma once

#include <QScrollArea>

struct Project;

class PrintPreview : public QScrollArea
{
  public:
    PrintPreview(const Project& project);

    void Refresh(const Project& project);

  private:
    class PagePreview;
};
