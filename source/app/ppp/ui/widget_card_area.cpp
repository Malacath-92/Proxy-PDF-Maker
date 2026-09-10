#include <ppp/ui/widget_card_area.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/popups/popups.hpp>
#include <ppp/ui/widget_util/widget_card.hpp>

#include <ppp/ui/popups/decklist_popup.hpp>
#include <ppp/ui/popups/image_browse_popup.hpp>

#include <ppp/ui/view_models/util.hpp>
#include <ppp/ui/view_models/view_model_card_area.hpp>
#include <ppp/ui/view_models/view_model_card_area_card.hpp>

#include <ppp/profile/profile.hpp>

class CardAreaCardWidget : public QFrame
{
    Q_OBJECT

  public:
    CardAreaCardWidget(CardAreaCardViewModel* view_model)
        : m_ViewModel{ *view_model }
    {
        TRACY_AUTO_SCOPE();

        m_ViewModel.setParent(this);

        m_NumberEdit = new QLineEdit;
        m_NumberEdit->setValidator(new QIntValidator{ 0, 999, this });
        m_NumberEdit->setText("0");
        m_NumberEdit->setFixedWidth(40);

        auto* decrement_button{ new QPushButton{ "-" } };
        decrement_button->setToolTip("Remove one");
        decrement_button->setMinimumWidth(20);

        auto* increment_button{ new QPushButton{ "+" } };
        increment_button->setToolTip("Add one");
        increment_button->setMinimumWidth(20);

        auto* number_layout{ new QHBoxLayout };
        number_layout->addStretch();
        number_layout->addWidget(decrement_button);
        number_layout->addWidget(m_NumberEdit);
        number_layout->addWidget(increment_button);
        number_layout->addStretch();

        {
            QMargins margins{ number_layout->contentsMargins() };
            margins.setLeft(0);
            margins.setRight(0);
            number_layout->setContentsMargins(margins);
        }

        m_NumberArea = new QWidget;
        m_NumberArea->setLayout(number_layout);
        m_NumberArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        m_NumberArea->setMaximumHeight(m_NumberArea->sizeHint().height());

        MakeCardWidget();
        MakeWithBacksideWidget();
        MakeExtraOptions();

        auto* this_layout{ new QVBoxLayout };
        this_layout->addWidget(m_CardWidget);
        this_layout->addWidget(m_NumberArea);
        this_layout->addWidget(m_ExtraOptions);
        setLayout(this_layout);

        QObject::connect(m_NumberEdit,
                         &QLineEdit::editingFinished,
                         &m_ViewModel,
                         [this]()
                         {
                             m_ViewModel.SetCardCount(m_NumberEdit->text());
                         });
        QObject::connect(decrement_button,
                         &QPushButton::clicked,
                         &m_ViewModel,
                         &CardAreaCardViewModel::DecrementCard);
        QObject::connect(increment_button,
                         &QPushButton::clicked,
                         &m_ViewModel,
                         &CardAreaCardViewModel::IncrementCard);

        const auto margins{ layout()->contentsMargins() };
        const auto minimum_img_width{ m_CardWidget->minimumWidth() };
        const auto minimum_width{ std::max(minimum_img_width + margins.left() + margins.right(), 160) };
        setMinimumSize(minimum_width, CardAreaCardWidget::heightForWidth(minimum_width));

        setFrameShape(Shape::Box);
        setFrameShadow(Shadow::Raised);

        FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideEnabledChanged);
        FORWARD_SIGNAL_FROM_VIEW_MODEL(CardCountChanged);
        FORWARD_SIGNAL_FROM_VIEW_MODEL(CardBacksideShortEdgeChanged);
        FORWARD_SIGNAL_FROM_VIEW_MODEL(CardBacksideChanged);

