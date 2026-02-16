#include <ppp/ui/widget_card_area.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_card.hpp>

#include <ppp/ui/popups/image_browse_popup.hpp>

#include <ppp/profile/profile.hpp>

class CardWidget : public QFrame
{
    Q_OBJECT

  public:
    CardWidget(const fs::path& card_name, Project& project)
        : m_CardName{ card_name }
        , m_BacksideEnabled{ project.m_Data.m_BacksideEnabled }
        , m_Backside{ project.GetBacksideImage(card_name) }
    {
        TRACY_AUTO_SCOPE();

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
        const auto minimum_width{ std::max(minimum_img_width + margins.left() + margins.right(), 160) };
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
        TRACY_AUTO_SCOPE();

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

        const auto margins{ layout()->contentsMargins() };
        const auto minimum_img_width{ m_ImageWidget->minimumWidth() };
        const auto minimum_width{ std::max(minimum_img_width + margins.left() + margins.right(), 160) };
        setMinimumSize(minimum_width, CardWidget::heightForWidth(minimum_width));
    }

  private:
    QWidget* MakeCardWidget(Project& project)
    {
        TRACY_AUTO_SCOPE();

        auto* card_image{ new CardImage{ m_CardName, project, CardImage::Params{} } };
        card_image->EnableContextMenu(true, project);

        if (m_BacksideEnabled)
        {
            BacksideImage* backside_image{ new BacksideImage{ project.GetBacksideImage(m_CardName), project } };
            backside_image->EnableContextMenu(true, project);

            auto* stacked_widget{ new StackedCardBacksideView{ card_image, backside_image } };

            auto backside_choose{
                [this, &project]()
                {
                    ImageBrowsePopup image_browser{ window(), project, { &m_CardName, 1 } };
                    image_browser.setWindowTitle(QString{ "Choose backside for %1" }.arg(ToQString(m_CardName)));
                    if (const auto backside_choice{ image_browser.Show() })
                    {
                        const auto& backside{ backside_choice.value() };
                        project.SetBacksideImage(m_CardName, backside);
                    }
                }
            };

            if (!m_CardName.empty())
            {
                QObject::connect(&project,
                                 &Project::CardBacksideChanged,
                                 stacked_widget,
                                 [this, stacked_widget, &project](const fs::path& card_name, const fs::path& backside)
                                 {
                                     if (m_CardName == card_name)
                                     {
                                         auto* new_backside_image{
                                             backside.empty()
                                                 ? new BacksideImage{ project.m_Data.m_BacksideDefault, project }
                                                 : new BacksideImage{ backside, project },
                                         };
                                         new_backside_image->EnableContextMenu(true, project);
                                         stacked_widget->RefreshBackside(new_backside_image);
                                     }
                                 });
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

        TRACY_AUTO_SCOPE();

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
        TRACY_AUTO_SCOPE();

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

class CardGrid : public QWidget
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

    void FullRefresh()
    {
        TRACY_AUTO_SCOPE();

        const auto cols{ g_Cfg.m_DisplayColumns };
        for (size_t j = m_Dummies.size(); j < cols; j++)
        {
            fs::path card_name{ fmt::format("__dummy__{}", j) };
            auto* dummy{ new DummyCardWidget{ card_name, m_Project } };
            m_Dummies.push_back(dummy);
        }

        {
            std::unordered_map<fs::path, CardWidget*> old_cards{
                std::move(m_Cards)
            };
            m_Cards = {};

            auto eat_or_make_card{
                [this, &old_cards](const fs::path& card_name) -> CardWidget*
                {
                    auto it{ old_cards.find(card_name) };
                    if (it == old_cards.end())
                    {
                        return new CardWidget{ card_name, m_Project };
                    }

                    CardWidget* card{ it->second };
                    old_cards.erase(it);
                    card->Refresh(m_Project);
                    return card;
                }
            };

            for (const auto& card_info : m_Project.GetCards())
            {
                const bool hidden{ card_info.m_Hidden > 0 };
                if (hidden)
                {
                    continue;
                }

                const auto& card_name{ card_info.m_Name };
                auto* card_widget{ eat_or_make_card(card_name) };
                m_Cards[card_name] = card_widget;
            }

            for (auto& [card_name, card] : old_cards)
            {
                delete card;
            }
        }

        ApplyFilter(m_CurrentFilter);
    }

    void ApplyFilter(const QString& filter)
    {
        TRACY_AUTO_SCOPE();
        TRACY_AUTO_SCOPE_INFO("Filter: \"%s\"", filter.isEmpty() ? "<none>" : filter.toStdString().c_str());

        m_CurrentFilter = filter;
        m_FirstItem = nullptr;

        {
            TRACY_NAMED_SCOPE(destroy_old_layout);
            if (auto* old_layout{ static_cast<QGridLayout*>(layout()) })
            {
                for (auto& [card_name, card] : m_Cards)
                {
                    old_layout->removeWidget(card);
                    card->setParent(nullptr);
                }
                for (auto* dummy : m_Dummies)
                {
                    old_layout->removeWidget(dummy);
                    dummy->setParent(nullptr);
                }
                delete old_layout;
            }
        }

        auto* this_layout{ new QGridLayout };
        this_layout->setContentsMargins(9, 9, 9, 9);
        setLayout(this_layout);

        const auto cols{ g_Cfg.m_DisplayColumns };

        const QString filter_lower{ filter.toLower() };
        size_t i{ 0 };

        {
            TRACY_NAMED_SCOPE(filter_cards);
            for (const auto& card_info : m_Project.GetCards())
            {
                const auto& card_name{ card_info.m_Name };
                if (!m_Cards.contains(card_name))
                {
                    continue;
                }

                if (!filter.isEmpty() && !ToQString(card_name).toLower().contains(filter_lower))
                {
                    continue;
                }

                auto* card_widget{ m_Cards.at(card_name) };
                if (m_FirstItem == nullptr)
                {
                    m_FirstItem = card_widget;
                }

                const auto x{ static_cast<int>(i / cols) };
                const auto y{ static_cast<int>(i % cols) };
                this_layout->addWidget(card_widget, x, y);
                ++i;
            }
        }

        if (i < cols)
        {
            TRACY_NAMED_SCOPE(fill_up_with_dummies);
            for (size_t j = i; j < cols; j++)
            {
                auto* dummy_widget{ m_Dummies[j] };
                if (m_FirstItem == nullptr)
                {
                    m_FirstItem = dummy_widget;
                }

                this_layout->addWidget(dummy_widget, 0, static_cast<int>(j));
                ++i;
            }
        }

        {
            TRACY_NAMED_SCOPE(set_column_stretch);
            for (int c = 0; c < this_layout->columnCount(); c++)
            {
                this_layout->setColumnStretch(c, 1);
            }
        }

        m_Columns = cols;
        m_Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / m_Columns));

        TRACY_NAMED_SCOPE(compute_height);
        setMinimumWidth(TotalWidthFromItemWidth(m_FirstItem->minimumWidth()));
        setMinimumHeight(heightForWidth(minimumWidth()));
        setFixedHeight(heightForWidth(size().width()));
    }

    bool HasCard(const fs::path& card_name) const
    {
        return m_Cards.contains(card_name);
    }

    bool HasCards() const
    {
        return !m_Cards.empty();
    }

    std::unordered_map<fs::path, CardWidget*>& GetCards()
    {
        return m_Cards;
    }

  private:
    Project& m_Project;

    std::unordered_map<fs::path, CardWidget*> m_Cards;
    std::vector<CardWidget*> m_Dummies;
    CardWidget* m_FirstItem;

    uint32_t m_Columns;
    uint32_t m_Rows;

    QString m_CurrentFilter{ "" };
};

class CardScrollArea : public QScrollArea
{
  public:
    CardScrollArea(Project& project);

