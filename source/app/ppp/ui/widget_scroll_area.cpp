#include <ppp/ui/widget_scroll_area.hpp>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_card.hpp>

#include <ppp/ui/popups/image_browse_popup.hpp>

class CardWidget : public QFrame
{
    Q_OBJECT

  public:
    CardWidget(const fs::path& card_name, Project& project)
        : m_CardName{ card_name }
        , m_BacksideEnabled{ project.m_Data.m_BacksideEnabled }
        , m_Backside{ project.GetBacksideImage(card_name) }
    {
        const uint32_t initial_number{ card_name.empty() ? 1 : project.GetCardCount(card_name) };

        auto* number_edit{ new QLineEdit };
        number_edit->setValidator(new QIntValidator{ 0, 999, this });
        number_edit->setText(QString{}.setNum(initial_number));
        number_edit->setFixedWidth(40);

        auto* decrement_button{ new QPushButton{ "-" } };
        decrement_button->setToolTip("Remove one");

        auto* increment_button{ new QPushButton{ "+" } };
        increment_button->setToolTip("Add one");

        auto* number_layout{ new QHBoxLayout };
        number_layout->addStretch();
        number_layout->addWidget(decrement_button);
        number_layout->addWidget(number_edit);
        number_layout->addWidget(increment_button);
        number_layout->addStretch();
        number_layout->setContentsMargins(0, 0, 0, 0);

        auto* number_area{ new QWidget };
        number_area->setLayout(number_layout);
        number_area->setFixedHeight(20);

        QWidget* card_widget{ MakeCardWidget(project) };
        m_ExtraOptions = MakeExtraOptions(project);

        auto* this_layout{ new QVBoxLayout };
        this_layout->addWidget(card_widget);
        this_layout->addWidget(number_area);
        if (m_ExtraOptions != nullptr)
        {
            this_layout->addWidget(m_ExtraOptions);
        }
        setLayout(this_layout);

        QObject::connect(number_edit,
                         &QLineEdit::editingFinished,
                         this,
                         std::bind_front(&CardWidget::EditNumber, this, std::ref(project)));
        QObject::connect(decrement_button,
                         &QPushButton::clicked,
                         this,
                         std::bind_front(&CardWidget::DecrementNumber, this, std::ref(project)));
        QObject::connect(increment_button,
                         &QPushButton::clicked,
                         this,
                         std::bind_front(&CardWidget::IncrementNumber, this, std::ref(project)));

        m_ImageWidget = card_widget;
        m_NumberEdit = number_edit;
        m_NumberArea = number_area;

        const auto margins{ layout()->contentsMargins() };
        const auto minimum_img_width{ card_widget->minimumWidth() };
        const auto minimum_width{ minimum_img_width + margins.left() + margins.right() };
        setMinimumSize(minimum_width, CardWidget::heightForWidth(minimum_width));

        setFrameShape(Shape::Box);
        setFrameShadow(Shadow::Raised);
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        const auto img_width{ width - margins.left() - margins.right() };
        const auto img_height{ m_ImageWidget->heightForWidth(img_width) };

        auto additional_widgets{ m_NumberArea->height() + spacing };
        if (m_ExtraOptions != nullptr)
        {
            additional_widgets += m_ExtraOptions->height() + spacing;
        }

        const auto height{ img_height + additional_widgets + margins.top() + margins.bottom() };
        return height;
    }

    void ApplyNumber(Project& project, int64_t number)
    {
        number = std::max(number, int64_t{ 0 });
        uint32_t final_number{ project.SetCardCount(m_CardName, static_cast<uint32_t>(number)) };
        m_NumberEdit->setText(QString{}.setNum(final_number));
    }

    virtual void Refresh(Project& project)
    {
        const bool backside_enabled_changed{ m_BacksideEnabled != project.m_Data.m_BacksideEnabled };
        const bool backside_changed{ m_Backside != project.GetBacksideImage(m_CardName) };

        m_BacksideEnabled = project.m_Data.m_BacksideEnabled;
        m_Backside = project.GetBacksideImage(m_CardName);

        if (backside_enabled_changed)
        {
            auto* card_widget{ MakeCardWidget(project) };
            layout()->replaceWidget(m_ImageWidget, card_widget);
            std::swap(card_widget, m_ImageWidget);
            delete card_widget;

            auto* extra_options{ MakeExtraOptions(project) };
            if (m_ExtraOptions == nullptr)
            {
                static_cast<QVBoxLayout*>(layout())->addWidget(extra_options);
                m_ExtraOptions = extra_options;
            }
            else if (extra_options == nullptr)
            {
                layout()->removeWidget(m_ExtraOptions);
                delete m_ExtraOptions;
                m_ExtraOptions = nullptr;
            }
            else
            {
                layout()->replaceWidget(m_ExtraOptions, extra_options);
                std::swap(extra_options, m_ExtraOptions);
                delete extra_options;
            }
        }
        else if (backside_changed)
        {
            if (auto* stacked_widget{ dynamic_cast<StackedCardBacksideView*>(m_ImageWidget) })
            {
                BacksideImage* backside_image{ new BacksideImage{ m_Backside, project } };
                stacked_widget->RefreshBackside(backside_image);
            }
        }
    }

