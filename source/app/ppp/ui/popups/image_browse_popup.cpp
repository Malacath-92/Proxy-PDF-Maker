#include <ppp/ui/popups/image_browse_popup.hpp>

#include <ranges>

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_card.hpp>

class SelectableCard : public QFrame
{
    Q_OBJECT

  public:
    SelectableCard(const fs::path& card_name, const Project& project)
    {
        m_CardImage = new CardImage{
            card_name,
            project,
            CardImage::Params{
                .m_MinimumWidth{ 80 },
            },
        };

        auto* this_layout{ new QVBoxLayout };
        this_layout->addWidget(m_CardImage);
        setLayout(this_layout);

        setFrameShape(Shape::StyledPanel);
        setFrameShadow(Shadow::Raised);
        setLineWidth(5);

        setMinimumWidth(m_CardImage->minimumWidth() + lineWidth() * 2);
    }

    bool ToggleSelected()
    {
        m_Selected ? Unselect() : Select();
        return m_Selected;
    }

    void Select()
    {
        m_Selected = true;
        setStyleSheet("QFrame{ background-color: blue; }");
    }

    void Unselect()
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

    const fs::path& GetCardName() const
    {
        return m_CardImage->GetCardName();
    }
    virtual void enterEvent(QEnterEvent* event) override
    {
        QFrame::enterEvent(event);
        if (!m_Selected)
        {
            setStyleSheet("QFrame{ background-color: purple; }");
        }
    }

    virtual void leaveEvent(QEvent* event) override
    {
        QFrame::leaveEvent(event);
        if (!m_Selected)
        {
            setStyleSheet("QFrame{ background-color: transparent; }");
        }
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return m_CardImage->heightForWidth(width - lineWidth() * 2) + lineWidth() * 2;
    }

  private:
    CardImage* m_CardImage{ nullptr };
    bool m_Selected{ false };
};

class SelectableCardGrid : public QWidget
{
    Q_OBJECT

  public:
    SelectableCardGrid(const Project& project,
                       std::span<const fs::path> ignored_images)
    {
        // Make all cards and dummies ahead of time
        {
            for (auto& card_info : project.GetCards())
            {
                const auto& card_name{ card_info.m_Name };
                if (std::ranges::contains(ignored_images, card_name) ||
                    card_info.m_Transient)
                {
                    continue;
                }

                auto* card_widget{ new SelectableCard{ card_name, project } };
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

        ApplyFilter("");
    }

    void ApplyFilter(const QString& filter)
    {
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

        {
            const QString filter_lower{ filter.toLower() };
            size_t i{ 0 };

            // Put cards into the layout, if the filter permits
            for (auto& [card, card_name] : m_Cards)
            {
                if (filter.isEmpty() || card_name.contains(filter_lower))
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
                if (isVisible())
                {
                    dummy_widget->show();
                }
            }

            for (int c = 0; c < grid_layout->columnCount(); c++)
            {
                grid_layout->setColumnStretch(c, 1);
            }

            m_Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / c_Columns));
        }

        setLayout(grid_layout);

        setMinimumWidth(TotalWidthFromItemWidth(m_Cards[0].m_Widget->minimumWidth()));
        setMinimumHeight(SelectableCardGrid::heightForWidth(minimumWidth()));
        adjustSize();
    }

    int TotalWidthFromItemWidth(int item_width) const
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        return item_width * c_Columns + margins.left() + margins.right() + spacing * (c_Columns - 1);
    }

    std::optional<fs::path> GetSelectedCardName() const
    {
        if (m_Selected == nullptr)
        {
            return std::nullopt;
        }
        return m_Selected->GetCardName();
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        const auto item_width{ static_cast<float>(width - margins.left() - margins.right() - spacing * (c_Columns - 1)) / c_Columns };
        const auto item_height{ m_Cards[0].m_Widget->heightForWidth(static_cast<int>(item_width)) };

        const auto height{ item_height * m_Rows + margins.top() + margins.bottom() + spacing * (m_Rows - 1) };
        return static_cast<int>(height);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const auto width{ event->size().width() };
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);
    }

    virtual bool eventFilter(QObject* obj, QEvent* event) override
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

  public:
    QObject* m_ClickStart{ nullptr };
    SelectableCard* m_Selected{ nullptr };

    struct Card
    {
        SelectableCard* m_Widget;
        QString m_NameLowercase;
    };
    std::vector<Card> m_Cards;
    std::vector<QWidget*> m_Dummies;

    static inline constexpr uint32_t c_Columns{ 6 };
    uint32_t m_Rows;
};

