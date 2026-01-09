#include <ppp/ui/widget_print_preview.hpp>

#include <ranges>

#include <QDrag>
#include <QGraphicsOpacityEffect>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QVBoxLayout>

#include <ppp/constants.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_card.hpp>

#include <ppp/ui/preview_overlays/widget_borders_overlay.hpp>
#include <ppp/ui/preview_overlays/widget_guides_overlay.hpp>
#include <ppp/ui/preview_overlays/widget_margins_overlay.hpp>

class PrintPreviewCardImage : public CardImage
{
    Q_OBJECT

  public:
    PrintPreviewCardImage(const fs::path& card_name,
                          const Project& project,
                          CardImage::Params params,
                          size_t idx,
                          QWidget* companion,
                          std::optional<ClipRect> clip_rect,
                          Size widget_size)
        : CardImage{
            card_name,
            project,
            params,
        }
        , m_Index{ idx }
        , m_Companion{ companion }
        , m_ClipRect{ clip_rect }
        , m_WidgetSize{ widget_size }
    {
        setAcceptDrops(true);
    }

    virtual void mousePressEvent(QMouseEvent* event) override
    {
        CardImage::mousePressEvent(event);

        if (!event->isAccepted() &&
            m_Companion->isVisible() &&
            event->button() == Qt::MouseButton::LeftButton)
        {
            QMimeData* mime_data{ new QMimeData };
            mime_data->setData(
                "data/size_t",
                QByteArray::fromRawData(reinterpret_cast<const char*>(&m_Index),
                                        sizeof(m_Index)));

            QDrag drag{ this };
            drag.setMimeData(mime_data);
            drag.setPixmap(pixmap().scaledToWidth(width() / 2));

            DragStarted();
            Qt::DropAction drop_action{ drag.exec() };
            (void)drop_action;
            DragFinished();

            event->accept();
        }
    }

    virtual void dropEvent(QDropEvent* event) override
    {
        const auto raw_data{ event->mimeData()->data("data/size_t") };
        const auto other_index{ *reinterpret_cast<const size_t*>(raw_data.constData()) };
        if (other_index != m_Index)
        {
            ReorderCards(other_index, m_Index);
            event->acceptProposedAction();
        }
    }

    virtual void enterEvent(QEnterEvent* event) override
    {
        CardImage::enterEvent(event);
        OnEnter();
    }