  private:
    QWidget* MakeCardWidget(Project& project)
    {
        auto* card_image{ new CardImage{ m_CardName, project, CardImage::Params{} } };
        card_image->EnableContextMenu(true, project);

        if (m_BacksideEnabled)
        {
            BacksideImage* backside_image{ new BacksideImage{ project.GetBacksideImage(m_CardName), project } };

            auto* stacked_widget{ new StackedCardBacksideView{ card_image, backside_image } };

            auto backside_reset{
                [=, this, &project]()
                {
                    project.SetBacksideImage(m_CardName, "");

                    auto* new_backside_image{ new BacksideImage{ project.GetBacksideImage(m_CardName), project } };
                    stacked_widget->RefreshBackside(new_backside_image);
                }
            };

            auto backside_choose{
                [=, this, &project]()
                {
                    ImageBrowsePopup image_browser{ window(), project, { &m_CardName, 1 } };
                    image_browser.setWindowTitle(QString{ "Choose backside for %1" }.arg(ToQString(m_CardName)));
                    if (const auto backside_choice{ image_browser.Show() })
                    {
                        const auto& backside{ backside_choice.value() };
                        project.SetBacksideImage(m_CardName, backside);

                        auto* new_backside_image{ new BacksideImage{ backside, project } };
                        stacked_widget->RefreshBackside(new_backside_image);
                    }
                }
            };

            if (!m_CardName.empty())
            {
                QObject::connect(stacked_widget,
                                 &StackedCardBacksideView::BacksideReset,
                                 this,
                                 backside_reset);
                QObject::connect(stacked_widget,
                                 &StackedCardBacksideView::BacksideClicked,
                                 this,
                                 backside_choose);
            }

            return stacked_widget;
        }
        else
        {
            return card_image;
        }
    }

    QWidget* MakeExtraOptions(Project& project)
    {
        if (!m_BacksideEnabled)
        {
            return nullptr;
        }

        std::vector<QWidget*> extra_options{};

        if (m_BacksideEnabled)
        {
            const bool is_short_edge{ project.HasCardBacksideShortEdge(m_CardName) };

            auto* short_edge_checkbox{ new QCheckBox{ "Sideways" } };
            short_edge_checkbox->setChecked(is_short_edge);
            short_edge_checkbox->setToolTip("Determines whether to flip backside on short edge");

            QObject::connect(short_edge_checkbox,
                             &QCheckBox::checkStateChanged,
                             this,
                             std::bind_front(&CardWidget::SetShortEdge, this, std::ref(project)));

            extra_options.push_back(short_edge_checkbox);
        }

        auto* extra_options_layout{ new QHBoxLayout };
        extra_options_layout->addStretch();
        for (QWidget* option : extra_options)
        {
            extra_options_layout->addWidget(option);
        }
        extra_options_layout->addStretch();
        extra_options_layout->setContentsMargins(0, 0, 0, 0);

        auto* extra_options_area{ new QWidget };
        extra_options_area->setLayout(extra_options_layout);
        extra_options_area->setFixedHeight(20);

        return extra_options_area;
    }

  public slots:
    virtual void EditNumber(Project& project)
    {
        ApplyNumber(project, m_NumberEdit->text().toLongLong());
    }

    virtual void IncrementNumber(Project& project)
    {
        const auto number{ static_cast<int64_t>(project.GetCardCount(m_CardName)) + 1 };
        ApplyNumber(project, number);
    }

    virtual void DecrementNumber(Project& project)
    {
        const auto number{ static_cast<int64_t>(project.GetCardCount(m_CardName)) - 1 };
        ApplyNumber(project, number);
    }

    virtual void SetShortEdge(Project& project, Qt::CheckState s)
    {
        project.SetCardBacksideShortEdge(m_CardName,
                                         s == Qt::CheckState::Checked);
    }

  protected:
    fs::path m_CardName;

  private:
    bool m_BacksideEnabled{ false };
    fs::path m_Backside{};

    QWidget* m_ImageWidget{ nullptr };
    QLineEdit* m_NumberEdit{ nullptr };
    QWidget* m_NumberArea{ nullptr };
    QWidget* m_ExtraOptions{ nullptr };
};

class DummyCardWidget : public CardWidget
{
    Q_OBJECT

