#include <ppp/ui/view_models/view_model_card.hpp>

#include <ppp/config.hpp>

#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

CardViewModel::CardViewModel(fs::path card_name,
                             CardViewParams params,
                             const Project& project)
    : m_CardName{ std::move(card_name) }
    , m_ViewParams{ params }
    , m_Project{ project }
{
}

void CardViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();
}
