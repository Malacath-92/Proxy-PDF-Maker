#pragma once

#include <QScrollArea>

#include <ppp/util.hpp>

class Project;

class CardScrollArea : public QScrollArea
{
  public:
    CardScrollArea(Project& project);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BacksideEnabledChanged();
    void BacksideDefaultChanged();
    void DisplayColumnsChanged();

    void CardAdded();
    void CardRemoved();
    void CardRenamed();

    void FullRefresh();

  private:
    int ComputeMinimumWidth();

    virtual void showEvent(QShowEvent* event) override;

    Project& m_Project;

    class CardGrid;
    CardGrid* m_Grid;
};