  public:
    DummyCardWidget(const fs::path& card_name, Project& project)
        : CardWidget{ "", project }
    {
        m_CardName = card_name;

        auto sp_retain{ sizePolicy() };
        sp_retain.setRetainSizeWhenHidden(true);
        setSizePolicy(sp_retain);
        hide();
    }

    // clang-format off
    virtual void Refresh(Project& /*project*/) override {}
    // clang-format on

  private slots:
    // clang-format off
    virtual void EditNumber(Project&) override {}
    virtual void IncrementNumber(Project&) override {}
    virtual void DecrementNumber(Project&) override {}
    virtual void SetShortEdge(Project&, Qt::CheckState) override {}
    // clang-format on
};

class CardScrollArea::CardGrid : public QWidget
{
    Q_OBJECT

  public:
    CardGrid(Project& project)
        : m_Project{ project }
    {
        FullRefresh();
    }

    int TotalWidthFromItemWidth(int item_width) const
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        return item_width * m_Columns + margins.left() + margins.right() + spacing * (m_Columns - 1);
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        const auto item_width{ static_cast<float>(width - margins.left() - margins.right() - spacing * (m_Columns - 1)) / m_Columns };
        const auto item_height{ m_FirstItem->heightForWidth(static_cast<int>(item_width)) };

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

    void BacksideEnabledChanged()
    {
        FullRefresh();
    }

    void BacksideDefaultChanged()
    {
        for (const auto& [card_name, card_widget] : m_Cards)
        {
            card_widget->Refresh(m_Project);
        }
    }

    void DisplayColumnsChanged()
    {
        FullRefresh();
    }

    void FullRefresh()
    {
        std::unordered_map<fs::path, CardWidget*> old_cards{
            std::move(m_Cards)
        };
        m_Cards = {};

        if (auto* old_layout{ static_cast<QGridLayout*>(layout()) })
        {
            for (auto& [card_name, card] : old_cards)
            {
                old_layout->removeWidget(card);
            }
            delete old_layout;
        }

        auto* this_layout{ new QGridLayout };
        this_layout->setContentsMargins(9, 9, 9, 9);
        setLayout(this_layout);

        auto eat_or_make_card{
            [this, &old_cards](const fs::path& card_name, auto ctor) -> CardWidget*
            {
                auto it{ old_cards.find(card_name) };
                if (it == old_cards.end())
                {
                    return ctor(card_name, m_Project);
                }

                CardWidget* card{ it->second };
                old_cards.erase(it);
                card->Refresh(m_Project);
                return card;
            }
        };
        auto eat_or_make_real_card{
            [&](const fs::path& card_name)
            {
                return eat_or_make_card(card_name, [](auto&&... args)
                                        { return new CardWidget{ std::forward<decltype(args)>(args)... }; });
            }
        };
        auto eat_or_make_dummy_card{
            [&](const fs::path& card_name)
            {
                return eat_or_make_card(card_name, [](auto&&... args)
                                        { return new DummyCardWidget{ std::forward<decltype(args)>(args)... }; });
            }
        };

        size_t i{ 0 };
        const auto cols{ g_Cfg.m_DisplayColumns };
        for (const auto& card_info : m_Project.GetCards())
        {
            const auto& card_name{ card_info.m_Name };

            const bool hidden{ card_info.m_Hidden > 0 };
            if (hidden)
            {
                // Show cards that are hidden but have bad rotation to allow
                // user to fix rotation
                if (!m_Project.HasPreview(card_name) ||
                    !m_Project.m_Data.m_Previews.at(card_name).m_BadRotation)
                {
                    continue;
                }
            }

            auto* card_widget{ eat_or_make_real_card(card_name) };
            m_Cards[card_name] = card_widget;

            const auto x{ static_cast<int>(i / cols) };
            const auto y{ static_cast<int>(i % cols) };
            this_layout->addWidget(card_widget, x, y);
            ++i;
        }

        for (size_t j = i; j < cols; j++)
        {
            fs::path card_name{ fmt::format("__dummy__{}", j) };
            auto* card_widget{ eat_or_make_dummy_card(card_name) };
            m_Cards[std::move(card_name)] = card_widget;
            this_layout->addWidget(card_widget, 0, static_cast<int>(j));
            ++i;
        }

        for (int c = 0; c < this_layout->columnCount(); c++)
        {
            this_layout->setColumnStretch(c, 1);
        }

        for (auto& [card_name, card] : old_cards)
        {
            delete card;
        }

        m_FirstItem = m_Cards.begin()->second;
        m_Columns = cols;
        m_Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / m_Columns));

        setMinimumWidth(TotalWidthFromItemWidth(m_FirstItem->minimumWidth()));
        setMinimumHeight(heightForWidth(minimumWidth()));
        adjustSize();
    }

    bool HasCard(const fs::path& card_name) const
    {
        return m_Cards.contains(card_name);
    }

    std::unordered_map<fs::path, CardWidget*>& GetCards()
    {
        return m_Cards;
    }

  private:
    Project& m_Project;

    std::unordered_map<fs::path, CardWidget*> m_Cards;
    CardWidget* m_FirstItem;

    uint32_t m_Columns;
    uint32_t m_Rows;
};

