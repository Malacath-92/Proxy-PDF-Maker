#include <ppp/ui/widget_print_preview.hpp>

#include <ranges>

#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>

#include <ppp/constants.hpp>
#include <ppp/util.hpp>

#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_card.hpp>

class GuidesOverlay;
class BordersOverlay;

class PageGrid : public QWidget
{
  public:
    struct Params
    {
        bool m_IsBackside{ false };
    };

    PageGrid(const Project& project, const Grid& card_grid, Params params)
    {
        auto* grid{ new QGridLayout };
        grid->setSpacing(0);
        grid->setContentsMargins(0, 0, 0, 0);

        m_HasMissingPreviews = false;

        const auto rows{ card_grid.size() };
        const auto columns{ card_grid.front().size() };

        const bool rounded_corners{
            project.m_Data.m_Corners == CardCorners::Rounded &&
            project.m_Data.m_BleedEdge == 0_mm
        };

        for (uint32_t x = 0; x < columns; x++)
        {
            for (uint32_t y = 0; y < rows; y++)
            {
                if (auto& card{ card_grid[y][x] })
                {
                    const auto& [image_name, backside_short_edge]{ card.value() };

                    const Image::Rotation rotation{ GetCardRotation(params.m_IsBackside, backside_short_edge) };
                    auto* image_widget{
                        new CardImage{
                            image_name,
                            project,
                            CardImage::Params{
                                .m_RoundedCorners = rounded_corners,
                                .m_Rotation = rotation,
                                .m_BleedEdge{ project.m_Data.m_BleedEdge },
                            },
                        },
                    };

                    grid->addWidget(image_widget, y, x);
                }
            }
        }

        auto try_add_dummy{
            [&grid, &params, &project](int x, int y)
            {
                if (grid->itemAtPosition(y, x) != nullptr)
                {
                    return;
                }

                const Image::Rotation rotation{ GetCardRotation(params.m_IsBackside, false) };
                auto* image_widget{
                    new CardImage{
                        g_Cfg.m_FallbackName,
                        project,
                        CardImage::Params{
                            .m_RoundedCorners = false,
                            .m_Rotation = rotation,
                            .m_BleedEdge{ project.m_Data.m_BleedEdge },
                        },
                    },
                };

                auto sp_retain{ image_widget->sizePolicy() };
                sp_retain.setRetainSizeWhenHidden(true);
                image_widget->setSizePolicy(sp_retain);
                image_widget->hide();

                grid->addWidget(image_widget, y, x);
            }
        };

        // pad with dummy images
        for (uint32_t x = 0; x < columns; x++)
        {
            try_add_dummy(x, 0);
        }
        for (uint32_t y = 0; y < rows; y++)
        {
            try_add_dummy(0, y);
        }

        for (int i = 0; i < grid->columnCount(); i++)
        {
            grid->setColumnStretch(i, 1);
        }
        for (int i = 0; i < grid->rowCount(); i++)
        {
            grid->setRowStretch(i, 1);
        }

        setLayout(grid);

        m_CardsWidth = project.ComputeCardsSize().x;
        m_Spacing = project.m_Data.m_Spacing;
    }

    bool DoesHaveMissingPreviews() const
    {
        return m_HasMissingPreviews;
    }

    void SetOverlays(GuidesOverlay* guides, BordersOverlay* borders)
    {
        m_Guides = guides;
        m_Borders = borders;
    }

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    bool m_HasMissingPreviews{ false };
    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };

    Length m_CardsWidth{};
    Size m_Spacing{};
};

class GuidesOverlay : public QWidget
{
  public:
    GuidesOverlay(const Project& project, const Grid& card_grid)
        : m_Project{ project }
    {
        const auto rows{ card_grid.size() };
        const auto columns{ card_grid.front().size() };

        for (uint32_t x = 0; x < columns; x++)
        {
            for (uint32_t y = 0; y < rows; y++)
            {
                if (const auto card{ card_grid[y][x] })
                {
                    m_Cards.push_back({ x, y });
                }
            }
        }

        m_PenOne.setWidth(1);
        m_PenOne.setColor(QColor{ project.m_Data.m_GuidesColorA.r, project.m_Data.m_GuidesColorA.g, project.m_Data.m_GuidesColorA.b });

        m_PenTwo.setDashPattern({ 2.0f, 4.0f });
        m_PenTwo.setWidth(1);
        m_PenTwo.setColor(QColor{ project.m_Data.m_GuidesColorB.r, project.m_Data.m_GuidesColorB.g, project.m_Data.m_GuidesColorB.b });

        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_PaintOnScreen);
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

