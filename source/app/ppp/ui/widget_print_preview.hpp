#pragma once

#include <QScrollArea>

struct Project;

class PrintPreview : public QScrollArea
{
  public:

    void Refresh(const Project& project);
};
