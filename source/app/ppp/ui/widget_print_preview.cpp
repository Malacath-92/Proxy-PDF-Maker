#include <ppp/ui/widget_print_preview.hpp>

#include <ranges>

#include <QDrag>
#include <QGridLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>

#include <ppp/constants.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_card.hpp>

class GuidesOverlay : public QWidget
{
  public:
    GuidesOverlay(const Project& project, const PageImageTransforms& transforms)
        : m_Project{ project }
        , m_Transforms{ transforms }
    {
        m_PenOne.setWidth(1);
        m_PenOne.setColor(QColor{ project.m_Data.m_GuidesColorA.r, project.m_Data.m_GuidesColorA.g, project.m_Data.m_GuidesColorA.b });

        m_PenTwo.setDashPattern({ 2.0f, 4.0f });
        m_PenTwo.setWidth(1);
        m_PenTwo.setColor(QColor{ project.m_Data.m_GuidesColorB.r, project.m_Data.m_GuidesColorB.g, project.m_Data.m_GuidesColorB.b });

        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

        painter.setPen(m_PenOne);
        painter.drawLines(m_Lines);
        painter.setPen(m_PenTwo);
        painter.drawLines(m_Lines);

        painter.end();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const dla::ivec2 size{ event->size().width(), event->size().height() };
        const auto pixel_ratio{ size / m_Project.ComputePageSize() };

        const auto line_length{ m_Project.m_Data.m_GuidesLength * pixel_ratio };
        const auto bleed{ m_Project.m_Data.m_BleedEdge * pixel_ratio };
        const auto offset{ bleed - m_Project.m_Data.m_GuidesOffset * pixel_ratio };

        m_Lines.clear();
        if (m_Transforms.empty())
        {
            return;
        }

        if (m_Project.m_Data.m_CornerGuides)
        {
            for (const auto& transform : m_Transforms)
            {
                const auto top_left_corner{ transform.m_Position * pixel_ratio };
                const auto card_size{ transform.m_Size * pixel_ratio };

                const auto draw_guide{
                    [this](QLineF line, bool cross = false)
                    {
                        if (cross)
                        {
                            const auto from{ line.p1() };
                            const auto delta{ line.p2() - from };
                            line.setP1(from - delta);
                        }
                        m_Lines.push_back(line);
                    }
                };

                const auto top_left_pos{ top_left_corner + offset };
                const auto top_right_pos{ top_left_corner + dla::vec2(1.0f, 0.0f) * card_size + dla::vec2(-1.0f, 1.0f) * offset };
                const auto bottom_right_pos{ top_left_corner + dla::vec2(1.0f, 1.0f) * card_size + dla::vec2(-1.0f, -1.0f) * offset };
                const auto bottom_left_pos{ top_left_corner + dla::vec2(0.0f, 1.0f) * card_size + dla::vec2(1.0f, -1.0f) * offset };

                draw_guide(QLineF{ top_left_pos.x,
                                   top_left_pos.y,
                                   top_left_pos.x + line_length.x,
                                   top_left_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ top_left_pos.x,
                                   top_left_pos.y,
                                   top_left_pos.x,
                                   top_left_pos.y + line_length.y },
                           m_Project.m_Data.m_CrossGuides);

                draw_guide(QLineF{ top_right_pos.x,
                                   top_right_pos.y,
                                   top_right_pos.x - line_length.x,
                                   top_right_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ top_right_pos.x,
                                   top_right_pos.y,
                                   top_right_pos.x,
                                   top_right_pos.y + line_length.y },
                           m_Project.m_Data.m_CrossGuides);

                draw_guide(QLineF{ bottom_right_pos.x,
                                   bottom_right_pos.y,
                                   bottom_right_pos.x - line_length.x,
                                   bottom_right_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ bottom_right_pos.x,
                                   bottom_right_pos.y,
                                   bottom_right_pos.x,
                                   bottom_right_pos.y - line_length.y },
                           m_Project.m_Data.m_CrossGuides);

                draw_guide(QLineF{ bottom_left_pos.x,
                                   bottom_left_pos.y,
                                   bottom_left_pos.x + line_length.x,
                                   bottom_left_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ bottom_left_pos.x,
                                   bottom_left_pos.y,
                                   bottom_left_pos.x,
                                   bottom_left_pos.y - line_length.y },
                           m_Project.m_Data.m_CrossGuides);
            }
        }

        if (m_Project.m_Data.m_ExtendedGuides)
        {
            static constexpr auto g_Precision{ 0.1_pts };

            std::vector<int32_t> unique_x;
            std::vector<int32_t> unique_y;
            for (const auto& transform : m_Transforms)
            {
                const auto top_left_corner{
                    static_cast<dla::ivec2>((transform.m_Position + offset / pixel_ratio) / g_Precision)
                };
                if (!std::ranges::contains(unique_x, top_left_corner.x))
                {
                    unique_x.push_back(top_left_corner.x);
                }
                if (!std::ranges::contains(unique_y, top_left_corner.y))
                {
                    unique_y.push_back(top_left_corner.y);
                }

                const auto card_size{
                    static_cast<dla::ivec2>((transform.m_Size - offset * 2 / pixel_ratio) / g_Precision)
                };
                const auto bottom_right_corner{ top_left_corner + card_size };
                if (!std::ranges::contains(unique_x, bottom_right_corner.x))
                {
                    unique_x.push_back(bottom_right_corner.x);
                }
                if (!std::ranges::contains(unique_y, bottom_right_corner.y))
                {
                    unique_y.push_back(bottom_right_corner.y);
                }
            }

            const auto extended_off{ offset + 1_mm * pixel_ratio };
            const auto x_min{ std::ranges::min(unique_x) * g_Precision * pixel_ratio.x - extended_off.x };
            const auto x_max{ std::ranges::max(unique_x) * g_Precision * pixel_ratio.x + extended_off.y };
            const auto y_min{ std::ranges::min(unique_y) * g_Precision * pixel_ratio.y - extended_off.x };
            const auto y_max{ std::ranges::max(unique_y) * g_Precision * pixel_ratio.y + extended_off.y };

            for (const auto& x : unique_x)
            {
                const auto real_x{ x * g_Precision * pixel_ratio.x };
                m_Lines.push_back(QLineF{ real_x, y_min, real_x, 0.0f });
                m_Lines.push_back(QLineF{ real_x, y_max, real_x, static_cast<float>(size.y) });
            }

            for (const auto& y : unique_y)
            {
                const auto real_y{ y * g_Precision * pixel_ratio.y };
                m_Lines.push_back(QLineF{ x_min, real_y, 0.0f, real_y });
                m_Lines.push_back(QLineF{ x_max, real_y, static_cast<float>(size.x), real_y });
            }
        }
    }