        m_ViewModel.EmitDefaults();
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override
    {
        const auto margins{ layout()->contentsMargins() };
        const auto spacing{ layout()->spacing() };

        const auto* img_widget{
            m_CardWidget->isVisible() ? static_cast<QWidget*>(m_CardWidget)
                                      : m_WithBacksideWidget
        };

        const auto img_width{ width - margins.left() - margins.right() };
        const auto img_height{ img_widget->heightForWidth(img_width) };

        const auto number_area{ m_NumberArea->height() + spacing };
        const auto extra_options{ m_ExtraOptions->isVisible()
                                      ? m_ExtraOptions->height() + spacing
                                      : 0 };

        const auto height{ img_height + number_area + extra_options + margins.top() + margins.bottom() };
        return height;
    }

  private:
    void MakeCardWidget()
    {
        TRACY_AUTO_SCOPE();

        auto* card_view_model{ m_ViewModel.MakeCardViewModel() };

        m_CardWidget = new CardImage{ card_view_model };
        m_CardWidget->EnableContextMenu(true);
        m_CardWidget->setVisible(false);
    }
    void MakeWithBacksideWidget()
    {
        TRACY_AUTO_SCOPE();

        auto* card_view_model{ m_ViewModel.MakeCardViewModel() };

        auto* card_image{ new CardImage{ card_view_model } };
        card_image->EnableContextMenu(true);

        auto* backside_view_model{ m_ViewModel.MakeBacksideCardViewModel() };
        auto* blank_view_model{ m_ViewModel.MakeBlankCardViewModel() };

        auto* backside_image{
            new ClearableCardImage{
                backside_view_model,
                blank_view_model,
                !m_ViewModel.HasBackside(),
            }
        };
        m_WithBacksideWidget = new StackedCardBacksideView{ card_image, backside_image };
        m_WithBacksideWidget->setVisible(false);

        auto backside_choose{
            [this]()
            {
                auto* image_browser_view_model{ m_ViewModel.MakeImageBrowseViewModel() };
                ImageBrowsePopup image_browser{ window(), image_browser_view_model };
                image_browser.setWindowTitle(QString{ "Choose backside for %1" }.arg(ToQString(m_ViewModel.GetCardName())));

                if (const auto backside_choice{ image_browser.Show() })
                {
                    const auto& backside{ backside_choice.value() };
                    m_ViewModel.SetBacksideImage(backside);
                }
                else if (image_browser.GetChoice() == ImageBrowsePopup::Choice::Clear)
                {
                    m_ViewModel.ClearBacksideImage();
                }
                else if (image_browser.GetChoice() == ImageBrowsePopup::Choice::Reset)
                {
                    m_ViewModel.SetBacksideImageDefault();
                }
            }
        };

        QObject::connect(m_WithBacksideWidget,
                         &StackedCardBacksideView::BacksideClicked,
                         this,
                         backside_choose);
    }
    void MakeExtraOptions()
    {
        TRACY_AUTO_SCOPE();

        m_BacksideShortEdge = new QCheckBox{ "Sideways" };
        m_BacksideShortEdge->setChecked(false);
        m_BacksideShortEdge->setToolTip("Determines whether to flip backside on short edge");

        QObject::connect(m_BacksideShortEdge,
                         &QCheckBox::checkStateChanged,
                         &m_ViewModel,
                         &CardAreaCardViewModel::SetCardBacksideShortEdge);

        auto* extra_options_layout{ new QHBoxLayout };
        extra_options_layout->addStretch();
        extra_options_layout->addWidget(m_BacksideShortEdge);
        extra_options_layout->addStretch();
        extra_options_layout->setContentsMargins(0, 0, 0, 0);

        m_ExtraOptions = new QWidget;
        m_ExtraOptions->setLayout(extra_options_layout);
        m_ExtraOptions->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        m_ExtraOptions->setMaximumHeight(m_ExtraOptions->sizeHint().height());
    }

