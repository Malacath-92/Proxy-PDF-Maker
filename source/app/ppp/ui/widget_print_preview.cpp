#include <ppp/ui/widget_print_preview.hpp>

#include <ranges>

#include <QGridLayout>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>

#include <ppp/constants.hpp>
#include <ppp/util.hpp>

#include <ppp/pdf/util.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_card.hpp>

class PageGrid : public QWidget
{
  public:
    struct Params
    {
        bool IsBackside{ false };
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
                    const auto& [image_name, oversized, backside_short_edge]{ card.value() };

                    const auto& image{
                        [&]() -> std::variant<Image, std::reference_wrapper<const Image>>
                        {
                            const bool has_preview{ project.Previews.contains(image_name) };
                            if (!has_preview)
                            {
                                HasMissingPreviews = false;
                            }

                            const bool has_bleed_edge{ project.BleedEdge > 0_mm };
                            if (has_bleed_edge)
                            {
                                const Image& uncropped_image{
                                    has_preview
                                        ? project.Previews.at(image_name).UncroppedImage
                                        : project.FallbackPreview.UncroppedImage
                                };
                                return CropImage(uncropped_image, image_name, project.BleedEdge, 6800_dpi, nullptr);
                            }

                            return std::cref(has_preview
                                                 ? project.Previews.at(image_name).CroppedImage
                                                 : project.FallbackPreview.CroppedImage);
                        }()
                    };

                    bool show{ false };
                    if (show)
                    {
                        std::visit([](const auto& image)
                                   { static_cast<const Image&>(image).DebugDisplay(); },
                                   image);
                    }

                    const Image::Rotation rotation{ GetCardRotation(params.IsBackside, oversized, backside_short_edge) };
                    auto* image_widget{
                        new CardImage{
                            std::visit([](const auto& image) -> const Image&
                                       { return image; },
                                       image),
                            CardImage::Params{
                                .RoundedCorners{ false },
                                .Rotation{ rotation },
                                .BleedEdge{ project.BleedEdge },
                            },
                        },
                    };

