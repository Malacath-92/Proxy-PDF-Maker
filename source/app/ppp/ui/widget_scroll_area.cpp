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

class CardWidget : public QFrame
{
    Q_OBJECT

  public:
    CardWidget(const fs::path& card_name, Project& project)
        : m_CardName{ card_name }
        , m_BacksideEnabled{ project.m_Data.m_BacksideEnabled }
        , m_Backside{ project.GetBacksideImage(card_name) }
    {
        const uint32_t initial_number{ card_name.empty() ? 1 : project.m_Data.m_Cards[card_name].m_Num };

        auto* number_edit{ new QLineEdit };
        number_edit->setValidator(new QIntValidator{ 0, 100, this });
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
        project.m_Data.m_Cards[m_CardName].m_Num = static_cast<uint32_t>(std::max(std::min(number, int64_t{ 999 }), int64_t{ 0 }));
        m_NumberEdit->setText(QString{}.setNum(project.m_Data.m_Cards[m_CardName].m_Num));
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

  signals:
    void CardHidden();
    void CardShown();

  private:
    QWidget* MakeCardWidget(Project& project)
    {
        auto* card_image{ new CardImage{ m_CardName, project, CardImage::Params{} } };

        if (m_BacksideEnabled)
        {
            BacksideImage* backside_image{ new BacksideImage{ project.GetBacksideImage(m_CardName), project } };

            auto* stacked_widget{ new StackedCardBacksideView{ card_image, backside_image } };

            auto backside_reset{
                [=, this, &project]()
                {
                    const auto old_backside{ project.m_Data.m_Cards[m_CardName].m_Backside };
                    project.m_Data.m_Cards[m_CardName].m_Backside.clear();
                    auto* new_backside_image{ new BacksideImage{ project.GetBacksideImage(m_CardName), project } };
                    stacked_widget->RefreshBackside(new_backside_image);

                    auto it{ project.m_Data.m_Cards.find(old_backside) };
                    if (it != project.m_Data.m_Cards.end())
                    {
                        it->second.m_Hidden--;
                        const bool is_shown{ it->second.m_Hidden == 0 };
                        if (is_shown)
                        {
                            CardShown();
                        }
                    }
                }
            };

            auto backside_choose{
                [=, this, &project]()
                {
                    if (const auto backside_choice{ OpenImageDialog(project.m_Data.m_ImageDir) })
                    {
                        const auto& backside{ backside_choice.value() };
                        if (backside != project.m_Data.m_Cards[m_CardName].m_Backside && backside != m_CardName)
                        {
                            project.m_Data.m_Cards[m_CardName].m_Backside = backside;
                            auto* new_backside_image{ new BacksideImage{ backside, project } };
                            stacked_widget->RefreshBackside(new_backside_image);

                            auto it{ project.m_Data.m_Cards.find(backside) };
                            if (it != project.m_Data.m_Cards.end())
                            {
                                const bool was_visible{ it->second.m_Hidden == 0 };
                                it->second.m_Hidden++;
                                it->second.m_Num = 0;
                                if (was_visible)
                                {
                                    CardHidden();
                                }
                            }
                        }
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
            const bool is_short_edge{ m_CardName.empty() ? false : project.m_Data.m_Cards[m_CardName].m_BacksideShortEdge };

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
        const auto number{ static_cast<int64_t>(project.m_Data.m_Cards[m_CardName].m_Num) + 1 };
        ApplyNumber(project, number);
    }

    virtual void DecrementNumber(Project& project)
    {
        const auto number{ static_cast<int64_t>(project.m_Data.m_Cards[m_CardName].m_Num) - 1 };
        ApplyNumber(project, number);
    }

    virtual void SetShortEdge(Project& project, Qt::CheckState s)
    {
        auto& card{ project.m_Data.m_Cards[m_CardName] };
        card.m_BacksideShortEdge = s == Qt::CheckState::Checked;
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
    virtual void DecrementNumber(Project&)  override{}
    virtual void SetShortEdge(Project&, Qt::CheckState)  override{}
    // clang-format on
};

class CardScrollArea::CardGrid : public QWidget
{
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
                    CardWidget* new_card{ ctor(card_name, m_Project) };
                    QObject::connect(new_card,
                                     &CardWidget::CardShown,
                                     this,
                                     &CardGrid::FullRefresh);
                    QObject::connect(new_card,
                                     &CardWidget::CardHidden,
                                     this,
                                     &CardGrid::FullRefresh);
                    return new_card;
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
        for (auto& [card_name, card_info] : m_Project.m_Data.m_Cards)
        {
            if (card_info.m_Hidden > 0)
            {
                continue;
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

void CardScrollArea::CardAdded()
{
    FullRefresh();
}

void CardScrollArea::CardRemoved()
{
    FullRefresh();
}

void CardScrollArea::CardRenamed()
{
    FullRefresh();
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
