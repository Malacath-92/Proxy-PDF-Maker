#pragma once

#include <QWidget>
#include <QTimer>

#include <ppp/util.hpp>

#include <ppp/project/project_types.hpp>

class QLineEdit;
class QPushButton;

class CardScrollArea;

class CardAreaViewModel;

class CardArea : public QWidget
{
    Q_OBJECT

  public:
    CardArea(CardAreaViewModel* view_model);

    int MaximumColumnsFromAvailableWidth(int available_width) const;

  private slots:
    void CardSizeChanged(Size card_size);

    void HasExternalCardsChanged(bool has_external_cards);

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    void CardVisibilityChanged(const fs::path& card_name, bool visible);

  signals:
    void RequestOpenPluginsWindow();

  private:
    void FullRefresh();

    void QueueRefresh();
  
    CardAreaViewModel& m_ViewModel;

    QTimer m_RefreshTimer;

    QWidget* m_OnboardingHint;

    QWidget* m_Header;
    QPushButton* m_RemoveExternalCards;
    QLineEdit* m_Filter;
    CardScrollArea* m_ScrollArea;
};
