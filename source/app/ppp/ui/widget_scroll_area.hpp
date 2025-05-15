#pragma once

#include <QScrollArea>

class Project;

class CardScrollArea : public QScrollArea
{
  public:
    CardScrollArea(Project& project);

    void Refresh();

  private:
    int ComputeMinimumWidth();

    virtual void showEvent(QShowEvent* event) override;

    Project& m_Project;

    class CardGrid;
    CardGrid* Grid;
};
