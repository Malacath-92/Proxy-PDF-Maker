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

class GuidesOverlay;

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
    }

    bool DoesHaveMissingPreviews() const
    {
        return HasMissingPreviews;
    }

    void SetOverlay(GuidesOverlay* overlay)
    {
        Overlay = overlay;
    }

    virtual void resizeEvent(QResizeEvent* event) override;

  private:
    bool HasMissingPreviews{ false };
    GuidesOverlay* Overlay{ nullptr };
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
                    Cards.push_back({
                        { x, y },
                        card.value().Oversized,
                    });
                }
            }
        }
        BleedEdge = project.BleedEdge;
        CornerWeight = project.CornerWeight;

        const Length card_width{ CardSizeWithoutBleed.x + 2 * project.BleedEdge };
        const Length card_height{ CardSizeWithoutBleed.y + 2 * project.BleedEdge };
        CardSizeWithBleedEdge = Size{ card_width, card_height };

        PenOne.setWidth(2);
        PenOne.setColor(QColor{ project.GuidesColorA.r, project.GuidesColorA.g, project.GuidesColorA.b });

        PenTwo.setDashPattern({ 2.0f, 4.0f });
        PenTwo.setWidth(2);
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

    void ResizeOverlay(PageGrid* grid)
    {
        const auto columns{ static_cast<QGridLayout*>(grid->layout())->columnCount() };
        const auto rows{ static_cast<QGridLayout*>(grid->layout())->rowCount() };
        const dla::vec2 first_card_corner{
            static_cast<float>(grid->pos().x()),
            static_cast<float>(grid->pos().y()),
        };
        const dla::vec2 grid_size{
            static_cast<float>(grid->size().width()),
            static_cast<float>(grid->size().height()),
        };
        const dla::vec2 card_size{
            grid_size.x / columns,
            grid_size.y / rows,
        };
        const auto pixel_ratio{ card_size / CardSizeWithBleed };
        const auto line_length{ 3.0_mm * pixel_ratio };
        const auto offset{ CornerWeight * BleedEdge * pixel_ratio };

        Lines.clear();
        for (const auto& [idx, oversized] : Cards)
        {
            const auto top_left_corner{ first_card_corner + idx * card_size };
            const auto oversized_factor{ oversized ? 2.0f : 1.0f };

            const auto top_left_pos{ top_left_corner + oversized_factor * offset };
            Lines.push_back(QLineF{ top_left_pos.x, top_left_pos.y, top_left_pos.x + line_length.x, top_left_pos.y });
            Lines.push_back(QLineF{ top_left_pos.x, top_left_pos.y, top_left_pos.x, top_left_pos.y + line_length.y });

            const auto top_right_pos{ top_left_corner + dla::vec2(oversized_factor, 0.0f) * card_size + dla::vec2(-1.0f, 1.0f) * oversized_factor * offset };
            Lines.push_back(QLineF{ top_right_pos.x, top_right_pos.y, top_right_pos.x - line_length.x, top_right_pos.y });
            Lines.push_back(QLineF{ top_right_pos.x, top_right_pos.y, top_right_pos.x, top_right_pos.y + line_length.y });

            const auto bottom_right_pos{ top_left_corner + dla::vec2(oversized_factor, 1.0f) * card_size + dla::vec2(-1.0f, -1.0f) * oversized_factor * offset };
            Lines.push_back(QLineF{ bottom_right_pos.x, bottom_right_pos.y, bottom_right_pos.x - line_length.x, bottom_right_pos.y });
            Lines.push_back(QLineF{ bottom_right_pos.x, bottom_right_pos.y, bottom_right_pos.x, bottom_right_pos.y - line_length.y });

            const auto bottom_left_pos{ top_left_corner + dla::vec2(0.0f, 1.0f) * card_size + dla::vec2(1.0f, -1.0f) * oversized_factor * offset };
            Lines.push_back(QLineF{ bottom_left_pos.x, bottom_left_pos.y, bottom_left_pos.x + line_length.x, bottom_left_pos.y });
            Lines.push_back(QLineF{ bottom_left_pos.x, bottom_left_pos.y, bottom_left_pos.x, bottom_left_pos.y - line_length.y });
        }
    }

  private:
    struct Card
    {
        dla::uvec2 Index;
        bool Oversized;
    };
    std::vector<Card> Cards;

    Length BleedEdge;
    float CornerWeight;
    Size CardSizeWithBleedEdge;

    QPen PenOne;
    QPen PenTwo;
    QList<QLineF> Lines;
};

void PageGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (Overlay != nullptr)
    {
        Overlay->ResizeOverlay(this);
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

        if (project.EnableGuides && (!params.GridParams.IsBackside || project.BacksideEnableGuides))
        {
            Overlay = new GuidesOverlay{ project, card_grid };
            Overlay->setParent(this);
            grid->SetOverlay(Overlay);
        }

        const auto& [page_width, page_height]{ params.PageSize.pod() };
        PageRatio = page_width / page_height;
        PageWidth = page_width;
        PageHeight = page_height;

        const Length card_width{ CardSizeWithoutBleed.x + 2 * project.BleedEdge };
        const Length card_height{ CardSizeWithoutBleed.y + 2 * project.BleedEdge };
        CardWidth = card_width;
        CardHeight = card_height;

        PaddingWidth = (page_width - params.Columns * card_width) / 2.0f;
        PaddingHeight = (page_height - params.Rows * card_height) / 2.0f;
        BacksideOffset = params.GridParams.IsBackside ? project.BacksideOffset : 0_mm;

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
    GuidesOverlay* Overlay{ nullptr };
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
