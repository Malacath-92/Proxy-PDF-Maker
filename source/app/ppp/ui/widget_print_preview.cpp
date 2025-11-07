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
                          QWidget* companion)
        : CardImage{
            card_name,
            project,
            params,
        }
        , m_Index{ idx }
        , m_Companion{ companion }
    {
        setAcceptDrops(true);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        CardImage::resizeEvent(event);

        m_Companion->resize(event->size() + 2 * QSize{ c_CompanionSizeDelta, c_CompanionSizeDelta });
    }

    virtual void moveEvent(QMoveEvent* event) override
    {
        CardImage::moveEvent(event);

        m_Companion->move(event->pos() - QPoint{ c_CompanionSizeDelta, c_CompanionSizeDelta });
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

  signals:
    void DragStarted();
    void DragFinished();
    void ReorderCards(size_t form, size_t to);

  private:
    void OnEnter()
    {
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
            const auto& [position, size, base_rotation]{
                transforms[i]
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
                        .m_BleedEdge{ project.m_Data.m_BleedEdge },
                    },
                    index,
                    image_companion,
                },
            };
            image_widget->EnableContextMenu(true, project);
            image_widget->setParent(m_ImageContainer);

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

        if (project.m_Data.m_ExportExactGuides && !params.m_IsBackside)
        {
            m_Borders = new BordersOverlay{ project, transforms };
            m_Borders->setParent(this);
        }

        if (project.m_Data.m_MarginsMode != MarginsMode::Auto)
        {
            m_Margins = new MarginsOverlay{ project };
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

class ArrowWidget : public QFrame
{
    Q_OBJECT

  public:
    enum class ArrowDir
    {
        Up = -1,
        Down = +1,
    };

    ArrowWidget(ArrowDir dir)
        : m_Dir{ dir }
    {
        setStyleSheet("QFrame{ background-color: purple; }");

        setFrameStyle(QFrame::Shape::Panel | QFrame::Shadow::Sunken);
        setLineWidth(5);

        setAcceptDrops(true);

        QGraphicsOpacityEffect* effect{ new QGraphicsOpacityEffect{ this } };
        effect->setOpacity(0.75);
        setGraphicsEffect(effect);
    }

    virtual void paintEvent(QPaintEvent* event) override
    {
        QFrame::paintEvent(event);

        const auto height{ geometry().height() };

        const auto arrow_height{ static_cast<int>(height / 2) };
        const auto arrow_side{ static_cast<int>(2 * arrow_height / std::sqrtf(3)) };
        const QPoint arrow_base{
            geometry().center().x(), (height - arrow_height) / 2 + (m_Dir == ArrowDir::Up ? arrow_height : 0)
        };

        const auto dir{ static_cast<int>(m_Dir) };

        const QPoint p1{ arrow_base.x(), arrow_base.y() + dir * arrow_height };
        const QPoint p2{ arrow_base.x() + arrow_side / 2, arrow_base.y() };
        const QPoint p3{ arrow_base.x() - arrow_side / 2, arrow_base.y() };

        const auto arrow_center{ (p1 + p2 + p3) / 3 };
        const auto arrow_outer_side{ static_cast<int>(arrow_side + lineWidth() * 3) };
        const auto p1_outer{ arrow_center + (p1 - arrow_center) * arrow_outer_side / arrow_side };
        const auto p2_outer{ arrow_center + (p2 - arrow_center) * arrow_outer_side / arrow_side };
        const auto p3_outer{ arrow_center + (p3 - arrow_center) * arrow_outer_side / arrow_side };

        QPainterPath arrow_path{};
        arrow_path.addPolygon(QPolygon{ p1, p2, p3, p1 });

        QPainterPath light_path{};
        QPainterPath midlight_path{};
        QPainterPath dark_path{};

        if ((frameShadow() & QFrame::Shadow::Sunken) == QFrame::Shadow::Sunken)
        {
            light_path.addPolygon(QPolygon{ p2_outer, p3_outer, arrow_center, p2_outer });
            midlight_path.addPolygon(QPolygon{ p1_outer, p2_outer, arrow_center, p1_outer });
            dark_path.addPolygon(QPolygon{ p3_outer, p1_outer, arrow_center, p3_outer });
        }
        else
        {
            dark_path.addPolygon(QPolygon{ p2_outer, p3_outer, arrow_center, p2_outer });
            midlight_path.addPolygon(QPolygon{ p1_outer, p2_outer, arrow_center, p1_outer });
            light_path.addPolygon(QPolygon{ p3_outer, p1_outer, arrow_center, p3_outer });
        }

        QColor color_base{ QColor{ Qt::GlobalColor::darkMagenta } };
        QBrush light_brush{ color_base.lighter(130) };
        QBrush midlight_brush{ color_base.lighter(115) };
        QBrush mid_brush{ color_base };
        QBrush dark_brush{ color_base.darker(125) };

        QPainter painter{ this };
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);
        painter.fillPath(light_path, light_brush);
        painter.fillPath(midlight_path, midlight_brush);
        painter.fillPath(dark_path, dark_brush);
        painter.fillPath(arrow_path, mid_brush);
        painter.end();
    }

    virtual void dragEnterEvent(QDragEnterEvent* event) override
    {
        QFrame::dragEnterEvent(event);
        setFrameShadow(QFrame::Shadow::Raised);
        OnEnter();
    }

    virtual void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        QFrame::dragLeaveEvent(event);
        setFrameShadow(QFrame::Shadow::Sunken);
        OnLeave();
    }

  signals:
    void OnEnter();
    void OnLeave();

  private:
    ArrowDir m_Dir;
};

