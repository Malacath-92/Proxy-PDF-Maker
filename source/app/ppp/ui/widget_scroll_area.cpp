#include <ppp/ui/widget_scroll_area.hpp>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>

#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_card.hpp>

class CardWidget : public QWidget
{
    Q_OBJECT;

  public:
    CardWidget(const fs::path& card_name, Project& project)
        : CardName{ card_name }
    {
        auto* card_image{ new CardImage{ project.GetPreview(card_name).CroppedImage, CardImage::Params{} } };

        const bool backside_enabled{ project.BacksideEnabled };
        const bool oversized_enabled{ project.OversizedEnabled };

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

        BacksideImage* backside_image{ nullptr };
        if (backside_enabled)
        {
            backside_image = new BacksideImage{ project.GetBacksideImage(card_name), project };
        }

        QWidget* card_widget{ nullptr };
        if (backside_image != nullptr)
        {
            auto* stacked_widget{ new StackedCardBacksideView{ card_image, backside_image } };
            card_widget = stacked_widget;

            auto backside_reset{
                [=, &project]()
                {
                    project.Cards[card_name].Backside.clear();
                    auto* new_backside_image{ new BacksideImage{ project.GetBacksideImage(card_name), project } };
                    stacked_widget->RefreshBackside(new_backside_image);
                }
            };

            auto backside_choose{
                [=, &project]()
                {
                    if (const auto backside_choice{ OpenImageDialog(project.ImageDir) })
                    {
                        if (backside_choice.value() != project.Cards[card_name].Backside)
                        {
                            project.Cards[card_name].Backside = backside_choice.value();
                            auto* new_backside_image{ new BacksideImage{ backside_choice.value(), project } };
                            stacked_widget->RefreshBackside(new_backside_image);
                        }
                    }
                }
            };

            QObject::connect(stacked_widget,
                             &StackedCardBacksideView::BacksideReset,
                             this,
                             backside_reset);
            QObject::connect(stacked_widget,
                             &StackedCardBacksideView::BacksideClicked,
                             this,
                             backside_choose);
        }
        else
        {
            card_widget = card_image;
        }

        if (backside_enabled || oversized_enabled)
        {
            std::vector<QWidget*> extra_options{};

            if (backside_enabled)
            {
                const bool is_short_edge{ project.Cards[card_name].BacksideShortEdge };

                auto* short_edge_checkbox{ new QCheckBox{ "Sideways" } };
                short_edge_checkbox->setChecked(is_short_edge);
                short_edge_checkbox->setToolTip("Determines whether to flip backside on short edge");

                QObject::connect(short_edge_checkbox,
                                 &QCheckBox::checkStateChanged,
                                 this,
                                 std::bind_front(&CardWidget::ToggleShortEdge, this, std::ref(project)));

                extra_options.push_back(short_edge_checkbox);
            }

            if (oversized_enabled)
            {
                const bool is_oversized{ project.Cards[card_name].Oversized };

                auto* short_edge_checkbox{ new QCheckBox{ "Big" } };
                short_edge_checkbox->setChecked(is_oversized);
                short_edge_checkbox->setToolTip("Determines whether this is an oversized card");

                QObject::connect(short_edge_checkbox,
                                 &QCheckBox::checkStateChanged,
                                 this,
                                 std::bind_front(&CardWidget::ToggleOversized, this, std::ref(project)));

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

            ExtraOptions = extra_options_area;
        }

        auto* this_layout{ new QVBoxLayout };
        this_layout->addWidget(card_widget);
        this_layout->addWidget(number_area);
        if (ExtraOptions != nullptr)
        {
            this_layout->addWidget(ExtraOptions);
        }
        setLayout(this_layout);

        auto this_palette{ palette() };
        this_palette.setColor(backgroundRole(), 0x111111);
        setPalette(this_palette);
        setAutoFillBackground(true);

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
        const auto minimum_img_width{ card_image->minimumWidth() };
        const auto minimum_width{ minimum_img_width + margins.left() + margins.right() };
        setMinimumSize(minimum_width, heightForWidth(minimum_width));
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

    virtual void ToggleShortEdge(Project& project, Qt::CheckState s)
    {
        if (s == Qt::CheckState::Checked)
        {
            auto& card{ project.Cards[CardName] };
            card.BacksideShortEdge = !card.BacksideShortEdge;
        }
    }

    virtual void ToggleOversized(Project& project, Qt::CheckState s)
    {
        if (s == Qt::CheckState::Checked)
        {
            auto& card{ project.Cards[CardName] };
            card.Oversized = !card.Oversized;
        }
    }

  protected:
    fs::path CardName;

  private:
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

  private slots:
    // clang-format off
    virtual void EditNumber(Project&) {}
    virtual void IncrementNumber(Project&) {}
    virtual void DecrementNumber(Project&) {}
    virtual void ToggleShortEdge(Project&, Qt::CheckState) {}
    virtual void ToggleOversized(Project&, Qt::CheckState) {}
    // clang-format on
};

class CardScrollArea::CardGrid : public QWidget
{
  public:
    CardGrid(Project& project)
    {
        auto* this_layout{ new QGridLayout };
        this_layout->setContentsMargins(9, 9, 9, 9);
        setLayout(this_layout);

        Refresh(project);
    }

    int TotalWidthFromItemWidth(int item_width) const
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        return item_width * Columns + margins.left() + margins.right() + spacing * (Columns - 1);
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
        auto* this_layout{ static_cast<QGridLayout*>(layout()) };

        for (auto& [_, card] : Cards)
        {
            this_layout->removeWidget(card);
            delete card;
        }
        Cards.clear();

        size_t i{ 0 };
        const auto cols{ CFG.DisplayColumns };
        for (auto& [card_name, _] : project.Cards)
        {
            if (std::basic_string_view{ card_name.c_str() }.starts_with(L"__") || !project.Previews.contains(card_name))
            {
                continue;
            }

            auto* card_widget{ new CardWidget{ card_name, project } };
            Cards[card_name] = card_widget;

            const auto x{ static_cast<int>(i / cols) };
            const auto y{ static_cast<int>(i % cols) };
            this_layout->addWidget(card_widget, x, y);
            ++i;
        }

        for (size_t j = i; j < cols; j++)
        {
            fs::path card_name{ fmt::format("__dummy__{}", j) };
            auto* card_widget{ new DummyCardWidget{ card_name, project } };
            Cards[std::move(card_name)] = card_widget;
            this_layout->addWidget(card_widget, 0, static_cast<int>(j));
            ++i;
        }

        FirstItem = Cards.begin()->second;
        Columns = cols;
        Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / Columns));

        setMinimumWidth(TotalWidthFromItemWidth(FirstItem->minimumWidth()));
        setMinimumHeight(FirstItem->heightForWidth(FirstItem->minimumWidth()));
    }

    std::unordered_map<fs::path, CardWidget*>& GetCards()
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
    Grid->adjustSize();
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