  private slots:
    void BacksideEnabledChanged(bool backside_enabled)
    {
        auto* vbox_layout{ static_cast<QVBoxLayout*>(layout()) };
        auto* first_item{ vbox_layout->itemAt(0) };
        first_item->widget()->setVisible(false);
        vbox_layout->removeItem(first_item);
        if (backside_enabled)
        {
            vbox_layout->insertWidget(0, m_WithBacksideWidget);
            m_WithBacksideWidget->setVisible(true);
            m_ExtraOptions->setVisible(true);
        }
        else
        {
            vbox_layout->insertWidget(0, m_CardWidget);
            m_CardWidget->setVisible(true);
            m_ExtraOptions->setVisible(false);
        }
    }
    void CardCountChanged(uint32_t count)
    {
        m_NumberEdit->setText(QString{}.setNum(count));
    }
    void CardBacksideShortEdgeChanged(bool card_backside_short_edge)
    {
        m_BacksideShortEdge->setChecked(card_backside_short_edge);
    }
    void CardBacksideChanged(OptionalImageRef backside)
    {
        m_WithBacksideWidget->RefreshBackside(backside);
    }

  private:
    CardAreaCardViewModel& m_ViewModel;

    CardImage* m_CardWidget{ nullptr };
    StackedCardBacksideView* m_WithBacksideWidget;
    QLineEdit* m_NumberEdit{ nullptr };
    QWidget* m_NumberArea{ nullptr };
    QWidget* m_ExtraOptions{ nullptr };
    QCheckBox* m_BacksideShortEdge{ nullptr };
};

class DummyCardWidget : public CardAreaCardWidget
{
    Q_OBJECT

  public:
    DummyCardWidget(CardAreaCardViewModel* view_model)
        : CardAreaCardWidget{ view_model }
    {
        TRACY_AUTO_SCOPE();

        auto sp_retain{ sizePolicy() };
        sp_retain.setRetainSizeWhenHidden(true);
        setSizePolicy(sp_retain);
        hide();
    }
};

class CardGrid : public QWidget
{
    Q_OBJECT

  public:
    CardGrid(CardAreaViewModel& view_model)
        : m_ViewModel{ view_model }
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