    void ResizeOverlay(PageGrid* grid)
    {
        const dla::vec2 first_card_corner{
            static_cast<float>(grid->pos().x()),
            static_cast<float>(grid->pos().y()),
        };
        const dla::vec2 grid_size{
            static_cast<float>(grid->size().width()),
            static_cast<float>(grid->size().height()),
        };
        const auto pixel_ratio{ grid_size.x / m_Project.ComputeCardsSize().x };
        const auto card_size{ m_Project.CardSizeWithBleed() * pixel_ratio };
        const auto line_length{ m_Project.m_Data.m_GuidesLength * pixel_ratio };
        const auto bleed{ m_Project.m_Data.m_BleedEdge * pixel_ratio };
        const auto extended_off{ bleed + 1_mm * pixel_ratio };
        const auto offset{ bleed - m_Project.m_Data.m_GuidesOffset * pixel_ratio };
        const auto spacing{ m_Project.m_Data.m_Spacing * pixel_ratio };

        m_Lines.clear();
        for (const auto& idx : m_Cards)
        {
            const auto top_left_corner{ first_card_corner + idx * (card_size + spacing) };

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

            if (m_Project.m_Data.m_CornerGuides)
            {
                draw_guide(QLineF{ top_left_pos.x,
                                   top_left_pos.y,
                                   top_left_pos.x + line_length,
                                   top_left_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ top_left_pos.x,
                                   top_left_pos.y,
                                   top_left_pos.x,
                                   top_left_pos.y + line_length },
                           m_Project.m_Data.m_CrossGuides);

                draw_guide(QLineF{ top_right_pos.x,
                                   top_right_pos.y,
                                   top_right_pos.x - line_length,
                                   top_right_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ top_right_pos.x,
                                   top_right_pos.y,
                                   top_right_pos.x,
                                   top_right_pos.y + line_length },
                           m_Project.m_Data.m_CrossGuides);

                draw_guide(QLineF{ bottom_right_pos.x,
                                   bottom_right_pos.y,
                                   bottom_right_pos.x - line_length,
                                   bottom_right_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ bottom_right_pos.x,
                                   bottom_right_pos.y,
                                   bottom_right_pos.x,
                                   bottom_right_pos.y - line_length },
                           m_Project.m_Data.m_CrossGuides);

                draw_guide(QLineF{ bottom_left_pos.x,
                                   bottom_left_pos.y,
                                   bottom_left_pos.x + line_length,
                                   bottom_left_pos.y },
                           m_Project.m_Data.m_CrossGuides);
                draw_guide(QLineF{ bottom_left_pos.x,
                                   bottom_left_pos.y,
                                   bottom_left_pos.x,
                                   bottom_left_pos.y - line_length },
                           m_Project.m_Data.m_CrossGuides);
            }

            if (m_Project.m_Data.m_ExtendedGuides)
            {
                const uint32_t columns{ static_cast<uint32_t>(static_cast<QGridLayout*>(grid->layout())->columnCount()) };
                const uint32_t rows{ static_cast<uint32_t>(static_cast<QGridLayout*>(grid->layout())->rowCount()) };

                const auto [x, y]{ idx.pod() };
                if (x == 0)
                {
                    draw_guide(QLineF{ top_left_pos.x - extended_off,
                                       top_left_pos.y,
                                       0,
                                       top_left_pos.y });
                    draw_guide(QLineF{ bottom_left_pos.x - extended_off,
                                       bottom_left_pos.y,
                                       0,
                                       bottom_left_pos.y });
                }
                if (x + 1 == columns)
                {
                    const auto page_width{ static_cast<float>(width()) };
                    draw_guide(QLineF{ top_right_pos.x + extended_off,
                                       top_right_pos.y,
                                       page_width,
                                       top_right_pos.y });
                    draw_guide(QLineF{ bottom_right_pos.x + extended_off,
                                       bottom_right_pos.y,
                                       page_width,
                                       bottom_right_pos.y });
                }
                if (y == 0)
                {
                    draw_guide(QLineF{ top_left_pos.x,
                                       top_left_pos.y - extended_off,
                                       top_left_pos.x,
                                       0 });
                    draw_guide(QLineF{ top_right_pos.x,
                                       top_right_pos.y - extended_off,
                                       top_right_pos.x,
                                       0 });
                }
                if (y + 1 == rows)
                {
                    const auto page_height{ static_cast<float>(height()) };
                    draw_guide(QLineF{ bottom_left_pos.x,
                                       bottom_left_pos.y + extended_off,
                                       bottom_left_pos.x,
                                       page_height });
                    draw_guide(QLineF{ bottom_right_pos.x,
                                       bottom_right_pos.y + extended_off,
                                       bottom_right_pos.x,
                                       page_height });
                }
            }
        }
    }

  private:
    const Project& m_Project;

    std::vector<dla::uvec2> m_Cards;

    QPen m_PenOne;
    QPen m_PenTwo;
    QList<QLineF> m_Lines;
};

