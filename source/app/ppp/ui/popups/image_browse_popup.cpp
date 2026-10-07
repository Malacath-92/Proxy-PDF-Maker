#include <ppp/ui/popups/image_browse_popup.hpp>

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

#include <ppp/ui/view_models/popups/view_model_image_browse_popup.hpp>

ImageBrowsePopup::ImageBrowsePopup(QWidget* parent,
                                   ImageBrowseViewModel* view_model)
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

    const auto has_cards{ m_ViewModel.HasCards() };

    QWidget* grid{ nullptr };
    if (has_cards)
    {
        m_Grid = new SelectableCardGrid{ m_ViewModel.MakeGridViewModel() };
        grid = m_Grid;
    }
    else
    {
        const auto has_ignored_cards{ m_ViewModel.HasIgnoredCards() };
        auto* error_label{ new QLabel{ !has_ignored_cards ? "No cards loaded..."
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
    auto* clear_button{ new QPushButton{ "Clear" } };
    auto* reset_button{ new QPushButton{ "Reset" } };
    auto* cancel_button{ new QPushButton{ "Cancel" } };

    auto* buttons_layout{ new QHBoxLayout };
    buttons_layout->setContentsMargins(0, 0, 0, 0);
    buttons_layout->addStretch();
    buttons_layout->addWidget(ok_button);
    buttons_layout->addWidget(clear_button);
    buttons_layout->addWidget(reset_button);
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
                     std::bind_front(&ImageBrowsePopup::CloseWithChoice, this, Choice::Ok));
    QObject::connect(clear_button,
                     &QPushButton::clicked,
                     this,
                     std::bind_front(&ImageBrowsePopup::CloseWithChoice, this, Choice::Clear));
    QObject::connect(reset_button,
                     &QPushButton::clicked,
                     this,
                     std::bind_front(&ImageBrowsePopup::CloseWithChoice, this, Choice::Reset));
    QObject::connect(cancel_button,
                     &QPushButton::clicked,
                     this,
                     std::bind_front(&ImageBrowsePopup::CloseWithChoice, this, Choice::Cancel));

    const auto parent_rect{ parent != nullptr
                                ? parent->rect()
                                : QApplication::primaryScreen()->geometry() };
    resize(parent_rect.width() - 100, parent_rect.height() - 100);
}

std::optional<fs::path> ImageBrowsePopup::Show()
{
    PopupBase::Show();
    return m_Choice == Choice::Ok ? m_Grid->GetSelectedCardName() : std::nullopt;
}

ImageBrowsePopup::Choice ImageBrowsePopup::GetChoice() const
{
    return m_Choice;
}

void ImageBrowsePopup::CloseWithChoice(Choice choice)
{
    m_Choice = choice;
    close();
}