                    if (oversized)
                    {
                        grid->addWidget(image_widget, x, y, 1, 2);
                    }
                    else
                    {
                        grid->addWidget(image_widget, x, y);
                    }
                }
            }
        }

        // pad with dummy images
        for (uint32_t i = 0; i < columns; i++)
        {
            const auto [x, y]{ GetGridCords(i, static_cast<uint32_t>(columns), params.IsBackside).pod() };
            if (grid->itemAtPosition(x, y) == nullptr)
            {
                const Image::Rotation rotation{ GetCardRotation(params.IsBackside, false, false) };
                auto* image_widget{
                    new CardImage{
                        project.FallbackPreview.CroppedImage,
                        CardImage::Params{
                            .Rotation{ rotation },
                            .BleedEdge{ project.BleedEdge },
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

        Rows = static_cast<uint32_t>(rows);
        Columns = static_cast<uint32_t>(columns);
        BleedEdge = project.BleedEdge;
    }

    bool DoesHaveMissingPreviews() const
    {
        return HasMissingPreviews;
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const auto width{ event->size().width() };
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);
    }

  private:
    uint32_t Rows;
    uint32_t Columns;
    Length BleedEdge;
    bool HasMissingPreviews{ false };
};

class GuidesOverlay : public QWidget
{
  public:
    struct Params
    {
        PageGrid::Params GridParams{};
        Size PageSize{};
    };

    GuidesOverlay(const Project& project, const Grid& card_grid, Params params)
    {
        const auto rows{ card_grid.size() };
        const auto columns{ card_grid.front().size() };

        for (uint32_t x = 0; x < columns; x++)
        {
            for (uint32_t y = 0; y < rows; y++)
            {
                if (card_grid[y][x].has_value())
                {
                    Cards.push_back({ x, y });
                }
            }
        }
        BleedEdge = project.BleedEdge;

        const auto& [page_width, page_height]{ params.PageSize.pod() };
        PageSize = params.PageSize;

        const Length card_width{ CardSizeWithoutBleed.x + 2 * project.BleedEdge };
        const Length card_height{ CardSizeWithoutBleed.y + 2 * project.BleedEdge };
        CardSizeWithBleedEdge = Size{ card_width, card_height };

        const Length padding_width{ (page_width - columns * card_width) / 2.0f };
        const Length padding_height{ (page_height - rows * card_height) / 2.0f };
        FirstCardCorner = Size{ padding_width, padding_height };

        PenOne.setDashPattern({ 2.0f, 4.0f });
        PenOne.setWidth(1);
        PenOne.setColor(QColor{ project.GuidesColorB.r, project.GuidesColorA.g, project.GuidesColorA.b });

        PenTwo.setDashPattern({ 2.0f, 4.0f });
        PenTwo.setDashOffset(2.0f);
        PenTwo.setWidth(1);
        PenTwo.setColor(QColor{ project.GuidesColorB.r, project.GuidesColorB.g, project.GuidesColorB.b });

        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_PaintOnScreen);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);

        painter.setPen(PenOne);
        painter.drawLines(Lines);

        painter.setPen(PenTwo);
        painter.drawLines(Lines);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const auto width{ event->size().width() };
        const auto height{ event->size().height() };
        const dla::tvec2 size{ width, height };
        const auto pixel_ratio{ size / PageSize };

        const auto line_length{ pixel_ratio * 12.0_pts };

        Lines.clear();
        for (const auto& idx : Cards)
        {
            const auto top_left_pos{ FirstCardCorner + idx * CardSizeWithBleedEdge + BleedEdge };
            const auto top_left_pos_pixels{ pixel_ratio * top_left_pos };
            Lines.push_back(QLineF{ top_left_pos_pixels.x, top_left_pos_pixels.y, top_left_pos_pixels.x + line_length.x, top_left_pos_pixels.y });
            Lines.push_back(QLineF{ top_left_pos_pixels.x, top_left_pos_pixels.y, top_left_pos_pixels.x, top_left_pos_pixels.y + line_length.y });

            const auto top_right_pos{ top_left_pos + dla::vec2(1.0f, 0.0f) * CardSizeWithoutBleed };
            const auto top_right_pos_pixels{ pixel_ratio * top_right_pos };
            Lines.push_back(QLineF{ top_right_pos_pixels.x, top_right_pos_pixels.y, top_right_pos_pixels.x - line_length.x, top_right_pos_pixels.y });
            Lines.push_back(QLineF{ top_right_pos_pixels.x, top_right_pos_pixels.y, top_right_pos_pixels.x, top_right_pos_pixels.y + line_length.y });

            const auto bottom_right_pos{ top_left_pos + dla::vec2(1.0f, 1.0f) * CardSizeWithoutBleed };
            const auto bottom_right_pos_pixels{ pixel_ratio * bottom_right_pos };
            Lines.push_back(QLineF{ bottom_right_pos_pixels.x, bottom_right_pos_pixels.y, bottom_right_pos_pixels.x - line_length.x, bottom_right_pos_pixels.y });
            Lines.push_back(QLineF{ bottom_right_pos_pixels.x, bottom_right_pos_pixels.y, bottom_right_pos_pixels.x, bottom_right_pos_pixels.y - line_length.y });

            const auto bottom_left_pos{ top_left_pos + dla::vec2(0.0f, 1.0f) * CardSizeWithoutBleed };
            const auto bottom_left_pos_pixels{ pixel_ratio * bottom_left_pos };
            Lines.push_back(QLineF{ bottom_left_pos_pixels.x, bottom_left_pos_pixels.y, bottom_left_pos_pixels.x + line_length.x, bottom_left_pos_pixels.y });
            Lines.push_back(QLineF{ bottom_left_pos_pixels.x, bottom_left_pos_pixels.y, bottom_left_pos_pixels.x, bottom_left_pos_pixels.y - line_length.y });
        }
    }

  private:
    std::vector<dla::uvec2> Cards;

    Length BleedEdge;
    Size PageSize;
    Size CardSizeWithBleedEdge;
    Size FirstCardCorner;

    QPen PenOne;
    QPen PenTwo;
    QList<QLineF> Lines;
};

