#pragma once

#include <QScrollArea>

struct Project;

class CardScrollArea : public QScrollArea
{
  public:
    CardScrollArea(Project& project);

    void Refresh(Project& project);

  private:
    int ComputeMinimumWidth();

    virtual void showEvent(QShowEvent* event) override;

    class CardGrid;
    CardGrid* Grid;
};