    void FullRefresh()
    {
        TRACY_AUTO_SCOPE();

        const auto cols{ m_ViewModel.GetDisplayColumns() };
        for (size_t j = m_Dummies.size(); j < cols; j++)
        {
            fs::path card_name{ fmt::format("__dummy__{}", j) };
            auto* dummy{ new DummyCardWidget{ m_ViewModel.MakeCardViewModel(card_name) } };
            m_Dummies.push_back(dummy);
        }

        {
            std::unordered_map<fs::path, CardAreaCardWidget*> old_cards{
                std::move(m_Cards)
            };
            m_Cards = {};

            auto eat_or_make_card{
                [this, &old_cards](const fs::path& card_name) -> CardAreaCardWidget*
                {
                    auto it{ old_cards.find(card_name) };
                    if (it == old_cards.end())
                    {
                        return new CardAreaCardWidget{ m_ViewModel.MakeCardViewModel(card_name) };
                    }

                    CardAreaCardWidget* card{ it->second };
                    old_cards.erase(it);
                    return card;
                }
            };

            for (const auto& card_info : m_ViewModel.GetCards())
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
        TRACY_SCOPE_INFO_FMT("Filter: \"{}\"", filter.isEmpty() ? "<none>" : filter.toStdString().c_str());

        m_CurrentFilter = filter;
        m_FirstItem = nullptr;

        {
            TRACY_AUTO_SCOPE();
            TRACY_SCOPE_NAME(destroy_old_layout);

            if (auto* old_layout{ static_cast<QGridLayout*>(layout()) })
            {
                for (auto& [card_name, card] : m_Cards)
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
        }

        auto* this_layout{ new QGridLayout };
        this_layout->setContentsMargins(9, 9, 9, 9);
        setLayout(this_layout);

        const auto cols{ m_ViewModel.GetDisplayColumns() };

        const QString filter_lower{ filter.toLower() };
        size_t i{ 0 };

        {
            TRACY_AUTO_SCOPE();
            TRACY_SCOPE_NAME(filter_cards);

            for (const auto& card_info : m_ViewModel.GetCards())
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

                TRACY_AUTO_SCOPE();
                TRACY_SCOPE_NAME(accept_card);

                auto* card_widget{ m_Cards.at(card_name) };
                if (m_FirstItem == nullptr)
                {
                    m_FirstItem = card_widget;
                }

                const auto x{ static_cast<int>(i / cols) };
                const auto y{ static_cast<int>(i % cols) };
                this_layout->addWidget(card_widget, x, y);
                card_widget->show();
                ++i;
            }
        }

        if (i < cols)
        {
            TRACY_AUTO_SCOPE();
            TRACY_SCOPE_NAME(fill_up_with_dummies);

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
            TRACY_AUTO_SCOPE();
            TRACY_SCOPE_NAME(set_column_stretch);

            for (int c = 0; c < this_layout->columnCount(); c++)
            {
                this_layout->setColumnStretch(c, 1);
            }
        }

        m_Columns = cols;
        m_Rows = static_cast<uint32_t>(std::ceil(static_cast<float>(i) / m_Columns));

        RefreshSize();
    }

    void RefreshSize()
    {
        TRACY_AUTO_SCOPE();

        setMinimumWidth(TotalWidthFromItemWidth(m_FirstItem->minimumWidth()));
        setMinimumHeight(heightForWidth(minimumWidth()));
    }

    int MaximumColumnsFromAvailableWidth(int available_width) const
    {
        const auto image_minimum_size{
            m_FirstItem->minimumWidth()
        };
        return available_width / image_minimum_size;
    }

    bool HasCard(const fs::path& card_name) const
    {
        return m_Cards.contains(card_name);
    }

    bool HasCards() const
    {
        return !m_Cards.empty();
    }

    std::unordered_map<fs::path, CardAreaCardWidget*>& GetCards()
    {
        return m_Cards;
    }

  private:
    CardAreaViewModel& m_ViewModel;

    std::unordered_map<fs::path, CardAreaCardWidget*> m_Cards;
    std::vector<CardAreaCardWidget*> m_Dummies;
    CardAreaCardWidget* m_FirstItem;

    uint32_t m_Columns;
    uint32_t m_Rows;

    QString m_CurrentFilter{ "" };
};

class CardScrollArea : public QScrollArea
{
    Q_OBJECT

  public:
    CardScrollArea(CardAreaViewModel& view_model);

    CardGrid& GetGrid()
    {
        return *m_Grid;
    }

    void FullRefresh();

    int MaximumColumnsFromAvailableWidth(int available_width) const;

    void ApplyFilter(const QString& filter);

  private:
    int ComputeMinimumWidth() const;

    virtual void showEvent(QShowEvent* event) override;

    const CardAreaViewModel& m_ViewModel;
    CardGrid* m_Grid;
};

CardScrollArea::CardScrollArea(CardAreaViewModel& view_model)
    : m_ViewModel{ view_model }
    , m_Grid{ new CardGrid{ view_model } }
{
    TRACY_AUTO_SCOPE();

    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);
    setWidget(m_Grid);

    setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
}

void CardScrollArea::FullRefresh()
{
    m_Grid->FullRefresh();
    setMinimumWidth(ComputeMinimumWidth());
}

int CardScrollArea::MaximumColumnsFromAvailableWidth(int available_width) const
{
    const auto margins{ contentsMargins() };
    const auto scrollbar_width{ verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0 };
    available_width -= margins.left() +
                       margins.right() +
                       scrollbar_width;
    return m_Grid->MaximumColumnsFromAvailableWidth(available_width);
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
}