  private:
    const Project& m_Project;
    const PageImageTransforms& m_Transforms;

    QPen m_PenOne;
    QPen m_PenTwo;
    QList<QLineF> m_Lines;
};

class BordersOverlay : public QWidget
{
  public:
    BordersOverlay(const Project& project, const PageImageTransforms& transforms)
        : m_Project{ project }
        , m_Transforms{ transforms }
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        DrawSvg(painter, m_CardBorder);
        painter.end();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const dla::ivec2 size{ event->size().width(), event->size().height() };
        const auto page_size{ m_Project.ComputePageSize() };
        const auto pixel_ratio{ size / page_size };

        const auto bleed_edge{ m_Project.m_Data.m_BleedEdge * pixel_ratio };
        const auto corner_radius{ m_Project.CardCornerRadius() * pixel_ratio };

        m_CardBorder.clear();

        if (m_Project.m_Data.m_BleedEdge > 0_mm)
        {
            const auto margins{ m_Project.ComputeMargins() };
            const auto cards_size{ m_Project.ComputeCardsSize() };
            const Size available_space{
                page_size.x - margins.m_Left - margins.m_Right,
                page_size.y - margins.m_Top - margins.m_Bottom,
            };
            const Position cards_origin{
                margins.m_Left + (available_space.x - cards_size.x) / 2.0f,
                margins.m_Top + (available_space.y - cards_size.y) / 2.0f,
            };

            const QRectF rect{
                cards_origin.x * pixel_ratio.x,
                cards_origin.y * pixel_ratio.y,
                cards_size.x * pixel_ratio.x,
                cards_size.y * pixel_ratio.y,
            };
            m_CardBorder.addRect(rect);
        }

