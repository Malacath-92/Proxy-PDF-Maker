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
        bool IsBackside{ false };
        int Spacing{ 0 };
    };

    PageGrid(const Project& project, const Grid& card_grid, Params params)
    {
        auto* grid{ new QGridLayout };
        grid->setSpacing(0);
        grid->setContentsMargins(0, 0, 0, 0);

        HasMissingPreviews = false;

        const auto rows{ card_grid.front().size() };
        const auto columns{ card_grid.size() };

        for (uint32_t x = 0; x < columns; x++)
        {
            for (uint32_t y = 0; y < rows; y++)
            {
                if (auto& card{ card_grid[x][y] })
                {
                    const auto& [image_name, backside_short_edge]{ card.value() };

                    const Image::Rotation rotation{ GetCardRotation(params.IsBackside, backside_short_edge) };
                    auto* image_widget{
                        new CardImage{
                            image_name,
                            project,
                            CardImage::Params{
                                .RoundedCorners = false,
                                .Rotation = rotation,
                                .BleedEdge{ project.Data.BleedEdge },
                            },
                        },
                    };

                    grid->addWidget(image_widget, x, y);
                }
            }
        }

        // pad with dummy images
        for (uint32_t i = 0; i < rows; i++)
        {
            const auto [x, y]{ GetGridCords(i, static_cast<uint32_t>(rows), params.IsBackside).pod() };
            if (grid->itemAtPosition(x, y) == nullptr)
            {
                const Image::Rotation rotation{ GetCardRotation(params.IsBackside, false) };
                auto* image_widget{
                    new CardImage{
                        CFG.FallbackName,
                        project,
                        CardImage::Params{
                            .Rotation = rotation,
                            .BleedEdge{ project.Data.BleedEdge },
                        },
                    },
                };

                auto sp_retain{ image_widget->sizePolicy() };
                sp_retain.setRetainSizeWhenHidden(true);
                image_widget->setSizePolicy(sp_retain);
                image_widget->hide();

                grid->addWidget(image_widget, x, y);
            }
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

        CardsWidth = project.ComputeCardsSize().x;
        Spacing = project.Data.Spacing;
    }

    bool DoesHaveMissingPreviews() const
    {
        return HasMissingPreviews;
    }

    void SetOverlays(GuidesOverlay* guides, BordersOverlay* borders)
    {
        Guides = guides;
        Borders = borders;
    }

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    bool HasMissingPreviews{ false };
    GuidesOverlay* Guides{ nullptr };
    BordersOverlay* Borders{ nullptr };

    Length CardsWidth{};
    Length Spacing{};
};

class GuidesOverlay : public QWidget
{
  public:
    GuidesOverlay(const Project& project, const Grid& card_grid)
    {
        const auto rows{ card_grid.size() };
        const auto columns{ card_grid.front().size() };

        for (uint32_t x = 0; x < columns; x++)
        {
            for (uint32_t y = 0; y < rows; y++)
            {
                if (const auto card{ card_grid[y][x] })
                {
                    Cards.push_back({ x, y });
                }
            }
        }
        BleedEdge = project.Data.BleedEdge;
        Spacing = project.Data.Spacing;
        GuidesOffset = project.Data.GuidesOffset;

        CardSizeWithBleedEdge = project.CardSizeWithBleed();
        CornerRadius = project.CardCornerRadius();

        CardsSize = project.ComputeCardsSize();

        PenOne.setWidth(1);
        PenOne.setColor(QColor{ project.Data.GuidesColorA.r, project.Data.GuidesColorA.g, project.Data.GuidesColorA.b });

        PenTwo.setDashPattern({ 2.0f, 4.0f });
        PenTwo.setWidth(1);
        PenTwo.setColor(QColor{ project.Data.GuidesColorB.r, project.Data.GuidesColorB.g, project.Data.GuidesColorB.b });

        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_PaintOnScreen);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

        painter.setPen(PenOne);
        painter.drawLines(Lines);
        painter.setPen(PenTwo);
        painter.drawLines(Lines);

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
        const auto pixel_ratio{ grid_size / CardsSize };
        const auto card_size{ CardSizeWithBleedEdge * pixel_ratio };
        const auto line_length{ CornerRadius * pixel_ratio * 0.5f };
        const auto offset{ (BleedEdge - GuidesOffset) * pixel_ratio };
        const auto spacing{ Spacing * pixel_ratio };

