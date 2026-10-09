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

    m_Grid = new SelectableCardGrid{ m_ViewModel.MakeGridViewModel() };

    auto* grid_scroll{ new QScrollArea };
    grid_scroll->setWidget(m_Grid);
    grid_scroll->setWidgetResizable(true);
    grid_scroll->setMinimumWidth(
        [=, this]()
        {
            const auto margins{ m_Grid->layout()->contentsMargins() };
            return m_Grid->minimumWidth() + 2 * grid_scroll->verticalScrollBar()->width() + margins.left() + margins.right();
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

void MtGCardBrowserPopup::Show()
{
    PopupBase::Show();
    // return m_Choice == Choice::Ok ? m_Grid->GetSelectedCardName() : std::nullopt;
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