    virtual void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasFormat("data/size_t"))
        {
            event->acceptProposedAction();
        }

        CardImage::dragEnterEvent(event);
        OnEnter();
    }

    virtual void leaveEvent(QEvent* event) override
    {
        CardImage::leaveEvent(event);
        OnLeave();
    }

    virtual void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        CardImage::dragLeaveEvent(event);
        OnLeave();
    }

    virtual void paintEvent(QPaintEvent* event) override
    {
        if (m_ClipRect.has_value())
        {
            const auto clipped_rect{ event->rect().intersected(GetClippedRect()) };
            const auto scaled_pixmap{
                [this]()
                {
                    const auto unscaled_pixmap{ pixmap() };
                    const auto pdpr{ unscaled_pixmap.devicePixelRatio() };
                    const auto dpr{ devicePixelRatio() };
                    const bool scaled_contents{ hasScaledContents() };
                    if (scaled_contents || dpr != pdpr)
                    {
                        const auto scaled_size{
                            scaled_contents ? (contentsRect().size() * dpr)
                                            : (unscaled_pixmap.size() * (dpr / pdpr))
                        };
                        if (!m_ScaledPixmap.has_value() || m_ScaledPixmap.value().size() != scaled_size)
                        {
                            m_ScaledPixmap = unscaled_pixmap.scaled(scaled_size,
                                                                    Qt::IgnoreAspectRatio,
                                                                    Qt::SmoothTransformation);
                            m_ScaledPixmap.value().setDevicePixelRatio(dpr);
                        }
                        return m_ScaledPixmap.value();
                    }
                    else
                    {
                        return pixmap();
                    }
                }()
            };

            QPainter painter{ this };
            painter.setClipRect(clipped_rect);
            painter.drawPixmap(QPoint{ 0, 0 }, scaled_pixmap);
            painter.end();
        }
        else
        {
            CardImage::paintEvent(event);
        }
    }

  signals:
    void DragStarted();
    void DragFinished();
    void ReorderCards(size_t form, size_t to);

  private:
    QRect GetClippedRect() const
    {
        if (m_ClipRect.has_value())
        {
            const dla::ivec2 pixel_size{
                size().width(),
                size().height(),
            };
            const auto pixel_ratio{ pixel_size / m_WidgetSize };
            const auto pixel_clip_offset{ m_ClipRect.value().m_Position * pixel_ratio };
            const auto pixel_clip_size{ m_ClipRect.value().m_Size * pixel_ratio };
            const QRect clipped_rect{
                QPoint{ (int)pixel_clip_offset.x, (int)pixel_clip_offset.y },
                QSize{ (int)pixel_clip_size.x, (int)pixel_clip_size.y },
            };
            return clipped_rect;
        }
        else
        {
            return rect();
        }
    }

    void OnEnter()
    {
        {
            const auto clipped_rect{ GetClippedRect() };
            m_Companion->move(pos() +
                              clipped_rect.topLeft() -
                              QPoint{ c_CompanionSizeDelta, c_CompanionSizeDelta });
            m_Companion->resize(clipped_rect.size() +
                                2 * QSize{ c_CompanionSizeDelta, c_CompanionSizeDelta });
        }
        m_Companion->setVisible(true);
        m_Companion->raise();
        raise();
    }
    void OnLeave()
    {
        m_Companion->setVisible(false);
    }

    size_t m_Index{ 0 };

    static inline constexpr int c_CompanionSizeDelta{ 4 };
    QWidget* m_Companion{ nullptr };

    std::optional<ClipRect> m_ClipRect{ std::nullopt };
    std::optional<QPixmap> m_ScaledPixmap;

    Size m_WidgetSize;
};

class PrintPreview::PagePreview : public QWidget
{
    Q_OBJECT

