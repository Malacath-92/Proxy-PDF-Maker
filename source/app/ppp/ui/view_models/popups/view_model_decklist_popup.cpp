#include <ppp/ui/view_models/popups/view_model_decklist_popup.hpp>

#include <ppp/project/project.hpp>

#include <ppp/qt_util.hpp>

DecklistPopupViewModel::DecklistPopupViewModel(Project& project)
    : m_Project{ project }
{
}

QList<QString> DecklistPopupViewModel::GetAllCards() const
{
    return m_Project.GetCards() |
           std::views::filter(std::not_fn(&CardInfo::m_Transient)) |
           std::views::filter(std::not_fn(&CardInfo::m_Hidden)) |
           std::views::transform(&CardInfo::m_Name) |
           std::views::transform(&fs::path::filename) |
           std::views::transform(QOverload<const fs::path&>::of(&ToQString)) |
           std::ranges::to<QList>();
}

QList<DecklistPopupViewModel::CardInList> DecklistPopupViewModel::GetCardsInList() const
{
    const auto make_card_in_list{
        [](const auto& card)
        {
            return CardInList{
                ToQString(card.m_Name.filename()),
                card.m_Num,
            };
        }
    };
    return m_Project.GetCards() |
           std::views::filter(std::not_fn(&CardInfo::m_Transient)) |
           std::views::filter(std::not_fn(&CardInfo::m_Hidden)) |
           std::views::filter(&CardInfo::m_Num) |
           std::views::transform(make_card_in_list) |
           std::ranges::to<QList>();
}

bool DecklistPopupViewModel::HasCard(const QString& card) const
{
    const fs::path& card_name{ card.toStdString() };
    return m_Project.HasCard(card_name) || m_Project.HasCardByStem(card_name);
}

void DecklistPopupViewModel::ChangeDecklist(const std::unordered_map<fs::path, uint32_t>& decklist)
{
    for (const auto& card : m_Project.m_Data.m_Cards)
    {
        const auto card_count{
            [&]()
            {
                auto it{ decklist.find(card.m_Name) };
                if (it == decklist.end())
                {
                    it = decklist.find(card.Stem());
                }

                if (it != decklist.end())
                {
                    return it->second;
                }
                else
                {
                    return 0u;
                }
            }()
        };

        m_Project.SetCardCount(card.m_Name, card_count);
    }
}