        for (const auto& transform : m_Transforms)
        {
            const auto top_left_corner{ transform.m_Position * pixel_ratio };
            const auto card_size{ transform.m_Size * pixel_ratio };

            const QRectF rect{
                top_left_corner.x + bleed_edge.x,
                top_left_corner.y + bleed_edge.y,
                card_size.x - bleed_edge.x * 2.0f,
                card_size.y - bleed_edge.x * 2.0f,
            };
            m_CardBorder.addRoundedRect(rect, corner_radius.x, corner_radius.y);
        }
    }

  private:
    const Project& m_Project;
    const PageImageTransforms& m_Transforms;

    QPainterPath m_CardBorder;
};

class MarginsOverlay : public QWidget
{
  public:
    MarginsOverlay(const Project& project)
        : m_Project{ project }
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        DrawSvg(painter, m_Margins, QColor{ 0, 0, 255 });
        painter.end();
    }

    void resizeEvent(QResizeEvent* event) override
    {
        const dla::ivec2 size{ event->size().width(), event->size().height() };
        const auto pixel_ratio{ size.x / m_Project.ComputePageSize().x };

        const auto margins{
            m_Project.ComputeMargins() * pixel_ratio
        };
        const auto page_size{
            m_Project.ComputePageSize() * pixel_ratio
        };

        m_Margins.clear();

        m_Margins.moveTo(0, margins.m_Top);
        m_Margins.lineTo(page_size.x, margins.m_Top);

        m_Margins.moveTo(0, page_size.y - margins.m_Bottom);
        m_Margins.lineTo(page_size.x, page_size.y - margins.m_Bottom);

        m_Margins.moveTo(margins.m_Left, 0);
        m_Margins.lineTo(margins.m_Left, page_size.y);

        m_Margins.moveTo(page_size.x - margins.m_Right, 0);
        m_Margins.lineTo(page_size.x - margins.m_Right, page_size.y);
    }

  private:
    const Project& m_Project;

    QPainterPath m_Margins;
};

class PrintPreviewCardImage : public CardImage
{
    Q_OBJECT

  public:
    PrintPreviewCardImage(const fs::path& image_name,
                          const Project& project,
                          CardImage::Params params,
                          size_t idx,
                          QWidget* companion)
        : CardImage{
            image_name,
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

        QMimeData* mime_data{ new QMimeData };
        mime_data->setData(
            "data/size_t",
            QByteArray::fromRawData(reinterpret_cast<const char*>(&m_Index),
                                    sizeof(m_Index)));

        QDrag drag{ this };
        drag.setMimeData(mime_data);
        drag.setPixmap(pixmap().scaledToWidth(width() / 2));

        Qt::DropAction drop_action{ drag.exec() };
        (void)drop_action;
    }

    virtual void dropEvent(QDropEvent* event) override
    {
        const auto raw_data{ event->mimeData()->data("data/size_t") };
        const auto other_index{ *reinterpret_cast<const size_t*>(raw_data.constData()) };
        ReorderCards(other_index, m_Index);
        event->acceptProposedAction();
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
    PagePreview(const Project& project,
                const Page& page,
                const PageImageTransforms& transforms,
                Params params)
        : m_Transforms{ transforms }
    {
        setStyleSheet("background-color: white;");

        const bool rounded_corners{ project.m_Data.m_Corners == CardCorners::Rounded };

        m_ImageContainer = new QWidget;
        m_ImageContainer->setParent(this);

        for (size_t i = 0; i < page.m_Images.size(); ++i)
        {
            const auto& [image_name, backside_short_edge, index]{
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
                        return base_rotation;
                    }

                    switch (base_rotation)
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
                    image_name,
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
            image_widget->setParent(m_ImageContainer);

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
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);

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

PrintPreview::PrintPreview(const Project& project)
    : m_Project{ project }
{
    Refresh();
    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);
}

void PrintPreview::Refresh()
{
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
        empty_label->setStyleSheet("QLabel { color : red; font-size: 14px; }");

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
                         &PagePreview::ReorderCards,
                         this,
                         &PrintPreview::ReorderCards);
    }

    auto* header_layout{ new QHBoxLayout };
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->addWidget(new QLabel{ "Only a preview; Quality is lower than final render" });

    if (g_Cfg.m_ColorCube != "None")
    {
        auto* vibrance_info{ new QLabel{ "Preview does not respect 'Vibrance Bump' setting" } };
        vibrance_info->setStyleSheet("QLabel { color : red; }");
        header_layout->addWidget(vibrance_info);
    }

    auto* header{ new QWidget };
    header->setLayout(header_layout);

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

#include <widget_print_preview.moc>