CardScrollArea::CardScrollArea(Project& project)
    : m_Project{ project }
{
    auto* global_label{ new QLabel{ "Global Controls:" } };
    auto* global_decrement_button{ new QPushButton{ "-" } };
    auto* global_increment_button{ new QPushButton{ "+" } };
    auto* global_set_zero_button{ new QPushButton{ "Reset All" } };

    global_decrement_button->setToolTip("Remove one from all");
    global_increment_button->setToolTip("Add one to all");
    global_set_zero_button->setToolTip("Set all to zero");

    auto* global_number_layout{ new QHBoxLayout };
    global_number_layout->addWidget(global_label);
    global_number_layout->addWidget(global_decrement_button);
    global_number_layout->addWidget(global_increment_button);
    global_number_layout->addWidget(global_set_zero_button);
    global_number_layout->addStretch();
    global_number_layout->setContentsMargins(6, 0, 6, 0);

    auto* global_number_widget{ new QWidget };
    global_number_widget->setLayout(global_number_layout);

    auto* card_grid{ new CardGrid{ project } };

    auto* card_area_layout{ new QVBoxLayout };
    card_area_layout->addWidget(global_number_widget);
    card_area_layout->addWidget(card_grid);
    card_area_layout->addStretch();
    card_area_layout->setSpacing(0);

    auto* card_area{ new QWidget };
    card_area->setLayout(card_area_layout);

    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);
    setWidget(card_area);

    setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);

    auto dec_number{
        [=, &project]()
        {
            for (auto& [_, card] : card_grid->GetCards())
            {
                card->DecrementNumber(project);
            }
        }
    };

    auto inc_number{
        [=, &project]()
        {
            for (auto& [_, card] : card_grid->GetCards())
            {
                card->IncrementNumber(project);
            }
        }
    };

    auto reset_number{
        [=, &project]()
        {
            for (auto& [_, card] : card_grid->GetCards())
            {
                card->ApplyNumber(project, 0);
            }
        }
    };

    QObject::connect(global_decrement_button,
                     &QPushButton::clicked,
                     this,
                     dec_number);
    QObject::connect(global_increment_button,
                     &QPushButton::clicked,
                     this,
                     inc_number);
    QObject::connect(global_set_zero_button,
                     &QPushButton::clicked,
                     this,
                     reset_number);

    m_Grid = card_grid;
}

void CardScrollArea::NewProjectOpened()
{
    FullRefresh();
}

void CardScrollArea::ImageDirChanged()
{
    FullRefresh();
}

void CardScrollArea::BacksideEnabledChanged()
{
    m_Grid->BacksideEnabledChanged();
}

void CardScrollArea::BacksideDefaultChanged()
{
    m_Grid->BacksideDefaultChanged();
}

void CardScrollArea::DisplayColumnsChanged()
{
    m_Grid->DisplayColumnsChanged();
}

void CardScrollArea::CardOrderChanged()
{
    FullRefresh();
}

void CardScrollArea::CardOrderDirectionChanged()
{
    FullRefresh();
}

void CardScrollArea::CardAdded(const fs::path& card_name)
{
    if (m_Grid->HasCard(card_name))
    {
        return;
    }

    FullRefresh();
}

void CardScrollArea::CardRemoved(const fs::path& card_name)
{
    if (!m_Grid->HasCard(card_name))
    {
        return;
    }

    FullRefresh();
}

void CardScrollArea::CardRenamed(const fs::path& old_card_name, const fs::path& /*new_card_name*/)
{
    if (!m_Grid->HasCard(old_card_name))
    {
        return;
    }

    FullRefresh();
}

void CardScrollArea::CardVisibilityChanged(const fs::path& card_name, bool visible)
{
    if (visible && !m_Grid->HasCard(card_name))
    {
        FullRefresh();
    }
    else if (!visible && m_Grid->HasCard(card_name))
    {
        FullRefresh();
    }
}

void CardScrollArea::FullRefresh()
{
    m_Grid->FullRefresh();
    setMinimumWidth(ComputeMinimumWidth());
}

int CardScrollArea::ComputeMinimumWidth()
{
    const auto margins{ widget()->layout()->contentsMargins() };
    return m_Grid->minimumWidth() + 2 * verticalScrollBar()->width() + margins.left() + margins.right();
}

void CardScrollArea::showEvent(QShowEvent* event)
{
    QScrollArea::showEvent(event);

    setMinimumWidth(ComputeMinimumWidth());
}

#include <widget_scroll_area.moc>
