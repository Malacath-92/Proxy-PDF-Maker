#include <ppp/ui/widget_util/card/widget_selectable_card_grid.hpp>

#include <QResizeEvent>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

#include <ppp/project/card_info.hpp>

#include <ppp/ui/widget_util/card/widget_selectable_card.hpp>

#include <ppp/ui/view_models/util.hpp>
#include <ppp/ui/view_models/view_model_selectable_card_grid.hpp>

SelectableCardGrid::SelectableCardGrid(SelectableCardGridViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    view_model->setParent(this);

    // Make all cards and dummies ahead of time
    {
        for (auto& card_info : m_ViewModel.GetCards())
        {
            const auto& card_name{ card_info.m_Name };
            if (m_ViewModel.IsCardIgnored(card_name) || card_info.m_Transient)
            {
                continue;
            }

            auto* card_view_model{ m_ViewModel.MakeCardViewModel(card_name) };
            auto* card_widget{ new SelectableCard{ card_view_model } };
            card_widget->installEventFilter(this);
            m_Cards.push_back({ card_widget, ToQString(card_name).toLower() });
        }

        for (size_t j = 0; j < c_Columns; j++)
        {
            auto* dummy_widget{ new QWidget };

            QSizePolicy size_policy{ dummy_widget->sizePolicy() };
            size_policy.setRetainSizeWhenHidden(true);
            dummy_widget->setSizePolicy(size_policy);
            dummy_widget->setVisible(false);

            m_Dummies.push_back(dummy_widget);
        }
    }

    ApplyFilter("", true);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardAdded);
}

void SelectableCardGrid::ApplyFilter(const QString& filter, bool force)
{
    QString filter_lower{ filter.toLower() };
    if (!force && m_CurrentFilter == filter_lower)
    {
        return;
    }
    m_CurrentFilter = std::move(filter_lower);

    // Remove cards from the old layout
    if (auto* old_layout{ static_cast<QGridLayout*>(layout()) })
    {
        for (auto& [card, card_name] : m_Cards)
        {
            old_layout->removeWidget(card);
            card->hide();
        }
        for (auto* dummy : m_Dummies)
        {
            old_layout->removeWidget(dummy);
            dummy->hide();
        }
        delete old_layout;
    }

    auto* grid_layout{ new QGridLayout };
    setLayout(grid_layout);

    {
        size_t i{ 0 };

        // Put cards into the layout, if the filter permits
        for (auto& [card, card_name] : m_Cards)
        {
            if (IsFiltered(card_name))
            {
                const auto x{ static_cast<int>(i / c_Columns) };
                const auto y{ static_cast<int>(i % c_Columns) };
                grid_layout->addWidget(card, x, y);
                if (isVisible())
                {
                    card->show();
                }

                ++i;
            }
        }

        // Fill empty columns with dummies
        for (size_t j = i; j < c_Columns; j++)
        {
            auto* dummy_widget{ m_Dummies[j] };
            grid_layout->addWidget(dummy_widget, 0, static_cast<int>(j));
        }

        for (int c = 0; c < grid_layout->columnCount(); c++)
        {
            grid_layout->setColumnStretch(c, 1);
        }

        m_Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / c_Columns));
    }

    setMinimumWidth(TotalWidthFromItemWidth(FirstItem()->minimumWidth()));
    setMinimumHeight(SelectableCardGrid::heightForWidth(minimumWidth()));
    adjustSize();
}

int SelectableCardGrid::TotalWidthFromItemWidth(int item_width) const
{
    const auto margins{ layout()->contentsMargins() };
    const auto spacing{ layout()->spacing() };

    return item_width * c_Columns + margins.left() + margins.right() + spacing * (c_Columns - 1);
}

std::optional<fs::path> SelectableCardGrid::GetSelectedCardName() const
{
    if (m_Selected == nullptr)
    {
        return std::nullopt;
    }
    return m_Selected->GetCardName();
}

bool SelectableCardGrid::hasHeightForWidth() const
{
    return true;
}
int SelectableCardGrid::heightForWidth(int width) const
{
    const auto margins{ layout()->contentsMargins() };
    const auto spacing{ layout()->spacing() };

    const auto item_width{ static_cast<float>(width - margins.left() - margins.right() - spacing * (c_Columns - 1)) / c_Columns };
    const auto item_height{ FirstItem()->heightForWidth(static_cast<int>(item_width)) };

    const auto height{ item_height * m_Rows + margins.top() + margins.bottom() + spacing * (m_Rows - 1) };
    return static_cast<int>(height);
}

void SelectableCardGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const auto width{ event->size().width() };
    const auto height{ heightForWidth(width) };
    setFixedHeight(height);
}

bool SelectableCardGrid::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouse_event{ static_cast<QMouseEvent*>(event) };
        if (mouse_event->button() == Qt::MouseButton::LeftButton)
        {
            m_ClickStart = obj;
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* mouse_event{ static_cast<QMouseEvent*>(event) };
        if (mouse_event->button() == Qt::MouseButton::LeftButton && m_ClickStart == obj)
        {
            auto* card{ dynamic_cast<SelectableCard*>(m_ClickStart) };
            if (card != nullptr)
            {
                auto* current_selected{ dynamic_cast<SelectableCard*>(m_Selected) };
                if (current_selected != nullptr && current_selected != card)
                {
                    current_selected->Unselect();
                }

                m_Selected = card->ToggleSelected() ? card : nullptr;
            }
            return true;
        }

        m_ClickStart = nullptr;
    }

    return QWidget::eventFilter(obj, event);
}

void SelectableCardGrid::CardAdded(const fs::path& card_name)
{
    const auto name_lowercase{ ToQString(card_name).toLower() };
    if (!std::ranges::contains(m_Cards, name_lowercase, &Card::m_NameLowercase))
    {
        auto* card_view_model{ m_ViewModel.MakeCardViewModel(card_name) };
        auto* card_widget{ new SelectableCard{ card_view_model } };
        card_widget->installEventFilter(this);
        m_Cards.push_back({ card_widget, std::move(name_lowercase) });

        if (IsFiltered(m_Cards.back().m_NameLowercase))
        {
            auto current_filter{ std::move(m_CurrentFilter) };
            m_CurrentFilter.clear();
            ApplyFilter(std::move(current_filter), true);
        }
    }
}

QWidget* SelectableCardGrid::FirstItem() const
{
    return m_Cards.empty() ? m_Dummies[0]
                           : m_Cards[0].m_Widget;
}
bool SelectableCardGrid::IsFiltered(const QString& card_name_lowercase)
{
    return m_CurrentFilter.isEmpty() ||
           card_name_lowercase.contains(m_CurrentFilter);
}