PrintPreview::PrintPreview(Project& project)
    : m_Project{ project }
{
    Refresh();

    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);

    // We only do this to get QDragMoveEvent
    setAcceptDrops(true);

    m_ScrollTimer.setInterval(1);
    m_ScrollTimer.setSingleShot(false);

    m_ScrollUpWidget = new ArrowWidget{ ArrowWidget::ArrowDir::Up };
    m_ScrollUpWidget->setParent(this);
    m_ScrollUpWidget->setVisible(false);

    m_ScrollDownWidget = new ArrowWidget{ ArrowWidget::ArrowDir::Down };
    m_ScrollDownWidget->setParent(this);
    m_ScrollDownWidget->setVisible(false);

    QObject::connect(&m_ScrollTimer,
                     &QTimer::timeout,
                     this,
                     [this]()
                     {
                         if (m_ScrollUpWidget->underMouse())
                         {
                             verticalScrollBar()->setValue(verticalScrollBar()->value() - m_ScrollSpeed);
                         }
                         else
                         {
                             verticalScrollBar()->setValue(verticalScrollBar()->value() + m_ScrollSpeed);
                         }
                     });

    QObject::connect(static_cast<ArrowWidget*>(m_ScrollUpWidget),
                     &ArrowWidget::OnEnter,
                     &m_ScrollTimer,
                     static_cast<void (QTimer::*)()>(&QTimer::start));
    QObject::connect(static_cast<ArrowWidget*>(m_ScrollUpWidget),
                     &ArrowWidget::OnLeave,
                     &m_ScrollTimer,
                     &QTimer::stop);

    QObject::connect(static_cast<ArrowWidget*>(m_ScrollDownWidget),
                     &ArrowWidget::OnEnter,
                     &m_ScrollTimer,
                     static_cast<void (QTimer::*)()>(&QTimer::start));
    QObject::connect(static_cast<ArrowWidget*>(m_ScrollDownWidget),
                     &ArrowWidget::OnLeave,
                     &m_ScrollTimer,
                     &QTimer::stop);
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
                             m_ScrollUpWidget->setVisible(true);
                             m_ScrollDownWidget->setVisible(true);
                         });
        QObject::connect(page,
                         &PagePreview::DragFinished,
                         this,
                         [this]()
                         {
                             m_ScrollUpWidget->setVisible(false);
                             m_ScrollDownWidget->setVisible(false);
                         });
        QObject::connect(page,
                         &PagePreview::ReorderCards,
                         this,
                         &PrintPreview::ReorderCards);
    }

    auto* restore_order_button{ new QPushButton{ "Restore Alphabetical Order" } };
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

    if (g_Cfg.m_ColorCube != "None")
    {
        auto* vibrance_info{ new QLabel{ "Preview does not respect 'Vibrance Bump' setting" } };
        vibrance_info->setStyleSheet("QLabel{ color : red; }");
        header_layout->addWidget(vibrance_info);
    }

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

void PrintPreview::CardOrderChanged()
{
    if (!m_Project.IsManuallySorted())
    {
        Refresh();
    }
}

void PrintPreview::CardOrderDirectionChanged()
{
    if (!m_Project.IsManuallySorted())
    {
        Refresh();
    }
}

void PrintPreview::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);

    const auto scroll_widget_size{ event->size().height() / 8 };

    m_ScrollUpWidget->resize(QSize{ event->size().width(), scroll_widget_size });
    m_ScrollDownWidget->resize(QSize{ event->size().width(), scroll_widget_size });

    m_ScrollUpWidget->move(QPoint{ 0, 0 });
    m_ScrollDownWidget->move(QPoint{ 0, event->size().height() - scroll_widget_size });
}

void PrintPreview::wheelEvent(QWheelEvent* event)
{
    // Don't use wheel events if the scroll widgets are visible
    if (!m_ScrollUpWidget->isVisible())
    {
        QScrollArea::wheelEvent(event);
    }
}

#include <widget_print_preview.moc>
