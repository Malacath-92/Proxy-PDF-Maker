#include <ppp/ui/widget_util/card/widget_selectable_card.hpp>

#include <QVBoxLayout>

#include <ppp/ui/widget_util/card/widget_card_image.hpp>

#include <ppp/ui/view_models/view_model_card.hpp>

SelectableCard::SelectableCard(CardViewModel* view_model)
    : m_ViewModel{ view_model }
    , m_CardImage{ new CardImage{ view_model } }
{
    auto* this_layout{ new QVBoxLayout };
    this_layout->addWidget(m_CardImage);
    setLayout(this_layout);

    setFrameShape(Shape::StyledPanel);
    setFrameShadow(Shadow::Raised);
    setLineWidth(5);
    setStyleSheet("QFrame{ background-color: transparent; }");

    setMinimumWidth(m_CardImage->minimumWidth() + lineWidth() * 2);
}

bool SelectableCard::ToggleSelected()
{
    m_Selected ? Unselect() : Select();
    return m_Selected;
}
void SelectableCard::Select()
{
    m_Selected = true;
    setStyleSheet("QFrame{ background-color: blue; }");
}
void SelectableCard::Unselect()
{
    m_Selected = false;
    if (underMouse())
    {
        setStyleSheet("QFrame{ background-color: purple; }");
    }
    else
    {
        setStyleSheet("QFrame{ background-color: transparent; }");
    }
}

const fs::path& SelectableCard::GetCardName() const
{
    return m_ViewModel->GetCardName();
}

void SelectableCard::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);
    if (!m_Selected)
    {
        setStyleSheet("QFrame{ background-color: purple; }");
    }
}
void SelectableCard::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    if (!m_Selected)
    {
        setStyleSheet("QFrame{ background-color: transparent; }");
    }
}

bool SelectableCard::hasHeightForWidth() const
{
    return true;
}
int SelectableCard::heightForWidth(int width) const
{
    return m_CardImage->heightForWidth(width - lineWidth() * 2) + lineWidth() * 2;
}