CardArea::CardArea(CardAreaViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    m_ViewModel.setParent(this);

    m_RefreshTimer.setSingleShot(true);
    m_RefreshTimer.setInterval(50);
    QObject::connect(&m_RefreshTimer,
                     &QTimer::timeout,
                     this,
                     &CardArea::FullRefresh);

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(init_onboarding);

        auto* onboarding_line_1{ new QLabel{ "No images are loaded..." } };
        auto* onboarding_line_2{ new QLabel{
            QString(
                "To start either add images into the <a href=\"file:///%1\">image folder</a>, drag-and-drop")
                .arg(m_ViewModel.GetImageDir().replace(' ', "%20")),
        } };
        auto* onboarding_line_3{ new QLabel{
            "images onto the app, or enable one of the <a href=\"#plugins\">plugins</a>.",
        } };
        onboarding_line_3->setOpenExternalLinks(false);

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
        auto plugins_link_activated{
            [this](const QString& link)
            {
                if (link == "#plugins")
                {
                    RequestOpenPluginsWindow();
                }
            }
        };

        QObject::connect(onboarding_line_2,
                         &QLabel::linkActivated,
                         this,
                         image_folder_link_activated);
        QObject::connect(onboarding_line_3,
                         &QLabel::linkActivated,
                         this,
                         plugins_link_activated);
    }

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(init_header);

        auto* global_label{ new QLabel{ "Global Controls:" } };
        auto* global_decrement_button{ new QPushButton{ "-" } };
        auto* global_increment_button{ new QPushButton{ "+" } };
        auto* global_set_zero_button{ new QPushButton{ "Zero All" } };
        auto* global_decklist_button{ new QPushButton{ "From Decklist" } };
        m_Filter = new QLineEdit;
        m_Filter->setPlaceholderText("Filter");
        m_RemoveExternalCards = new QPushButton{ "Remove All External Cards" };

        global_decrement_button->setMinimumWidth(20);
        global_increment_button->setMinimumWidth(20);

        global_decrement_button->setToolTip("Remove one from all");
        global_increment_button->setToolTip("Add one to all");
        global_set_zero_button->setToolTip("Set all to zero");
        global_decklist_button->setToolTip("Set amounts from a decklist (matching filenames)");
        m_Filter->setToolTip("Filter by filename");
        m_RemoveExternalCards->setToolTip("Removes all cards not part of the images folder");

        auto* header_layout{ new QHBoxLayout };
        header_layout->addWidget(global_label);
        header_layout->addWidget(global_decrement_button);
        header_layout->addWidget(global_increment_button);
        header_layout->addWidget(global_set_zero_button);
        header_layout->addWidget(global_decklist_button);
        header_layout->addWidget(m_Filter);
        header_layout->addWidget(m_RemoveExternalCards);
        header_layout->addStretch();
        header_layout->setContentsMargins(6, 0, 6, 0);

        m_Header = new QWidget;
        m_Header->setLayout(header_layout);

        auto open_decklist{
            [this]()
            {
                window()->setEnabled(false);
                {
                    // TODO: No Project
                    auto& project{ m_ViewModel.m_Project };
                    DecklistPopup decklist_popup{ nullptr, project };

                    QObject::connect(
                        &decklist_popup,
                        &DecklistPopup::DecklistChanged,
                        [this, &project](const std::unordered_map<fs::path, uint32_t>& decklist)
                        {
                            for (const auto& card : project.m_Data.m_Cards)
                            {
                                const auto card_count{
                                    [&]()
                                    {
                                        auto it{ decklist.find(card.m_Name) };
                                        if (it == decklist.end())
                                        {
                                            it = decklist.find(card.Stem());
                                        }

                                        if (it != decklist.end())
                                        {
                                            return it->second;
                                        }
                                        else
                                        {
                                            return 0u;
                                        }
                                    }()
                                };

                                project.SetCardCount(card.m_Name, card_count);
                            }
                        });

                    decklist_popup.Show();
                }
                window()->setEnabled(true);
            }
        };

        auto apply_filter{
            [this](const QString& text)
            {
                m_ScrollArea->ApplyFilter(text);
            }
        };

        QObject::connect(global_decrement_button,
                         &QPushButton::clicked,
                         &m_ViewModel,
                         &CardAreaViewModel::DecrementAllCards);
        QObject::connect(global_increment_button,
                         &QPushButton::clicked,
                         &m_ViewModel,
                         &CardAreaViewModel::DecrementAllCards);
        QObject::connect(global_set_zero_button,
                         &QPushButton::clicked,
                         &m_ViewModel,
                         &CardAreaViewModel::ResetAllCards);
        QObject::connect(global_decklist_button,
                         &QPushButton::clicked,
                         this,
                         open_decklist);
        QObject::connect(m_Filter,
                         &QLineEdit::textChanged,
                         this,
                         apply_filter);
        QObject::connect(m_RemoveExternalCards,
                         &QPushButton::clicked,
                         &m_ViewModel,
                         &CardAreaViewModel::RemoveAllExternalCards);
    }

    m_ScrollArea = new CardScrollArea{ m_ViewModel };

    auto* card_area_layout{ new QVBoxLayout };
    card_area_layout->addWidget(m_OnboardingHint);
    card_area_layout->addWidget(m_Header);
    card_area_layout->addWidget(m_ScrollArea);

    setLayout(card_area_layout);

    const auto& grid{ m_ScrollArea->GetGrid() };
    m_OnboardingHint->setVisible(!grid.HasCards());
    m_Header->setVisible(grid.HasCards());
    m_ScrollArea->setVisible(grid.HasCards());

    QObject::connect(&m_ViewModel,
                     &CardAreaViewModel::RequestRefresh,
                     this,
                     &CardArea::QueueRefresh);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardSizeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(HasExternalCardsChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardAdded);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardRemoved);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardRenamed);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardVisibilityChanged);

    m_ViewModel.EmitDefaults();
}

