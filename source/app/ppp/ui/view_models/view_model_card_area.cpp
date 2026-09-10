#include <ppp/ui/view_models/view_model_card_area.hpp>

#include <ppp/config.hpp>

#include <ppp/project/project.hpp>

#include <ppp/qt_util.hpp>

#include <ppp/ui/view_models/view_model_card_area_card.hpp>

#include <ppp/profile/profile.hpp>

CardAreaViewModel::CardAreaViewModel(Project& project,
                                     const Config& config)
    : m_Project{ project }
    , m_Cfg{ config }
{
    TRACY_AUTO_SCOPE();

    QObject::connect(this, &CardAreaViewModel::DisplayColumnsChanged, &CardAreaViewModel::RequestRefresh);
    QObject::connect(this, &CardAreaViewModel::CardOrderChanged, &CardAreaViewModel::RequestRefresh);
    QObject::connect(this, &CardAreaViewModel::CardOrderDirectionChanged, &CardAreaViewModel::RequestRefresh);
    QObject::connect(this, &CardAreaViewModel::NewProjectOpened, &CardAreaViewModel::RequestRefresh);
    QObject::connect(this, &CardAreaViewModel::ImageDirChanged, &CardAreaViewModel::RequestRefresh);
    QObject::connect(this, &CardAreaViewModel::CardSortingChanged, &CardAreaViewModel::RequestRefresh);
}

CardAreaCardViewModel* CardAreaViewModel::MakeCardViewModel(const fs::path& card_name) const
{
    return new CardAreaCardViewModel{ card_name, m_Project };
}

const CardContainer& CardAreaViewModel::GetCards() const
{
    return m_Project.GetCards();
}

void CardAreaViewModel::DecrementAllCards()
{
    m_Project.DecrementAllCardCounts();
}
void CardAreaViewModel::IncrementAllCards()
{
    m_Project.IncrementAllCardCounts();
}
void CardAreaViewModel::ResetAllCards()
{
    m_Project.ResetAllCardCounts();
}

void CardAreaViewModel::RemoveAllExternalCards()
{
    m_Project.RemoveAllExternalCards();
}

void CardAreaViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    HasExternalCardsChanged(m_Project.HasExternalCards());
}

uint32_t CardAreaViewModel::GetDisplayColumns() const
{
    return m_Cfg.m_DisplayColumns;
}

QString CardAreaViewModel::GetImageDir() const
{
    return ToQString(m_Project.m_Data.m_ImageDir);
}
