#pragma once

#include <QTimer>
#include <QWidget>

#include <ppp/util.hpp>

class QLineEdit;
class QPushButton;

class Project;
class CardScrollArea;

class CardArea : public QWidget
{
    Q_OBJECT

  public:
    CardArea(Project& project,
             uint32_t display_columns);

  public slots:
    void NewProjectOpened();
    void ImageDirChanged();
    void BacksideEnabledChanged();
    void BacksideDefaultChanged();
    void CardSizeChanged();
    void DisplayColumnsChanged(uint32_t display_columns);
    void CardOrderChanged();
    void CardOrderDirectionChanged();

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    void CardVisibilityChanged(const fs::path& card_name, bool visible);

    void FullRefresh();

    int MaximumColumnsFromAvailableWidth(int available_width) const;

  signals:
    void RequestOpenPluginsWindow();

  private:
    const Project& m_Project;
    uint32_t m_DisplayColumns;

    QWidget* m_OnboardingHint;

    QWidget* m_Header;
    QPushButton* m_RemoveExternalCards;
    QLineEdit* m_Filter;
    CardScrollArea* m_ScrollArea;

    // We use a timer whenever we do a full refresh
    // to avoid cases where we get multiple requests
    // in quick succession
    QTimer m_RefreshTimer;
};