class BordersOverlay : public QWidget
{
  public:
    BordersOverlay(const Project& project)
        : m_Project{ project }
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_PaintOnScreen);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        DrawSvg(painter, m_CardBorder);
        painter.end();
    }

    void ResizeOverlay(PageGrid* grid)
    {
        const dla::vec2 first_card_corner{
            static_cast<float>(grid->pos().x()),
            static_cast<float>(grid->pos().y()),
        };
        const dla::vec2 grid_size{
            static_cast<float>(grid->size().width()),
            static_cast<float>(grid->size().height()),
        };
        m_CardBorder = GenerateCardsPath(first_card_corner, grid_size, m_Project);
    }

  private:
    const Project& m_Project;

    QPainterPath m_CardBorder;
};

void PageGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const auto pixel_ratio{ static_cast<float>(event->size().width()) / m_CardsWidth };
    auto* grid_layout{ static_cast<QGridLayout*>(layout()) };

    // Safety check: ensure spacing values are reasonable
    const int horizontal_spacing = qMax(0, static_cast<int>(pixel_ratio * m_Spacing.x));
    const int vertical_spacing = qMax(0, static_cast<int>(pixel_ratio * m_Spacing.y));

    grid_layout->setHorizontalSpacing(horizontal_spacing);
    grid_layout->setVerticalSpacing(vertical_spacing);

    if (m_Guides != nullptr)
    {
        m_Guides->ResizeOverlay(this);
    }

    if (m_Borders != nullptr)
    {
        m_Borders->ResizeOverlay(this);
    }
}

class PrintPreview::PagePreview : public QWidget
{
  public:
    struct Params
    {
        PageGrid::Params m_GridParams{};
        Size m_PageSize;
        uint32_t m_Columns{ 3 };
        uint32_t m_Rows{ 3 };
    };
    PagePreview(const Project& project, const Page& page, Params params)
    {
        const auto flip_left_edge{ project.m_Data.m_FlipOn == FlipPageOn::LeftEdge };
        // clang-format off
        const auto orientation{
            !params.m_GridParams.m_IsBackside ? GridOrientation::Default :
                               flip_left_edge ? GridOrientation::FlippedHorizontally
                                              : GridOrientation::FlippedVertically
        };
        // clang-format on
        const ::Grid card_grid{ DistributeCardsToGrid(page, orientation, params.m_Columns, params.m_Rows) };

        auto* grid{ new PageGrid{ project, card_grid, params.m_GridParams } };

        auto* layout{ new QVBoxLayout };
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(grid);
        layout->setAlignment(grid, Qt::AlignmentFlag::AlignTop);

        setLayout(layout);
        setStyleSheet("background-color: white;");

        if (project.m_Data.m_EnableGuides && (!params.m_GridParams.m_IsBackside || project.m_Data.m_BacksideEnableGuides))
        {
            m_Guides = new GuidesOverlay{ project, card_grid };
            m_Guides->setParent(this);
        }

        if (project.m_Data.m_ExportExactGuides && !params.m_GridParams.m_IsBackside)
        {
            m_Borders = new BordersOverlay{ project };
            m_Borders->setParent(this);
        }

        grid->SetOverlays(m_Guides, m_Borders);

        const auto& [page_width, page_height]{ params.m_PageSize.pod() };
        m_PageRatio = page_width / page_height;
        m_PageWidth = page_width;
        m_PageHeight = page_height;

        const auto card_size_with_bleed{ project.CardSizeWithBleed() };
        const auto [card_width, card_height]{ card_size_with_bleed.pod() };
        m_CardWidth = card_width;
        m_CardHeight = card_height;

        const auto cards_size{ project.ComputeCardsSize() };
        m_PaddingWidth = (page_width - cards_size.x) / 2.0f;
        m_PaddingHeight = (page_height - cards_size.y) / 2.0f;

        const auto margins{ project.ComputeMargins() };
        m_LeftMargins = m_PaddingWidth - margins.m_Left;
        m_TopMargins = m_PaddingHeight - margins.m_Top;

        const auto effective_margins_right{ page_width - cards_size.x - margins.m_Left };
        const auto effective_margins_bottom{ page_height - cards_size.y - margins.m_Top };
        m_RightMargins = m_PaddingWidth - effective_margins_right;
        m_BottomMargins = m_PaddingHeight - effective_margins_bottom;

        if (params.m_GridParams.m_IsBackside)
        {
            if (project.m_Data.m_FlipOn == FlipPageOn::LeftEdge)
            {
                std::swap(m_LeftMargins, m_RightMargins);
            }
            else
            {
                std::swap(m_TopMargins, m_BottomMargins);
            }
            m_LeftMargins += project.m_Data.m_BacksideOffset;
            m_RightMargins -= project.m_Data.m_BacksideOffset;
        }

        m_Grid = grid;
    }

