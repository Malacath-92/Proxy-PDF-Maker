#pragma once

#include <QScrollArea>

#include <ppp/util.hpp>

class Project;

class CardScrollArea : public QScrollArea
{
    Q_OBJECT

  public:
    CardScrollArea(Project& project);

  signals:
    void CardRotationChanged(const fs::path& card_name);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BacksideEnabledChanged();
    void BacksideDefaultChanged();
    void DisplayColumnsChanged();

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    void FullRefresh();

  private:
    int ComputeMinimumWidth();

    virtual void showEvent(QShowEvent* event) override;

    Project& m_Project;

    class CardGrid;
    CardGrid* m_Grid;
};
