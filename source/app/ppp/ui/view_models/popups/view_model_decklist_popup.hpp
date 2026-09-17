#pragma once

#include <unordered_map>

#include <QObject>

#include <ppp/util.hpp>

class Project;

class DecklistPopupViewModel : public QObject
{
  public:
    DecklistPopupViewModel(Project& project);

    QList<QString> GetAllCards() const;

    struct CardInList
    {
        QString m_Name;
        uint32_t m_Num;
    };
    QList<CardInList> GetCardsInList() const;

    bool HasCard(const QString& card) const;

  public slots:
    void ChangeDecklist(const std::unordered_map<fs::path, uint32_t>& decklist);

  private:
    Project& m_Project;
};
