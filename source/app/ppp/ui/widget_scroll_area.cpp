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
    Q_OBJECT;

  public:
    CardWidget(const fs::path& card_name, Project& project)
        : CardName{ card_name }
        , BacksideEnabled{ project.BacksideEnabled }
        , OversizedEnabled{ project.OversizedEnabled }
    {
        const uint32_t initial_number{ card_name.empty() ? 1 : project.Cards[card_name].Num };

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
        ExtraOptions = MakeExtraOptions(project);

        auto* this_layout{ new QVBoxLayout };
        this_layout->addWidget(card_widget);
        this_layout->addWidget(number_area);
        if (ExtraOptions != nullptr)
        {
            this_layout->addWidget(ExtraOptions);
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

        ImageWidget = card_widget;
        NumberEdit = number_edit;
        NumberArea = number_area;

        const auto margins{ layout()->contentsMargins() };
        const auto minimum_img_width{ card_widget->minimumWidth() };
        const auto minimum_width{ minimum_img_width + margins.left() + margins.right() };
        setMinimumSize(minimum_width, heightForWidth(minimum_width));

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
        const auto img_height{ ImageWidget->heightForWidth(img_width) };

        auto additional_widgets{ NumberArea->height() + spacing };
        if (ExtraOptions != nullptr)
        {
            additional_widgets += ExtraOptions->height() + spacing;
        }

        const auto height{ img_height + additional_widgets + margins.top() + margins.bottom() };
        return height;
    }

    void ApplyNumber(Project& project, int64_t number)
    {
        project.Cards[CardName].Num = static_cast<uint32_t>(std::max(std::min(number, int64_t{ 999 }), int64_t{ 0 }));
        NumberEdit->setText(QString{}.setNum(project.Cards[CardName].Num));
    }

    virtual void Refresh(Project& project)
    {
        const bool backside_changed{ BacksideEnabled != project.BacksideEnabled };
        const bool oversized_changed{ OversizedEnabled != project.OversizedEnabled };

        BacksideEnabled = project.BacksideEnabled;
        OversizedEnabled = project.OversizedEnabled;

        if (backside_changed)
        {
            auto* card_widget{ MakeCardWidget(project) };
            layout()->replaceWidget(ImageWidget, card_widget);
            std::swap(card_widget, ImageWidget);
            delete card_widget;
        }

        if (backside_changed || oversized_changed)
        {
            auto* extra_options{ MakeExtraOptions(project) };
            if (ExtraOptions == nullptr)
            {
                static_cast<QVBoxLayout*>(layout())->addWidget(extra_options);
                ExtraOptions = extra_options;
            }
            else if (extra_options == nullptr)
            {
                layout()->removeWidget(ExtraOptions);
                delete ExtraOptions;
                ExtraOptions = nullptr;
            }
            else
            {
                layout()->replaceWidget(ExtraOptions, extra_options);
                std::swap(extra_options, ExtraOptions);
                delete extra_options;
            }
        }
    }

  private:
    QWidget* MakeCardWidget(Project& project)
    {
        auto* card_image{ new CardImage{ CardName, project, CardImage::Params{} } };

        if (BacksideEnabled)
        {
            BacksideImage* backside_image{ new BacksideImage{ project.GetBacksideImage(CardName), project } };

            auto* stacked_widget{ new StackedCardBacksideView{ card_image, backside_image } };

            auto backside_reset{
                [=, this, &project]()
                {
                    project.Cards[CardName].Backside.clear();
                    auto* new_backside_image{ new BacksideImage{ project.GetBacksideImage(CardName), project } };
                    stacked_widget->RefreshBackside(new_backside_image);
                }
            };

            auto backside_choose{
                [=, this, &project]()
                {
                    if (const auto backside_choice{ OpenImageDialog(project.ImageDir) })
                    {
                        if (backside_choice.value() != project.Cards[CardName].Backside)
                        {
                            project.Cards[CardName].Backside = backside_choice.value();
                            auto* new_backside_image{ new BacksideImage{ backside_choice.value(), project } };
                            stacked_widget->RefreshBackside(new_backside_image);
                        }
                    }
                }
            };

            if (!CardName.empty())
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
        if (!BacksideEnabled && !OversizedEnabled)
        {
            return nullptr;
        }

        std::vector<QWidget*> extra_options{};

        if (BacksideEnabled)
        {
            const bool is_short_edge{ CardName.empty() ? false : project.Cards[CardName].BacksideShortEdge };

            auto* short_edge_checkbox{ new QCheckBox{ "Sideways" } };
            short_edge_checkbox->setChecked(is_short_edge);
            short_edge_checkbox->setToolTip("Determines whether to flip backside on short edge");

            QObject::connect(short_edge_checkbox,
                             &QCheckBox::checkStateChanged,
                             this,
                             std::bind_front(&CardWidget::SetShortEdge, this, std::ref(project)));

            extra_options.push_back(short_edge_checkbox);
        }

        if (OversizedEnabled)
        {
            const bool is_oversized{ CardName.empty() ? false : project.Cards[CardName].Oversized };

            auto* short_edge_checkbox{ new QCheckBox{ "Big" } };
            short_edge_checkbox->setChecked(is_oversized);
            short_edge_checkbox->setToolTip("Determines whether this is an oversized card");

            QObject::connect(short_edge_checkbox,
                             &QCheckBox::checkStateChanged,
                             this,
                             std::bind_front(&CardWidget::SetOversized, this, std::ref(project)));

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
        ApplyNumber(project, NumberEdit->text().toLongLong());
    }

    virtual void IncrementNumber(Project& project)
    {
        const auto number{ static_cast<int64_t>(project.Cards[CardName].Num) + 1 };
        ApplyNumber(project, number);
    }

    virtual void DecrementNumber(Project& project)
    {
        const auto number{ static_cast<int64_t>(project.Cards[CardName].Num) - 1 };
        ApplyNumber(project, number);
    }

    virtual void SetShortEdge(Project& project, Qt::CheckState s)
    {
        auto& card{ project.Cards[CardName] };
        card.BacksideShortEdge = s == Qt::CheckState::Checked;
    }

    virtual void SetOversized(Project& project, Qt::CheckState s)
    {
        auto& card{ project.Cards[CardName] };
        card.Oversized = s == Qt::CheckState::Checked;
    }

  protected:
    fs::path CardName;

  private:
    bool BacksideEnabled{ false };
    bool OversizedEnabled{ false };

    QWidget* ImageWidget{ nullptr };
    QLineEdit* NumberEdit{ nullptr };
    QWidget* NumberArea{ nullptr };
    QWidget* ExtraOptions{ nullptr };
};

class DummyCardWidget : public CardWidget
{
    Q_OBJECT;

  public:
    DummyCardWidget(const fs::path& card_name, Project& project)
        : CardWidget{ "", project }
    {
        CardName = card_name;

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
    virtual void EditNumber(Project&) {}
    virtual void IncrementNumber(Project&) {}
    virtual void DecrementNumber(Project&) {}
    virtual void SetShortEdge(Project&, Qt::CheckState) {}
    virtual void SetOversized(Project&, Qt::CheckState) {}
    // clang-format on
};

class CardScrollArea::CardGrid : public QWidget
{
  public:
    CardGrid(Project& project)
    {
        Refresh(project);
    }

    int TotalWidthFromItemWidth(int item_width) const
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        return item_width * Columns + margins.left() + margins.right() + spacing * (Columns - 1);
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        const auto item_width{ static_cast<float>(width - margins.left() - margins.right() - spacing * (Columns - 1)) / Columns };
        const auto item_height{ FirstItem->heightForWidth(static_cast<int>(item_width)) };

        const auto height{ item_height * Rows + margins.top() + margins.bottom() + spacing * (Rows - 1) };
        return static_cast<int>(height);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const auto width{ event->size().width() };
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);
    }

    void Refresh(Project& project)
    {
        delete layout();

        auto* this_layout{ new QGridLayout };
        this_layout->setContentsMargins(9, 9, 9, 9);
        setLayout(this_layout);

        std::unordered_map<fs::path, CardWidget*> old_cards{
            std::move(Cards)
        };
        auto eat_or_make_card{
            [&old_cards, &project](const fs::path& card_name) -> CardWidget*
            {
                auto it{ old_cards.find(card_name) };
                if (it == old_cards.end())
                {
                    return new CardWidget{ card_name, project };
                }

                CardWidget* card{ it->second };
                old_cards.erase(it);
                card->Refresh(project);
                return card;
            }
        };

        size_t i{ 0 };
        const auto cols{ CFG.DisplayColumns };
        for (auto& [card_name, _] : project.Cards)
        {
            if (ToQString(card_name).startsWith("__"))
            {
                continue;
            }

            auto* card_widget{ eat_or_make_card(card_name) };
            Cards[card_name] = card_widget;

            const auto x{ static_cast<int>(i / cols) };
            const auto y{ static_cast<int>(i % cols) };
            this_layout->addWidget(card_widget, x, y);
            ++i;
        }

        for (size_t j = i; j < cols; j++)
        {
            fs::path card_name{ fmt::format("__dummy__{}", j) };
            auto* card_widget{ eat_or_make_card(card_name) };
            Cards[std::move(card_name)] = card_widget;
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

        FirstItem = Cards.begin()->second;
        Columns = cols;
        Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / Columns));

        setMinimumWidth(TotalWidthFromItemWidth(FirstItem->minimumWidth()));
        setMinimumHeight(FirstItem->heightForWidth(FirstItem->minimumWidth()));
    }

    std::unordered_map<fs::path, CardWidget*>&
    GetCards()
    {
        return Cards;
    }

  private:
    std::unordered_map<fs::path, CardWidget*> Cards;
    CardWidget* FirstItem;

    uint32_t Columns;
    uint32_t Rows;
};

CardScrollArea::CardScrollArea(Project& project)
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

    Grid = card_grid;
}

void CardScrollArea::Refresh(Project& project)
{
    Grid->Refresh(project);
    setMinimumWidth(ComputeMinimumWidth());
}

int CardScrollArea::ComputeMinimumWidth()
{
    const auto margins{ widget()->layout()->contentsMargins() };
    return Grid->minimumWidth() + 2 * verticalScrollBar()->width() + margins.left() + margins.right();
}

void CardScrollArea::showEvent(QShowEvent* event)
{
    QScrollArea::showEvent(event);

    setMinimumWidth(ComputeMinimumWidth());
}

#include <widget_scroll_area.moc>