    CardGrid& GetGrid()
    {
        return *m_Grid;
    }

    void FullRefresh();
    void RefreshGridSize();

    void ApplyFilter(const QString& filter);

  private:
    int ComputeMinimumWidth() const;

    virtual void showEvent(QShowEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

    CardGrid* m_Grid;
};

CardScrollArea::CardScrollArea(Project& project)
{
    TRACY_AUTO_SCOPE();

    m_Grid = new CardGrid{ project };

    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);
    setWidget(m_Grid);

    setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
}

void CardScrollArea::FullRefresh()
{
    m_Grid->FullRefresh();
    setMinimumWidth(ComputeMinimumWidth());
    RefreshGridSize();
}

void CardScrollArea::RefreshGridSize()
{
    TRACY_AUTO_SCOPE();

    const auto width{ size().width() };
    const auto height{ m_Grid->heightForWidth(width) };
    m_Grid->setFixedHeight(height);
}

void CardScrollArea::ApplyFilter(const QString& filter)
{
    m_Grid->ApplyFilter(filter);
}

int CardScrollArea::ComputeMinimumWidth() const
{
    const auto margins{ contentsMargins() };
    const auto scrollbar_width{ verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0 };
    return m_Grid->minimumWidth() + 2 * scrollbar_width + margins.left() + margins.right();
}

void CardScrollArea::showEvent(QShowEvent* event)
{
    QScrollArea::showEvent(event);
    setMinimumWidth(ComputeMinimumWidth());
    RefreshGridSize();
}

void CardScrollArea::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);
    RefreshGridSize();
}

