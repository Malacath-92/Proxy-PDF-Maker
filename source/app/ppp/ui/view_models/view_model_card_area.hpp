#pragma once

#include <QObject>

#include <ppp/config_types.hpp>
#include <ppp/project/project_types.hpp>
#include <ppp/util.hpp>

class Config;
class Project;
struct ProjectData;

class CardAreaCardViewModel;

class CardAreaViewModel : public QObject
{
    Q_OBJECT

    friend class CardArea;
    friend class CardAreaCardWidget;

  public:
    CardAreaViewModel(Project& project,
                      const Config& config);

    CardAreaCardViewModel* MakeCardViewModel(const fs::path& card_name) const;

    const CardContainer& GetCards() const;

    uint32_t GetDisplayColumns() const;
    QString GetImageDir() const;

  signals:
    // forward

    void DisplayColumnsChanged(uint32_t display_columns);
    void CardOrderChanged(CardOrder card_order);
    void CardOrderDirectionChanged(CardOrderDirection card_order_direction);

    void NewProjectOpened(const ProjectData& old_data, const ProjectData& new_data);
    void ImageDirChanged(const fs::path& old_path, const fs::path& new_path);

    void HasExternalCardsChanged(bool has_external_cards);

    void CardVisibilityChanged(const fs::path& card_name, bool visible);

    void CardAdded(const fs::path& card_name);
    void CardRemoved(const fs::path& card_name);
    void CardRenamed(const fs::path& old_card_name, const fs::path& new_card_name);

    void CardSortingChanged();

  signals:
    void RequestRefresh();

  private slots:
    void DecrementAllCards();
    void IncrementAllCards();
    void ResetAllCards();

    void RemoveAllExternalCards();

  private:
    void EmitDefaults();

    Project& m_Project;
    const Config& m_Cfg;
};
