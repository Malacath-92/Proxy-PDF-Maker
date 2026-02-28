#include <ppp/ui/widget_print_preview_page.hpp>

#include <QResizeEvent>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_print_preview_card.hpp>

#include <ppp/ui/preview_overlays/widget_borders_overlay.hpp>
#include <ppp/ui/preview_overlays/widget_guides_overlay.hpp>
#include <ppp/ui/preview_overlays/widget_margins_overlay.hpp>

PagePreview::PagePreview(Project& project,
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
        const auto& [card_name, backside_short_edge, index, slot]{
            page.m_Images[i]
        };
        const auto& [position, size, base_rotation, card, clip_rect]{
            transforms[i]
        };

        if (!card_name.has_value())
        {
            continue;
        }

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
                card_name.value(),
                project,
                CardImageWidgetParams{
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
        image_widget->EnableContextMenu(true,
                                        project,
                                        CardContextMenuFeatures::Default | CardContextMenuFeatures::SkipSlot);
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
        QObject::connect(image_widget,
                         &PrintPreviewCardImage::SkipThisSlot,
                         this,
                         [&project, this, slot]()
                         {
                             project.m_Data.m_SkippedLayoutSlots.push_back(slot);
                             RequestRefresh();
                         });

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

bool PagePreview::hasHeightForWidth() const
{
    return true;
}

int PagePreview::heightForWidth(int width) const
{
    return static_cast<int>(static_cast<float>(width) / m_PageRatio);
}

void PagePreview::resizeEvent(QResizeEvent* event)
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