  public:
    struct Params
    {
        Size m_PageSize;
        bool m_IsBackside;
    };
    PagePreview(Project& project,
                QObject* event_filter,
                const Page& page,
                const PageImageTransforms& transforms,
                Params params)
        : m_Transforms{ transforms }
    {
        QPalette pal = palette();
        pal.setColor(QPalette::ColorRole::Window, Qt::white);
        setAutoFillBackground(true);
        setPalette(pal);

        const bool rounded_corners{ project.m_Data.m_Corners == CardCorners::Rounded && project.m_Data.m_BleedEdge <= 0_mm };

        m_ImageContainer = new QWidget;
        m_ImageContainer->setParent(this);

        for (size_t i = 0; i < page.m_Images.size(); ++i)
        {
            const auto& [card_name, backside_short_edge, index]{
                page.m_Images[i]
            };
            const auto& [position, size, base_rotation, card, clip_rect]{
                transforms[i]
            };

            const auto widget_clip_rect{
                clip_rect.and_then([position](const auto& clip_rect)
                                   { return std::optional{
                                         ClipRect{
                                             clip_rect.m_Position - position,
                                             clip_rect.m_Size }
                                     }; })
            };

            const auto rotation{
                [=]()
                {
                    if (!backside_short_edge || !params.m_IsBackside)
                    {
                        return base_rotation; // NOLINT
                    }

                    switch (base_rotation) // NOLINT
                    {
                    default:
                    case Image::Rotation::None:
                        return Image::Rotation::Degree180;
                    case Image::Rotation::Degree90:
                        return Image::Rotation::Degree270;
                    case Image::Rotation::Degree180:
                        return Image::Rotation::None;
                    case Image::Rotation::Degree270:
                        return Image::Rotation::Degree90;
                    }
                }()
            };

            auto* image_companion{ new QWidget };
            image_companion->setVisible(false);
            image_companion->setStyleSheet("background-color: purple;");
            image_companion->setParent(m_ImageContainer);

            auto* image_widget{
                new PrintPreviewCardImage{
                    card_name,
                    project,
                    CardImage::Params{
                        .m_RoundedCorners = rounded_corners,
                        .m_Rotation = rotation,
                        .m_BleedEdge{
                            project.m_Data.m_BleedEdge +
                                project.m_Data.m_EnvelopeBleedEdge,
                        },
                    },
                    index,
                    image_companion,
                    widget_clip_rect,
                    size,
                },
            };
            image_widget->EnableContextMenu(true, project);
            image_widget->setParent(m_ImageContainer);
            image_widget->installEventFilter(event_filter);

            QObject::connect(image_widget,
                             &PrintPreviewCardImage::DragStarted,
                             this,
                             &PagePreview::DragStarted);
            QObject::connect(image_widget,
                             &PrintPreviewCardImage::DragFinished,
                             this,
                             &PagePreview::DragFinished);
            QObject::connect(image_widget,
                             &PrintPreviewCardImage::ReorderCards,
                             this,
                             &PagePreview::ReorderCards);

            m_Images.push_back(image_widget);
        }

        if (project.m_Data.m_EnableGuides && (!params.m_IsBackside || project.m_Data.m_BacksideEnableGuides))
        {
            m_Guides = new GuidesOverlay{ project, transforms };
            m_Guides->setParent(this);
        }

        if (project.m_Data.m_ExportExactGuides)
        {
            m_Borders = new BordersOverlay{ project, transforms, params.m_IsBackside };
            m_Borders->setParent(this);
        }

        if (project.m_Data.m_MarginsMode != MarginsMode::Auto)
        {
            m_Margins = new MarginsOverlay{ project, params.m_IsBackside };
            m_Margins->setParent(this);
        }

        const auto& [page_width, page_height]{ params.m_PageSize.pod() };
        m_PageSize = params.m_PageSize;
        m_PageRatio = page_width / page_height;
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return static_cast<int>(static_cast<float>(width) / m_PageRatio);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        m_ImageContainer->resize(event->size());

        const auto width{ event->size().width() };
        const auto height{ event->size().height() };

        const dla::ivec2 size{ width, height };
        const auto pixel_ratio{ size / m_PageSize };

        for (size_t i = 0; i < m_Images.size(); ++i)
        {
            const auto& transform{ m_Transforms[i] };
            const auto card_position{ transform.m_Position * pixel_ratio };
            const auto card_size{ transform.m_Size * pixel_ratio };
            const auto card_far_corner{ card_position + card_size };

            const dla::ivec2 card_position_pixels{
                static_cast<int>(std::floor(card_position.x)),
                static_cast<int>(std::floor(card_position.y)),
            };
            const dla::ivec2 card_far_corner_pixels{
                static_cast<int>(std::ceil(card_far_corner.x)),
                static_cast<int>(std::ceil(card_far_corner.y)),
            };

            auto* card_image{ m_Images[i] };
            card_image->move(card_position_pixels.x, card_position_pixels.y);
            card_image->resize(card_far_corner_pixels.x - card_position_pixels.x,
                               card_far_corner_pixels.y - card_position_pixels.y);
        }

        if (m_Guides != nullptr)
        {
            m_Guides->resize(event->size());
        }

        if (m_Borders != nullptr)
        {
            m_Borders->resize(event->size());
        }

        if (m_Margins != nullptr)
        {
            m_Margins->resize(event->size());
        }
    }

  signals:
    void DragStarted();
    void DragFinished();
    void ReorderCards(size_t form, size_t to);

  private:
    const PageImageTransforms& m_Transforms;

    Size m_PageSize;
    float m_PageRatio;

    QWidget* m_ImageContainer{ nullptr };
    std::vector<PrintPreviewCardImage*> m_Images;

    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };
    MarginsOverlay* m_Margins{ nullptr };
};

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
    // Don't use wheel events if we are executing a drag-and-drop
    if (!m_Dragging)
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

const PrintPreview::PagePreview* PrintPreview::GetNthPage(uint32_t n) const
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
    qDebug() << m_DragScrollAlpha;
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

    qDebug() << static_cast<int>(sign * beta * beta * beta * c_DragScrollSpeed);
    return static_cast<int>(sign * beta * beta * beta * c_DragScrollSpeed);
}

#include <widget_print_preview.moc>
