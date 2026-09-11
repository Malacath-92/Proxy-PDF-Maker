#include <ppp/ui/view_models/view_model_card_area_card.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/view_models/popups/view_model_image_browse_popup.hpp>
#include <ppp/ui/view_models/view_model_blank_card.hpp>
#include <ppp/ui/view_models/view_model_card.hpp>

#include <ppp/profile/profile.hpp>

CardAreaCardViewModel::CardAreaCardViewModel(const fs::path& card_name,
                                             Project& project)
    : m_CardName{ card_name }
    , m_Project{ project }
{
    TRACY_AUTO_SCOPE();

    QObject::connect(&m_Project,
                     &Project::BacksideEnabledChanged,
                     this,
                     &CardAreaCardViewModel::BacksideEnabledChanged);
    QObject::connect(&m_Project,
                     &Project::CardCountChanged,
                     this,
                     [this](const fs::path& card_name, uint32_t count)
                     {
                         if (m_CardName == card_name)
                         {
                             CardCountChanged(count);
                         }
                     });
    QObject::connect(&m_Project,
                     &Project::CardBacksideShortEdgeChanged,
                     this,
                     [this](const fs::path& card_name, bool backside_short_edge)
                     {
                         if (m_CardName == card_name)
                         {
                             CardBacksideShortEdgeChanged(backside_short_edge);
                         }
                     });
    QObject::connect(&m_Project,
                     &Project::CardBacksideChanged,
                     this,
                     [this](const fs::path& card_name, OptionalImageRef backside)
                     {
                         if (m_CardName == card_name)
                         {
                             if (backside.has_value() && backside.value() == ""_p)
                             {
                                 CardBacksideChanged(m_Project.m_Data.m_BacksideDefault);
                             }
                             else
                             {
                                 CardBacksideChanged(backside);
                             }
                         }
                     });
    QObject::connect(&m_Project,
                     &Project::BacksideDefaultChanged,
                     this,
                     [this](OptionalImageRef backside_default)
                     {
                         const auto backside{ m_Project.GetBacksideImage(m_CardName) };
                         if (backside.has_value() && backside.value() == ""_p)
                         {
                             CardBacksideChanged(backside_default);
                         }
                         else
                         {
                             CardBacksideChanged(backside);
                         }
                     });
}

const fs::path& CardAreaCardViewModel::GetCardName() const
{
    return m_CardName;
}
bool CardAreaCardViewModel::HasBackside() const
{
    return m_Project.GetBacksideImage(m_CardName).has_value();
}
bool CardAreaCardViewModel::IsBacksideEnabled() const
{
    return m_Project.m_Data.m_BacksideEnabled;
}

CardViewModel* CardAreaCardViewModel::MakeCardViewModel() const
{
    return new CardViewModel{ m_CardName, CardViewParams{ .m_Backside = false, .m_MinimumWidth{ 60_pix } }, m_Project };
}
CardViewModel* CardAreaCardViewModel::MakeBacksideCardViewModel() const
{
    if (auto backside{ m_Project.GetBacksideImage(m_CardName) })
    {
        return new CardViewModel{ backside.value(),
                                  CardViewParams{ .m_Backside = true, .m_MinimumWidth{ 60_pix } },
                                  m_Project };
    }
    else
    {
        return new CardViewModel{ "__back.jpeg",
                                  CardViewParams{ .m_Backside = true, .m_MinimumWidth{ 60_pix } },
                                  m_Project };
    }
}
BlankCardViewModel* CardAreaCardViewModel::MakeBlankCardViewModel() const
{
    return new BlankCardViewModel{ CardViewParams{ .m_Backside = true, .m_MinimumWidth{ 60_pix } }, m_Project };
}
ImageBrowseViewModel* CardAreaCardViewModel::MakeImageBrowseViewModel() const
{
    return new ImageBrowseViewModel{ m_Project, { &m_CardName, 1 } };
}

void CardAreaCardViewModel::DecrementCard()
{
    m_Project.DecrementCardCount(m_CardName);
}
void CardAreaCardViewModel::IncrementCard()
{
    m_Project.IncrementCardCount(m_CardName);
}
void CardAreaCardViewModel::SetCardCount(const QString& count)
{
    const auto num{
        static_cast<uint32_t>(
            qMin(
                qMax(0, count.toLong()),
                static_cast<long>(std::numeric_limits<uint32_t>::max()))),
    };
    m_Project.SetCardCount(m_CardName, num);
}
void CardAreaCardViewModel::SetCardBacksideShortEdge(Qt::CheckState backside_short_edge)
{
    m_Project.SetCardBacksideShortEdge(m_CardName, backside_short_edge != Qt::CheckState::Unchecked);
}

void CardAreaCardViewModel::SetBacksideImage(const fs::path& backside_name)
{
    m_Project.SetBacksideImage(m_CardName, backside_name);
}
void CardAreaCardViewModel::ClearBacksideImage()
{
    m_Project.ClearBacksideImage(m_CardName);
}
void CardAreaCardViewModel::SetBacksideImageDefault()
{
    m_Project.SetBacksideImageDefault(m_CardName);
}

void CardAreaCardViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    BacksideEnabledChanged(m_Project.m_Data.m_BacksideEnabled);

    CardCountChanged(m_Project.GetCardCount(m_CardName));
    CardBacksideShortEdgeChanged(m_Project.HasCardBacksideShortEdge(m_CardName));

    const auto backside{ m_Project.GetBacksideImage(m_CardName) };
    if (backside.has_value() && backside.value() == ""_p)
    {
        CardBacksideChanged(m_Project.m_Data.m_BacksideDefault);
    }
    else
    {
        CardBacksideChanged(backside);
    }
}