ImageBrowsePopup::ImageBrowsePopup(QWidget* parent,
                                   const Project& project,
                                   std::span<const fs::path> ignored_images)
    : PopupBase{ parent }
{
    m_AutoCenter = false;
    m_AutoCenterOnShow = false;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("Choose Image");

    m_Filter = new QLineEdit;
    m_Filter->setPlaceholderText("Filter");

    const auto& cards{ project.GetCards() };
    const auto num_valid_ignored_images{
        std::ranges::count_if(ignored_images, [&](const auto& img)
                              { return project.HasCard(img); })
    };
    const auto num_valid_images{
        std::ranges::count_if(cards,
                              [&](const auto& img)
                              { return !img.m_Transient; })
    };
    const auto has_cards{ num_valid_images > num_valid_ignored_images };

    QWidget* grid{ nullptr };
    if (has_cards)
    {
        m_Grid = new SelectableCardGrid{ project, ignored_images };
        grid = m_Grid;
    }
    else
    {
        auto* error_label{ new QLabel{ num_valid_images == 0 ? "No cards loaded..."
                                                             : "No other cards loaded..." } };
        error_label->setAlignment(Qt::AlignmentFlag::AlignCenter);
        grid = error_label;
    }

    auto* grid_scroll{ new QScrollArea };
    grid_scroll->setWidget(grid);
    grid_scroll->setWidgetResizable(true);
    grid_scroll->setMinimumWidth(
        [=]()
        {
            const auto margins{ has_cards ? grid->layout()->contentsMargins() : QMargins{} };
            return grid->minimumWidth() + 2 * grid_scroll->verticalScrollBar()->width() + margins.left() + margins.right();
        }());

    auto* ok_button{ new QPushButton{ "OK" } };
    auto* cancel_button{ new QPushButton{ "Cancel" } };

    auto* buttons_layout{ new QHBoxLayout };
    buttons_layout->setContentsMargins(0, 0, 0, 0);
    buttons_layout->addStretch();
    buttons_layout->addWidget(ok_button);
    buttons_layout->addWidget(cancel_button);
    buttons_layout->addStretch();

    auto* window_buttons{ new QWidget };
    window_buttons->setLayout(buttons_layout);

    auto* outer_layout{ new QVBoxLayout };
    outer_layout->addWidget(m_Filter);
    outer_layout->addWidget(grid_scroll);
    outer_layout->addWidget(window_buttons);

    setLayout(outer_layout);

    QObject::connect(m_Filter,
                     &QLineEdit::textChanged,
                     this,
                     [this](const QString& text)
                     {
                         if (m_Grid != nullptr)
                         {
                             m_Grid->ApplyFilter(text);
                         }
                     });
    QObject::connect(ok_button,
                     &QPushButton::clicked,
                     this,
                     &QDialog::close);
    QObject::connect(cancel_button,
                     &QPushButton::clicked,
                     [this]()
                     {
                         m_Grid = nullptr;
                         close();
                     });

    const auto parent_rect{ parent != nullptr
                                ? parent->rect()
                                : QApplication::primaryScreen()->geometry() };
    resize(parent_rect.width() - 100, parent_rect.height() - 100);
}

std::optional<fs::path> ImageBrowsePopup::Show()
{
    PopupBase::Show();
    return m_Grid != nullptr ? m_Grid->GetSelectedCardName() : std::nullopt;
}

#include "image_browse_popup.moc"
