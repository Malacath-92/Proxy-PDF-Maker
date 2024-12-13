#include <ppp/ui/widget_print_preview.hpp>

#include <ranges>

#include <QGridLayout>
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
        uint32_t Columns{ 3 };
        uint32_t Rows{ 3 };
        Length BleedEdge{ 0_mm };
    };

    PageGrid(const Project& project, const Page& cards, Params params)
    {
        auto* grid{ new QGridLayout };
        grid->setSpacing(0);
        grid->setContentsMargins(0, 0, 0, 0);

        const bool left_to_right{ !params.IsBackside };
        Grid card_grid = DistributeCardsToGrid(cards, left_to_right, params.Columns, params.Rows);

        HasMissingPreviews = false;

        for (uint32_t x = 0; x < params.Rows; x++)
        {
            for (uint32_t y = 0; y < params.Columns; y++)
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

                            const bool has_bleed_edge{ params.BleedEdge > 0_mm };
                            if (has_bleed_edge)
                            {
                                const Image& uncropped_image{
                                    has_preview
                                        ? project.Previews.at(image_name).UncroppedImage
                                        : project.FallbackPreview.UncroppedImage
                                };
                                return CropImage(uncropped_image, image_name, params.BleedEdge, 6800_dpi, nullptr);
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
        for (uint32_t i = 0; i < params.Columns * params.Rows; i++)
        {
            const auto [x, y]{ GetGridCords(i, params.Columns, params.IsBackside).pod() };
            if (grid->itemAtPosition(x, y) == nullptr)
            {
                auto* image_widget{ new CardImage(project.FallbackPreview.CroppedImage, CardImage::Params{}) };

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

        Rows = params.Rows;
        Columns = params.Columns;
    }

    bool DoesHaveMissingPreviews() const
    {
        return HasMissingPreviews;
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return static_cast<int>(static_cast<float>(width) / CardRatio * (static_cast<float>(Rows) / Columns));
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
    bool HasMissingPreviews{ false };
};

class PrintPreview::PagePreview : public QWidget
{
  public:
    struct Params
    {
        PageGrid::Params GridParams{};
        Size PageSize{};
    };

    PagePreview(const Project& project, const Page& page, Params params)
    {
        auto* grid{ new PageGrid{ project, page, params.GridParams } };

        auto* layout{ new QVBoxLayout };
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(grid);
        layout->setAlignment(grid, Qt::AlignmentFlag::AlignTop);

        setLayout(layout);
        setStyleSheet("background-color: white;");

        const auto& [page_width, page_height]{ params.PageSize.pod() };
        PageRatio = page_width / page_height;
        PageWidth = page_width;
        PageHeight = page_height;

        const Length card_width{ CardSizeWithoutBleed.x + 2 * params.GridParams.BleedEdge };
        const Length card_height{ CardSizeWithoutBleed.y + 2 * params.GridParams.BleedEdge };
        CardWidth = card_width;
        CardHeight = card_height;

        PaddingWidth = (page_width - params.GridParams.Columns * card_width) / 2.0f;
        PaddingHeight = (page_height - params.GridParams.Rows * card_height) / 2.0f;
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

        const auto padding_width_left{ PaddingWidth + BacksideOffset };
        const auto padding_width_right{ PaddingWidth - BacksideOffset };
        const auto padding_width_left_pixels{ static_cast<int>(padding_width_left * width / PageWidth) };
        const auto padding_width_right_pixels{ static_cast<int>(padding_width_right * width / PageWidth) };
        const auto padding_height_pixels{ static_cast<int>(PaddingHeight * height / PageHeight) };
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

    auto page_size{ PredefinedPageSizes[PageSizes.at(project.PageSize)] };
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
        Page Page;
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
                    page.Page,
                    PagePreview::Params{
                        PageGrid::Params{
                            page.Backside,
                            columns,
                            rows,
                            project.BleedEdge },
                        page_size,
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

    if (CFG.VibranceBump)
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
