#include <ppp/ui/widget_print_preview.hpp>

#include <ranges>

#include <QDragMoveEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_print_preview_card.hpp>
#include <ppp/ui/widget_print_preview_page.hpp>

PrintPreview::PrintPreview(Project& project)
    : m_Project{ project }
{
    m_RefreshTimer.setSingleShot(true);
    m_RefreshTimer.setInterval(50);
    QObject::connect(&m_RefreshTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         Refresh();
                     });

    Refresh();

    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);

    // We only do this to get QDragMoveEvent
    setAcceptDrops(true);

    m_DragScrollTimer.setInterval(1);
    m_DragScrollTimer.setSingleShot(false);
    QObject::connect(&m_DragScrollTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         if (const auto diff{ ComputeDragScrollDiff() })
                         {
                             verticalScrollBar()->setValue(verticalScrollBar()->value() - diff);
                         }
                     });

    m_NumberTypeTimer.setSingleShot(true);
    m_NumberTypeTimer.setInterval(500);
    QObject::connect(&m_NumberTypeTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         m_TargetPage.reset();
                     });
}

void PrintPreview::Refresh()
{
    if (!isVisible())
    {
        return;
    }

    const auto current_scroll{ verticalScrollBar()->value() };
    if (auto* current_widget{ widget() })
    {
        delete current_widget;
    }

    const auto raw_pages{ DistributeCardsToPages(m_Project) };
    const auto page_size{ m_Project.ComputePageSize() };

    // Show empty preview when no cards can fit on the page
    if (raw_pages.empty())
    {
        m_FrontsideTransforms.clear();

        auto* empty_label{ new QLabel{ "No cards can fit on the page with current settings.\nPlease adjust page size, margins, or card size." } };
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("QLabel{ color: red; font-size: 14px; }");

        auto* empty_layout{ new QVBoxLayout };
        empty_layout->setSpacing(15);
        empty_layout->setContentsMargins(60, 20, 60, 20);

        empty_layout->addWidget(empty_label);
        empty_layout->addWidget(new PagePreview{
            m_Project,
            nullptr,
            Page{},
            m_FrontsideTransforms,
            PagePreview::Params{
                .m_PageSize{ page_size },
                .m_IsBackside = false,
            },
        });

        auto* empty_widget{ new QWidget };
        empty_widget->setLayout(empty_layout);

        setWidget(empty_widget);

        verticalScrollBar()->setValue(current_scroll);

        return;
    }

    m_FrontsideTransforms = ComputeTransforms(m_Project);

    struct TempPage
    {
        Page m_Page;
        std::reference_wrapper<const PageImageTransforms> m_Transforms;
        bool m_Backside;
    };
    auto pages{ raw_pages |
                std::views::transform([this](const Page& page)
                                      { return TempPage{
                                            page,
                                            m_FrontsideTransforms,
                                            false,
                                        }; }) |
                std::ranges::to<std::vector>() };

    if (m_Project.m_Data.m_BacksideEnabled)
    {
        m_BacksideTransforms = ComputeBacksideTransforms(m_Project, m_FrontsideTransforms);

        const auto raw_backside_pages{ MakeBacksidePages(m_Project, raw_pages) };
        const auto backside_pages{ raw_backside_pages |
                                   std::views::transform([this](const Page& page)
                                                         { return TempPage{
                                                               page,
                                                               m_BacksideTransforms,
                                                               true,
                                                           }; }) |
                                   std::ranges::to<std::vector>() };

        for (size_t i = 0; i < backside_pages.size(); i++)
        {
            pages.insert(pages.begin() + (2 * i + 1), backside_pages[i]);
        }
    }

    auto page_widgets{
        pages |
        std::views::transform(
            [&](const TempPage& page)
            {
                return new PagePreview{
                    m_Project,
                    this,
                    page.m_Page,
                    page.m_Transforms.get(),
                    PagePreview::Params{
                        page_size,
                        page.m_Backside,
                    }
                };
            }) |
        std::ranges::to<std::vector>()
    };

    for (auto* page : page_widgets)
    {
        QObject::connect(page,
                         &PagePreview::DragStarted,
                         this,
                         [this]()
                         {
                             m_Dragging = true;
                             m_DraggingStarted = true;
                             m_DragScrollTimer.start();
                         });
        QObject::connect(page,
                         &PagePreview::DragFinished,
                         this,
                         [this]()
                         {
                             m_Dragging = false;
                             m_DragScrollTimer.stop();
                         });
        QObject::connect(page,
                         &PagePreview::ReorderCards,
                         this,
                         &PrintPreview::ReorderCards);
    }

    auto* restore_order_button{ new QPushButton{ "Restore Original Order" } };
    QObject::connect(restore_order_button,
                     &QPushButton::clicked,
                     this,
                     &PrintPreview::RestoreCardsOrder);
    {
        QSizePolicy size_policy{ restore_order_button->sizePolicy() };
        size_policy.setRetainSizeWhenHidden(true);
        restore_order_button->setSizePolicy(size_policy);
    }

    auto* header_layout{ new QHBoxLayout };
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->addWidget(new QLabel{ "Only a preview; Quality is lower than final render" });
    header_layout->addWidget(restore_order_button);
    header_layout->addStretch();

    auto* header{ new QWidget };
    header->setLayout(header_layout);

    restore_order_button->setVisible(m_Project.IsManuallySorted());

    auto* layout{ new QVBoxLayout };
    layout->addWidget(header);
    for (auto* page_widget : page_widgets)
    {
        layout->addWidget(page_widget);
    }
    layout->setSpacing(15);
    layout->setContentsMargins(60, 20, 60, 20);
    auto* pages_widget{ new QWidget };
    pages_widget->setLayout(layout);

    setWidget(pages_widget);

    verticalScrollBar()->setValue(current_scroll);
}