        Lines.clear();
        for (const auto& idx : Cards)
        {
            const auto top_left_corner{ first_card_corner + idx * (card_size + spacing) };

            const auto top_left_pos{ top_left_corner + offset };
            Lines.push_back(QLineF{ top_left_pos.x, top_left_pos.y, top_left_pos.x + line_length.x, top_left_pos.y });
            Lines.push_back(QLineF{ top_left_pos.x, top_left_pos.y, top_left_pos.x, top_left_pos.y + line_length.y });

            const auto top_right_pos{ top_left_corner + dla::vec2(1.0f, 0.0f) * card_size + dla::vec2(-1.0f, 1.0f) * offset };
            Lines.push_back(QLineF{ top_right_pos.x, top_right_pos.y, top_right_pos.x - line_length.x, top_right_pos.y });
            Lines.push_back(QLineF{ top_right_pos.x, top_right_pos.y, top_right_pos.x, top_right_pos.y + line_length.y });

            const auto bottom_right_pos{ top_left_corner + dla::vec2(1.0f, 1.0f) * card_size + dla::vec2(-1.0f, -1.0f) * offset };
            Lines.push_back(QLineF{ bottom_right_pos.x, bottom_right_pos.y, bottom_right_pos.x - line_length.x, bottom_right_pos.y });
            Lines.push_back(QLineF{ bottom_right_pos.x, bottom_right_pos.y, bottom_right_pos.x, bottom_right_pos.y - line_length.y });

            const auto bottom_left_pos{ top_left_corner + dla::vec2(0.0f, 1.0f) * card_size + dla::vec2(1.0f, -1.0f) * offset };
            Lines.push_back(QLineF{ bottom_left_pos.x, bottom_left_pos.y, bottom_left_pos.x + line_length.x, bottom_left_pos.y });
            Lines.push_back(QLineF{ bottom_left_pos.x, bottom_left_pos.y, bottom_left_pos.x, bottom_left_pos.y - line_length.y });
        }
    }

  private:
    std::vector<dla::uvec2> Cards;

    Length BleedEdge;
    Length Spacing;
    Length GuidesOffset;
    Size CardSizeWithBleedEdge;
    Length CornerRadius;

    Size CardsSize;

    QPen PenOne;
    QPen PenTwo;
    QList<QLineF> Lines;
};

class BordersOverlay : public QWidget
{
  public:
    BordersOverlay(const Project& project)
    {
        BleedEdge = project.Data.BleedEdge;
        Spacing = project.Data.Spacing;

        CardSize = project.CardSize();
        CornerRadius = project.CardCornerRadius();

        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_PaintOnScreen);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        DrawSvg(painter, CardBorder);
        painter.end();
    }

    void ResizeOverlay(PageGrid* grid)
    {
        const uint32_t columns{ static_cast<uint32_t>(static_cast<QGridLayout*>(grid->layout())->columnCount()) };
        const uint32_t rows{ static_cast<uint32_t>(static_cast<QGridLayout*>(grid->layout())->rowCount()) };

        const dla::vec2 first_card_corner{
            static_cast<float>(grid->pos().x()),
            static_cast<float>(grid->pos().y()),
        };
        const dla::vec2 grid_size{
            static_cast<float>(grid->size().width()),
            static_cast<float>(grid->size().height()),
        };
        CardBorder = GenerateCardsPath(first_card_corner, grid_size, { columns, rows }, CardSize, BleedEdge, Spacing, CornerRadius);
    }

  private:
    Length BleedEdge;
    Length Spacing;
    Size CardSize;
    Length CornerRadius;

    QPainterPath CardBorder;
};

void PageGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const auto pixel_ratio{ static_cast<float>(event->size().width()) / CardsWidth };
    layout()->setSpacing(static_cast<int>(pixel_ratio * Spacing));

    if (Guides != nullptr)
    {
        Guides->ResizeOverlay(this);
    }

    if (Borders != nullptr)
    {
        Borders->ResizeOverlay(this);
    }
}

