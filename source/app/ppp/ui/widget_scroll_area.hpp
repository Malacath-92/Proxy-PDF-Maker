#pragma once

#include <QScrollArea>
#include <QTimer>

#include <ppp/util.hpp>

class Project;

class CardScrollArea : public QScrollArea
{
    Q_OBJECT

  public:
    CardScrollArea(Project& project);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BacksideEnabledChanged();
    void BacksideDefaultChanged();
    void DisplayColumnsChanged();
    void CardOrderChanged();
    void CardOrderDirectionChanged();

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    void CardVisibilityChanged(const fs::path& card_name, bool visible);

    void FullRefresh();

  private:
    int ComputeMinimumWidth();

    virtual void showEvent(QShowEvent* event) override;

    Project& m_Project;

    QWidget* m_Header;

    class CardGrid;
    CardGrid* m_Grid;

    QWidget* m_OnboardingHint;

    // We use a timer whenever we do a full refresh
    // to avoid cases where we get multiple requests
    // in quick succession
    QTimer m_RefreshTimer;
};
