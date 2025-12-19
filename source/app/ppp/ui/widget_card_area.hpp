#pragma once

#include <QWidget>
#include <QTimer>

#include <ppp/util.hpp>

class Project;
class CardScrollArea;

class CardArea : public QWidget
{
    Q_OBJECT

  public:
    CardArea(Project& project);

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
    QWidget* m_OnboardingHint;

    QWidget* m_Header;
    CardScrollArea* m_ScrollArea;

    // We use a timer whenever we do a full refresh
    // to avoid cases where we get multiple requests
    // in quick succession
    QTimer m_RefreshTimer;
};
