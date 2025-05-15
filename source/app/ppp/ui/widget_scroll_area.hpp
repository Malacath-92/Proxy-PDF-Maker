#pragma once

#include <QScrollArea>

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

  private:
    void FullRefresh();
    int ComputeMinimumWidth();

    virtual void showEvent(QShowEvent* event) override;

    Project& m_Project;

    class CardGrid;
    CardGrid* Grid;
};