class PrintPreview::PagePreview : public QWidget
{
  public:
    struct Params
    {
        PageGrid::Params GridParams{};
        Size PageSize;
        uint32_t Columns{ 3 };
        uint32_t Rows{ 3 };
    };
    PagePreview(const Project& project, const Page& page, Params params)
    {
        const bool left_to_right{ !params.GridParams.IsBackside };
        const ::Grid card_grid{ DistributeCardsToGrid(page, left_to_right, params.Columns, params.Rows) };

        auto* grid{ new PageGrid{ project, card_grid, params.GridParams } };

        auto* layout{ new QVBoxLayout };
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(grid);
        layout->setAlignment(grid, Qt::AlignmentFlag::AlignTop);

        setLayout(layout);
        setStyleSheet("background-color: white;");

        if (project.Data.EnableGuides && (!params.GridParams.IsBackside || project.Data.BacksideEnableGuides))
        {
            Guides = new GuidesOverlay{ project, card_grid };
            Guides->setParent(this);
        }

        if (project.Data.ExportExactGuides && !params.GridParams.IsBackside)
        {
            Borders = new BordersOverlay{ project };
            Borders->setParent(this);
        }

        grid->SetOverlays(Guides, Borders);

        const auto& [page_width, page_height]{ params.PageSize.pod() };
        PageRatio = page_width / page_height;
        PageWidth = page_width;
        PageHeight = page_height;

        const auto card_size_with_bleed{ project.CardSizeWithBleed() };
        const auto [card_width, card_height]{ card_size_with_bleed.pod() };
        CardWidth = card_width;
        CardHeight = card_height;

        const auto cards_size{ project.ComputeCardsSize() };
        PaddingWidth = (page_width - cards_size.x) / 2.0f;
        PaddingHeight = (page_height - cards_size.y) / 2.0f;

        const auto margins{ project.ComputeMargins() };
        LeftMargins = PaddingWidth - margins.x;
        TopMargins = PaddingHeight - margins.y;

        LeftMargins += params.GridParams.IsBackside ? project.Data.BacksideOffset : 0_mm;

        Grid = grid;
    }

    bool DoesHaveMissingPreviews() const
    {
        return Grid->DoesHaveMissingPreviews();
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return static_cast<int>(static_cast<float>(width) / PageRatio);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const auto width{ event->size().width() };
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);

        if (Guides != nullptr)
        {
            Guides->setFixedWidth(width);
            Guides->setFixedHeight(height);
        }

        if (Borders != nullptr)
        {
            Borders->setFixedWidth(width);
            Borders->setFixedHeight(height);
        }

        const dla::ivec2 size{ width, height };
        const Size page_size{ PageWidth, PageHeight };
        const auto pixel_ratio{ size / page_size };

        const auto padding_width_left{ PaddingWidth - LeftMargins };
        const auto padding_width_right{ PaddingWidth + LeftMargins };
        const auto padding_width_left_pixels{ static_cast<int>(padding_width_left * pixel_ratio.x) };
        const auto padding_width_right_pixels{ static_cast<int>(padding_width_right * pixel_ratio.x) };

        const auto padding_height_top{ PaddingHeight - TopMargins };
        const auto padding_height_bottom{ PaddingHeight + TopMargins };
        const auto padding_height_top_pixels{ static_cast<int>(padding_height_top * pixel_ratio.y) };
        const auto padding_height_bottom_pixels{ static_cast<int>(padding_height_bottom * pixel_ratio.y) };

        setContentsMargins(
            padding_width_left_pixels,
            padding_height_top_pixels,
            padding_width_right_pixels,
            padding_height_bottom_pixels);
    }

  private:
    float PageRatio;
    Length PageWidth;
    Length PageHeight;

    Length CardWidth;
    Length CardHeight;

    Length PaddingWidth;
    Length PaddingHeight;
    Length LeftMargins;
    Length TopMargins;

    PageGrid* Grid;
    GuidesOverlay* Guides{ nullptr };
    BordersOverlay* Borders{ nullptr };
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

    const auto [columns, rows]{ m_Project.Data.CardLayout.pod() };

    struct TempPage
    {
        Page ThePage;
        bool Backside;
    };

    const auto raw_pages{ DistributeCardsToPages(m_Project, columns, rows) };
    auto pages{ raw_pages |
                std::views::transform([](const Page& page)
                                      { return TempPage{ page, false }; }) |
                std::ranges::to<std::vector>() };

    if (m_Project.Data.BacksideEnabled)
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
                    page.ThePage,
                    PagePreview::Params{
                        PageGrid::Params{
                            page.Backside,
                        },
                        page_size,
                        columns,
                        rows,
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

    if (CFG.ColorCube != "None")
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