void PrintPreview::RequestRefresh()
{
    m_RefreshTimer.start();
}

void PrintPreview::CardOrderChanged()
{
    if (!m_Project.IsManuallySorted())
    {
        RequestRefresh();
    }
}

void PrintPreview::CardOrderDirectionChanged()
{
    if (!m_Project.IsManuallySorted())
    {
        RequestRefresh();
    }
}

void PrintPreview::wheelEvent(QWheelEvent* event)
{
    // On Windows, don't use wheel events if we are executing a drag-and-drop
    // as these only come in sporadically and just make the experience worse
#ifdef Q_OS_WIN32
    if (!m_Dragging)
#endif
    {
        QScrollArea::wheelEvent(event);
    }
}

bool PrintPreview::eventFilter(QObject* watched, QEvent* event)
{
    if (const auto* card{ dynamic_cast<PrintPreviewCardImage*>(watched) })
    {
        if (event->type() == QEvent::Type::DragMove)
        {
            const auto* drag_move_event{ static_cast<QDragMoveEvent*>(event) };
            const auto pos_card{ drag_move_event->position().toPoint() };
            const auto pos_global{ card->mapToGlobal(pos_card) };
            const auto pos_local{ mapFromGlobal(pos_global) };
            QDragMoveEvent local_event{ pos_local,
                                        drag_move_event->possibleActions(),
                                        drag_move_event->mimeData(),
                                        drag_move_event->buttons(),
                                        drag_move_event->modifiers() };
            dragMoveEvent(&local_event);
        }
    }

    return QObject::eventFilter(watched, event);
}

void PrintPreview::keyPressEvent(QKeyEvent* event)
{
    const auto key{ event->key() };
    if (key >= Qt::Key::Key_0 && key <= Qt::Key::Key_9)
    {
        const auto num{ static_cast<uint32_t>(key - Qt::Key::Key_0) };
        m_TargetPage = m_TargetPage.value_or(0) * 10 + num;
        m_NumberTypeTimer.start();

        GoToPage(m_TargetPage.value());

        event->accept();
    }
    else if (key == Qt::Key::Key_PageUp || key == Qt::Key::Key_PageDown)
    {
        if (const auto* first_page{ GetNthPage(1) })
        {
            auto* scroll_bar{ verticalScrollBar() };
            const auto page_size{ first_page->height() + widget()->layout()->spacing() };
            if (key == Qt::Key::Key_PageUp)
            {
                scroll_bar->setValue(scroll_bar->value() - page_size);
            }
            else
            {
                scroll_bar->setValue(scroll_bar->value() + page_size);
            }

            event->accept();
        }
    }

    if (!event->isAccepted())
    {
        QScrollArea::keyPressEvent(event);
    }
}

void PrintPreview::dragEnterEvent(QDragEnterEvent* event)
{
    QWidget::dragEnterEvent(event);
    event->accept();
}

void PrintPreview::dragMoveEvent(QDragMoveEvent* event)
{
    QWidget::dragMoveEvent(event);

    const auto mouse_y{ event->position().toPoint().y() };
    const auto my_height{ height() };
    m_DragScrollAlpha = static_cast<float>(mouse_y) / my_height;

    // Don't scroll until we move out from a scrolling area after
    // starting to drag
    if (m_DraggingStarted)
    {
        m_DraggingStarted = false;
        if (ComputeDragScrollDiff() != 0)
        {
            m_DraggingStarted = true;
        }
    }
}

void PrintPreview::GoToPage(uint32_t page)
{
    if (const auto* nth_page{ GetNthPage(page) })
    {
        const auto page_pos{ nth_page->pos().y() - widget()->layout()->spacing() };
        const auto widget_height{ widget()->height() };
        const auto maximum{ verticalScrollBar()->maximum() };
        verticalScrollBar()->setValue(maximum * page_pos / (widget_height - height()));
    }
    else if (GetNthPage(1) != nullptr)
    {
        const auto maximum{ verticalScrollBar()->maximum() };
        verticalScrollBar()->setValue(maximum);
    }
}

const PagePreview* PrintPreview::GetNthPage(uint32_t n) const
{
    for (const auto* child : widget()->children())
    {
        if (child->isWidgetType())
        {
            if (const auto* page{ dynamic_cast<const PagePreview*>(child) })
            {
                --n;
                if (n == 0)
                {
                    return page;
                }
            }
        }
    }
    return nullptr;
}

int PrintPreview::ComputeDragScrollDiff() const
{
    if (m_DraggingStarted)
    {
        return 0;
    }

    const auto [beta, sign]{
        [this]()
        {
            static constexpr auto c_Lower{ 0.4f };
            static constexpr auto c_Upper{ 1.0f - c_Lower };
            if (m_DragScrollAlpha < c_Lower)
            {
                return std::pair{ (c_Lower - m_DragScrollAlpha) / c_Lower, 1 };
            }
            else if (m_DragScrollAlpha > c_Upper)
            {
                return std::pair{ (m_DragScrollAlpha - c_Upper) / c_Lower, -1 };
            }
            return std::pair{ 0.0f, 0 };
        }()
    };

    if (sign == 0)
    {
        return 0;
    }

    return static_cast<int>(sign * beta * beta * beta * c_DragScrollSpeed);
}