    bool DoesHaveMissingPreviews() const
    {
        return m_Grid->DoesHaveMissingPreviews();
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

        const auto width{ event->size().width() };
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);

        if (m_Guides != nullptr)
        {
            m_Guides->setFixedWidth(width);
            m_Guides->setFixedHeight(height);
        }

        if (m_Borders != nullptr)
        {
            m_Borders->setFixedWidth(width);
            m_Borders->setFixedHeight(height);
        }

        const dla::ivec2 size{ width, height };
        const Size page_size{ m_PageWidth, m_PageHeight };
        const auto pixel_ratio{ size / page_size };

        const auto padding_width_left{ m_PaddingWidth - m_LeftMargins };
        const auto padding_width_right{ m_PaddingWidth - m_RightMargins };
        const auto padding_width_left_pixels{ static_cast<int>(padding_width_left * pixel_ratio.x) };
        const auto padding_width_right_pixels{ static_cast<int>(padding_width_right * pixel_ratio.x) };

        const auto padding_height_top{ m_PaddingHeight - m_TopMargins };
        const auto padding_height_bottom{ m_PaddingHeight - m_BottomMargins };
        const auto padding_height_top_pixels{ static_cast<int>(padding_height_top * pixel_ratio.y) };
        const auto padding_height_bottom_pixels{ static_cast<int>(padding_height_bottom * pixel_ratio.y) };

        // Safety check: ensure margins don't cause negative or excessive values
        const int safe_left = qMax(0, qMin(padding_width_left_pixels, width / 2));
        const int safe_right = qMax(0, qMin(padding_width_right_pixels, width / 2));
        const int safe_top = qMax(0, qMin(padding_height_top_pixels, height / 2));
        const int safe_bottom = qMax(0, qMin(padding_height_bottom_pixels, height / 2));

        setContentsMargins(
            safe_left,
            safe_top,
            safe_right,
            safe_bottom);
    }

  private:
    float m_PageRatio;
    Length m_PageWidth;
    Length m_PageHeight;

    Length m_CardWidth;
    Length m_CardHeight;

    Length m_PaddingWidth;
    Length m_PaddingHeight;
    Length m_LeftMargins;
    Length m_TopMargins;
    Length m_RightMargins;
    Length m_BottomMargins;

    PageGrid* m_Grid;
    GuidesOverlay* m_Guides{ nullptr };
    BordersOverlay* m_Borders{ nullptr };
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

    const auto page_size{ m_Project.ComputePageSize() };

    const auto [columns, rows]{ m_Project.m_Data.m_CardLayout.pod() };

    struct TempPage
    {
        Page m_Page;
        bool m_Backside;
    };

    const auto raw_pages{ DistributeCardsToPages(m_Project, columns, rows) };

    // Show empty preview when no cards can fit on the page
    if (raw_pages.empty())
    {
        auto* empty_widget{ new QWidget };
        auto* empty_layout{ new QVBoxLayout };
        auto* empty_label{ new QLabel{ "No cards can fit on the page with current settings.\nPlease adjust page size, margins, or card size." } };
        empty_label->setAlignment(Qt::AlignCenter);
        empty_label->setStyleSheet("QLabel { color : red; font-size: 14px; }");
        empty_layout->addWidget(empty_label);
        empty_widget->setLayout(empty_layout);
        setWidget(empty_widget);
        return;
    }

    auto pages{ raw_pages |
                std::views::transform([](const Page& page)
                                      { return TempPage{ page, false }; }) |
                std::ranges::to<std::vector>() };

    if (m_Project.m_Data.m_BacksideEnabled)
    {
        const auto raw_backside_pages{ MakeBacksidePages(m_Project, raw_pages) };
        const auto backside_pages{ raw_backside_pages |
                                   std::views::transform([](const Page& page)
                                                         { return TempPage{ page, true }; }) |
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
                    PagePreview::Params{
                        PageGrid::Params{
                            page.m_Backside,
                        },
                        page_size,
                        columns,
                        rows, // NOLINT(clang-analyzer-core.NullDereference)
                    }
                };
            }) |
        std::ranges::to<std::vector>()
    };

    auto* header_layout{ new QHBoxLayout };
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->addWidget(new QLabel{ "Only a preview; Quality is lower than final render" });

    const auto has_missing_previews{ std::ranges::any_of(page_widgets, &PagePreview::DoesHaveMissingPreviews) };
    if (has_missing_previews)
    {
        auto* bleed_info{ new QLabel{ "Bleed edge is incorrect; Run cropper for more accurate preview" } };
        bleed_info->setStyleSheet("QLabel { color : red; }");
        header_layout->addWidget(bleed_info);
    }

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
