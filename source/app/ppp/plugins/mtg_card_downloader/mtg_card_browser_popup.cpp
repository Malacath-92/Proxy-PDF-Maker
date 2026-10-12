#include <ppp/plugins/mtg_card_downloader/mtg_card_browser_popup.hpp>

#include <ranges>

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>

#include <ppp/project/card_info.hpp>

#include <ppp/ui/widget_util/card/widget_selectable_card.hpp>
#include <ppp/ui/widget_util/card/widget_selectable_card_grid.hpp>
#include <ppp/ui/widget_util/widget_collapse_container.hpp>
#include <ppp/ui/widget_util/widget_label.hpp>

#include <ppp/plugins/mtg_card_downloader/view_models/view_model_downloaded_mtg_card_grid.hpp>
#include <ppp/plugins/mtg_card_downloader/view_models/view_model_mtg_card_browser.hpp>

MtGCardBrowserPopup::MtGCardBrowserPopup(QWidget* parent,
                                         MtGCardBrowserViewModel* view_model)
    : PopupBase{ parent }
    , m_ViewModel{ *view_model }
{
    view_model->setParent(this);

    m_AutoCenter = false;
    m_AutoCenterOnShow = false;

    setWindowFlags(Qt::WindowType::Dialog);
    setWindowTitle("Choose Image");

    m_Filter = new QLineEdit;
    m_Filter->setPlaceholderText("Filter");

    auto* scryfall_query{ new LineEditWithLabel{ "Query", m_ViewModel.GetQuery().toStdString() } };
    m_ScryfallQuery = scryfall_query->GetWidget();

    m_GridViewModel = m_ViewModel.MakeGridViewModel();
    m_Grid = new SelectableCardGrid{ m_GridViewModel };

    auto* scryfall_layout{ new QVBoxLayout };
    scryfall_layout->addWidget(scryfall_query);
    scryfall_layout->addWidget(m_Grid);

    auto* widget_scryfall{ new QWidget };
    widget_scryfall->setObjectName("Scryfall");
    widget_scryfall->setLayout(scryfall_layout);

    auto* container_scryfall{ new CollapseContainer{ widget_scryfall, false } };
    container_scryfall->SetRelativeRate(6000_pix / 1_s);

    auto* content_scroll{ new QScrollArea };
    content_scroll->setWidget(container_scryfall);
    content_scroll->setWidgetResizable(true);
    content_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    content_scroll->setMinimumWidth(
        [=, this]()
        {
            const auto margins{ m_Grid->layout()->contentsMargins() };
            return m_Grid->minimumWidth() + 2 * content_scroll->verticalScrollBar()->width() + margins.left() + margins.right();
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
    window_buttons->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    auto* outer_layout{ new QVBoxLayout };
    outer_layout->addWidget(m_Filter);
    outer_layout->addWidget(content_scroll);
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
                     std::bind_front(&MtGCardBrowserPopup::CloseWithChoice, this, Choice::Ok));
    QObject::connect(cancel_button,
                     &QPushButton::clicked,
                     this,
                     std::bind_front(&MtGCardBrowserPopup::CloseWithChoice, this, Choice::Cancel));

    const auto parent_rect{ parent != nullptr
                                ? parent->rect()
                                : QApplication::primaryScreen()->geometry() };
    resize(parent_rect.width() - 100, parent_rect.height() - 100);
}

std::optional<QString> MtGCardBrowserPopup::Show()
{
    PopupBase::Show();
    if (m_Choice == Choice::Ok)
    {
        if (auto selected_card_name{ m_Grid->GetSelectedCardName() })
        {
            return m_GridViewModel->GetFullResImageURI(ToQString(selected_card_name.value()));
        }
    }
    return std::nullopt;
}
std::optional<ImagePreview> MtGCardBrowserPopup::GetSelectedPreview() const
{
    if (m_Choice == Choice::Ok)
    {
        if (auto selected_card_name{ m_Grid->GetSelectedCardName() })
        {
            return m_GridViewModel->GetPreview(ToQString(selected_card_name.value()));
        }
    }
    return std::nullopt;
}

std::optional<QString> MtGCardBrowserPopup::GetSelectedBackside() const
{
    if (m_Choice == Choice::Ok)
    {
        if (auto selected_backside_name{ m_Grid->GetSelectedBacksideName() })
        {
            return ToQString(selected_backside_name.value());
        }
    }
    return std::nullopt;
}
std::optional<QString> MtGCardBrowserPopup::GetSelectedBacksideURI() const
{
    if (m_Choice == Choice::Ok)
    {
        if (auto selected_backside_name{ m_Grid->GetSelectedBacksideName() })
        {
            return m_GridViewModel->GetFullResImageURI(ToQString(selected_backside_name.value()));
        }
    }
    return std::nullopt;
}
std::optional<ImagePreview> MtGCardBrowserPopup::GetSelectedBacksidePreview() const
{
    if (m_Choice == Choice::Ok)
    {
        if (auto selected_backside_name{ m_Grid->GetSelectedBacksideName() })
        {
            return m_GridViewModel->GetPreview(ToQString(selected_backside_name.value()));
        }
    }
    return std::nullopt;
}

MtGCardBrowserPopup::Choice MtGCardBrowserPopup::GetChoice() const
{
    return m_Choice;
}

void MtGCardBrowserPopup::CloseWithChoice(Choice choice)
{
    m_Choice = choice;
    close();
}