CardArea::CardArea(Project& project)
    : m_Project{ project }
{
    TRACY_AUTO_SCOPE();

    {
        TRACY_NAMED_SCOPE(init_onboarding);

        auto* onboarding_line_1{ new QLabel{ "No images are loaded..." } };
        auto* onboarding_line_2{ new QLabel{
            QString(
                "Start by adding images into the <a href=\"file:///%1\">image folder</a>")
                .arg(ToQString(project.m_Data.m_ImageDir).replace(' ', "%20")),
        } };
        auto* onboarding_line_3{ new QLabel{
            "or drag-and-drop images onto the app",
        } };

        onboarding_line_1->setAlignment(Qt::AlignmentFlag::AlignCenter);
        onboarding_line_2->setAlignment(Qt::AlignmentFlag::AlignCenter);
        onboarding_line_3->setAlignment(Qt::AlignmentFlag::AlignCenter);

        auto* onboarding_layout{ new QVBoxLayout };
        onboarding_layout->addStretch();
        onboarding_layout->addWidget(onboarding_line_1);
        onboarding_layout->addWidget(onboarding_line_2);
        onboarding_layout->addWidget(onboarding_line_3);
        onboarding_layout->addStretch();
        onboarding_layout->setContentsMargins(6, 0, 6, 0);
        onboarding_layout->setAlignment(Qt::AlignmentFlag::AlignCenter);

        m_OnboardingHint = new QWidget;
        m_OnboardingHint->setLayout(onboarding_layout);

        auto image_folder_link_activated{
            [](const QString& link)
            {
                const auto url{ QUrl::fromUserInput(link) };
                if (url.isValid())
                {
                    QDesktopServices::openUrl(url);
                }
            }
        };

        QObject::connect(onboarding_line_2,
                         &QLabel::linkActivated,
                         this,
                         image_folder_link_activated);
    }

    {
        TRACY_NAMED_SCOPE(init_header);

        auto* global_label{ new QLabel{ "Global Controls:" } };
        auto* global_decrement_button{ new QPushButton{ "-" } };
        auto* global_increment_button{ new QPushButton{ "+" } };
        auto* global_set_zero_button{ new QPushButton{ "Zero All" } };
        m_Filter = new QLineEdit;
        m_Filter->setPlaceholderText("Filter");
        m_RemoveExternalCards = new QPushButton{ "Remove All External Cards" };

        global_decrement_button->setToolTip("Remove one from all");
        global_increment_button->setToolTip("Add one to all");
        global_set_zero_button->setToolTip("Set all to zero");
        m_Filter->setToolTip("Filter by filename");
        m_RemoveExternalCards->setToolTip("Removes all cards not part of the images folder");

        auto* header_layout{ new QHBoxLayout };
        header_layout->addWidget(global_label);
        header_layout->addWidget(global_decrement_button);
        header_layout->addWidget(global_increment_button);
        header_layout->addWidget(global_set_zero_button);
        header_layout->addWidget(m_Filter);
        header_layout->addWidget(m_RemoveExternalCards);
        header_layout->addStretch();
        header_layout->setContentsMargins(6, 0, 6, 0);

        m_Header = new QWidget;
        m_Header->setLayout(header_layout);

        m_RemoveExternalCards->setVisible(project.HasExternalCards());

        auto dec_number{
            [this, &project]()
            {
                for (auto& [_, card] : m_ScrollArea->GetGrid().GetCards())
                {
                    card->DecrementNumber(project);
                }
            }
        };

        auto inc_number{
            [this, &project]()
            {
                for (auto& [_, card] : m_ScrollArea->GetGrid().GetCards())
                {
                    card->IncrementNumber(project);
                }
            }
        };

        auto reset_number{
            [this, &project]()
            {
                for (auto& [_, card] : m_ScrollArea->GetGrid().GetCards())
                {
                    card->ApplyNumber(project, 0);
                }
            }
        };

        auto apply_filter{
            [this](const QString& text)
            {
                m_ScrollArea->ApplyFilter(text);
            }
        };

        auto remove_all_external{
            [this, &project]()
            {
                for (auto& card : m_Project.m_Data.m_Cards)
                {
                    if (card.m_ExternalPath.has_value())
                    {
                        project.RemoveExternalCard(card.m_Name);
                    }
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
        QObject::connect(m_Filter,
                         &QLineEdit::textChanged,
                         this,
                         apply_filter);
        QObject::connect(m_RemoveExternalCards,
                         &QPushButton::clicked,
                         this,
                         remove_all_external);
    }

    m_ScrollArea = new CardScrollArea{ project };

    auto* card_area_layout{ new QVBoxLayout };
    card_area_layout->addWidget(m_OnboardingHint);
    card_area_layout->addWidget(m_Header);
    card_area_layout->addWidget(m_ScrollArea);

    setLayout(card_area_layout);

    const auto& grid{ m_ScrollArea->GetGrid() };
    m_OnboardingHint->setVisible(!grid.HasCards());
    m_Header->setVisible(grid.HasCards());
    m_ScrollArea->setVisible(grid.HasCards());

    m_RefreshTimer.setSingleShot(true);
    m_RefreshTimer.setInterval(50);
    QObject::connect(&m_RefreshTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         m_ScrollArea->FullRefresh();

                         const auto& grid{ m_ScrollArea->GetGrid() };
                         m_OnboardingHint->setVisible(!grid.HasCards());
                         m_Header->setVisible(grid.HasCards());
                         m_ScrollArea->setVisible(grid.HasCards());
                     });
}

void CardArea::NewProjectOpened()
{
    FullRefresh();
}

void CardArea::ImageDirChanged()
{
    FullRefresh();
}

void CardArea::BacksideEnabledChanged()
{
    auto& grid{ m_ScrollArea->GetGrid() };
    grid.BacksideEnabledChanged();
}

void CardArea::BacksideDefaultChanged()
{
    auto& grid{ m_ScrollArea->GetGrid() };
    grid.BacksideDefaultChanged();
}

void CardArea::DisplayColumnsChanged()
{
    FullRefresh();
}

void CardArea::CardOrderChanged()
{
    FullRefresh();
}

void CardArea::CardOrderDirectionChanged()
{
    FullRefresh();
}

void CardArea::CardAdded(const fs::path& card_name)
{
    m_RemoveExternalCards->setVisible(m_Project.HasExternalCards());

    const auto& grid{ m_ScrollArea->GetGrid() };
    if (grid.HasCard(card_name))
    {
        return;
    }

    FullRefresh();
}

void CardArea::CardRemoved(const fs::path& card_name)
{
    m_RemoveExternalCards->setVisible(m_Project.HasExternalCards());

    const auto& grid{ m_ScrollArea->GetGrid() };
    if (!grid.HasCard(card_name))
    {
        return;
    }

    FullRefresh();
}

void CardArea::CardRenamed(const fs::path& old_card_name, const fs::path& /*new_card_name*/)
{
    const auto& grid{ m_ScrollArea->GetGrid() };
    if (!grid.HasCard(old_card_name))
    {
        return;
    }

    FullRefresh();
}

void CardArea::CardVisibilityChanged(const fs::path& card_name, bool visible)
{
    const auto& grid{ m_ScrollArea->GetGrid() };
    if (visible && !grid.HasCard(card_name))
    {
        FullRefresh();
    }
    else if (!visible && grid.HasCard(card_name))
    {
        FullRefresh();
    }
}

void CardArea::FullRefresh()
{
    m_RefreshTimer.start();
}

#include <widget_card_area.moc>