int CardArea::MaximumColumnsFromAvailableWidth(int available_width) const
{
    const auto margins{ contentsMargins() };
    available_width -= layout()->spacing() +
                       margins.left() +
                       margins.right();
    return m_ScrollArea->MaximumColumnsFromAvailableWidth(available_width);
}

void CardArea::CardSizeChanged(Size /* card_size */)
{
    // TODO: Get rid of this!!!

    // This is the stupidest code I have ever written...
    // Nothing that should usually be working to make sure widget's size
    // changes trickled down to this scroll area will work here (or I am
    // just stupid) so we just wiggle the window as that at least forces
    // the recalculation of the grid
    auto* window{ this->window() };
    if (window)
    {
        const auto current_size{ window->size() };
        window->resize(current_size.width(), current_size.height() - 1);
        window->resize(current_size);
    }
}

void CardArea::HasExternalCardsChanged(bool has_external_cards)
{
    m_RemoveExternalCards->setVisible(has_external_cards);
}

void CardArea::CardAdded(const fs::path& card_name)
{
    const auto& grid{ m_ScrollArea->GetGrid() };
    if (grid.HasCard(card_name))
    {
        return;
    }

    QueueRefresh();
}
void CardArea::CardRemoved(const fs::path& card_name)
{
    const auto& grid{ m_ScrollArea->GetGrid() };
    if (!grid.HasCard(card_name))
    {
        return;
    }

    QueueRefresh();
}
void CardArea::CardRenamed(const fs::path& old_card_name, const fs::path& /*new_card_name*/)
{
    const auto& grid{ m_ScrollArea->GetGrid() };
    if (!grid.HasCard(old_card_name))
    {
        return;
    }

    QueueRefresh();
}
void CardArea::CardVisibilityChanged(const fs::path& card_name, bool visible)
{
    const auto& grid{ m_ScrollArea->GetGrid() };
    if (visible && !grid.HasCard(card_name))
    {
        QueueRefresh();
    }
    else if (!visible && grid.HasCard(card_name))
    {
        QueueRefresh();
    }
}

void CardArea::FullRefresh()
{
    m_ScrollArea->FullRefresh();

    const auto& grid{ m_ScrollArea->GetGrid() };
    m_OnboardingHint->setVisible(!grid.HasCards());
    m_Header->setVisible(grid.HasCards());
    m_ScrollArea->setVisible(grid.HasCards());
}

void CardArea::QueueRefresh()
{
    m_RefreshTimer.start();
}

#include <widget_card_area.moc>