class PrintPreview::PagePreview : public QWidget
{
  public:
    struct Params
    {
        GuidesOverlay::Params GuidesParams{};
        uint32_t Columns{ 3 };
        uint32_t Rows{ 3 };
    };
    PagePreview(const Project& project, const Page& page, Params params)
    {
        const bool left_to_right{ !params.GuidesParams.GridParams.IsBackside };
        const ::Grid card_grid{ DistributeCardsToGrid(page, left_to_right, params.Columns, params.Rows) };

        auto* grid{ new PageGrid{ project, card_grid, params.GuidesParams.GridParams } };

        auto* layout{ new QVBoxLayout };
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(grid);
        layout->setAlignment(grid, Qt::AlignmentFlag::AlignTop);

        setLayout(layout);
        setStyleSheet("background-color: white;");

        if (params.GuidesParams.GridParams.IsBackside)
        {
            Overlay = nullptr;
        }
        else
        {
            Overlay = new GuidesOverlay{ project, card_grid, params.GuidesParams };
            Overlay->setParent(this);
        }

        const auto& [page_width, page_height]{ params.GuidesParams.PageSize.pod() };
        PageRatio = page_width / page_height;
        PageWidth = page_width;
        PageHeight = page_height;

        const Length card_width{ CardSizeWithoutBleed.x + 2 * project.BleedEdge };
        const Length card_height{ CardSizeWithoutBleed.y + 2 * project.BleedEdge };
        CardWidth = card_width;
        CardHeight = card_height;

        PaddingWidth = (page_width - params.Columns * card_width) / 2.0f;
        PaddingHeight = (page_height - params.Rows * card_height) / 2.0f;
        BacksideOffset = params.GuidesParams.GridParams.IsBackside ? project.BacksideOffset : 0_mm;

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

    virtual int heightForWidth(int width) const
    {
        return static_cast<int>(static_cast<float>(width) / PageRatio);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

        const auto width{ event->size().width() };
        const auto height{ heightForWidth(width) };
        setFixedHeight(height);

        if (Overlay != nullptr)
        {
            Overlay->setFixedWidth(width);
            Overlay->setFixedHeight(height);
        }

        const dla::tvec2 size{ width, height };
        const dla::tvec2 page_size{ PageWidth, PageHeight };
        const auto pixel_ratio{ size / page_size };

        const auto padding_width_left{ PaddingWidth + BacksideOffset };
        const auto padding_width_right{ PaddingWidth - BacksideOffset };
        const auto padding_width_left_pixels{ static_cast<int>(padding_width_left * pixel_ratio.x) };
        const auto padding_width_right_pixels{ static_cast<int>(padding_width_right * pixel_ratio.x) };
        const auto padding_height_pixels{ static_cast<int>(PaddingHeight * pixel_ratio.y) };
        setContentsMargins(
            padding_width_left_pixels,
            padding_height_pixels,
            padding_width_right_pixels,
            padding_height_pixels);
    }

  private:
    float PageRatio;
    Length PageWidth;
    Length PageHeight;

    Length CardWidth;
    Length CardHeight;

    Length PaddingWidth;
    Length PaddingHeight;
    Length BacksideOffset;

    PageGrid* Grid;
    GuidesOverlay* Overlay;
};

PrintPreview::PrintPreview(const Project& project)
{
    Refresh(project);
    setWidgetResizable(true);
    setFrameShape(QFrame::Shape::NoFrame);
}

void PrintPreview::Refresh(const Project& project)
{
    const auto current_scroll{ verticalScrollBar()->value() };
    if (auto* current_widget{ widget() })
    {
        delete current_widget;
    }

    auto page_size{ CFG.PageSizes[project.PageSize].Dimensions };
    if (project.Orientation == "Landscape")
    {
        std::swap(page_size.x, page_size.y);
    }
    const auto [page_width, page_height]{ page_size.pod() };
    const Length card_width{ CardSizeWithoutBleed.x + 2 * project.BleedEdge };
    const Length card_height{ CardSizeWithoutBleed.y + 2 * project.BleedEdge };

    const auto columns{ static_cast<uint32_t>(std::floor(page_width / card_width)) };
    const auto rows{ static_cast<uint32_t>(std::floor(page_height / card_height)) };

    struct TempPage
    {
        Page ThePage;
        bool Backside;
    };

    const auto raw_pages{ DistributeCardsToPages(project, columns, rows) };
    auto pages{ raw_pages |
                std::views::transform([](const Page& page)
                                      { return TempPage{ page, false }; }) |
                std::ranges::to<std::vector>() };

    if (project.BacksideEnabled)
    {
        const auto raw_backside_pages{ MakeBacksidePages(project, raw_pages) };
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
                    project,
                    page.ThePage,
                    PagePreview::Params{
                        GuidesOverlay::Params{
                            PageGrid::Params{
                                page.Backside,
                            },
                            page_size,
                        },
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
