#include <ppp/ui/preview/widget_print_preview_page.hpp>

#include <QPainter>
#include <QResizeEvent>
#include <QStyleOption>
#include <QVBoxLayout>

#include <ppp/project/project.hpp>
#include <ppp/render_pdf.hpp>

#include <ppp/ui/preview/widget_print_preview_card.hpp>

#include <ppp/ui/preview/overlays/widget_borders_overlay.hpp>
#include <ppp/ui/preview/overlays/widget_guides_overlay.hpp>
#include <ppp/ui/preview/overlays/widget_margins_overlay.hpp>

#include <ppp/ui/view_models/view_model_card.hpp>
#include <ppp/ui/view_models/view_model_page_preview.hpp>

class PageBackground : public QLabel
{
  public:
    PageBackground(Size page_size)
        : m_PageRatio{ page_size.x / page_size.y }
    {
        QSizePolicy policy{ sizePolicy() };
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }

    void SetPageBackgroud(const QPixmap& background)
    {
        m_Background = &background;
        update();
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return static_cast<int>(static_cast<float>(width) / m_PageRatio);
    }

    virtual void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter{ this };
        painter.fillRect(rect(), Qt::white);

        if (m_Background != nullptr && !m_Background->isNull())
        {
            painter.drawPixmap(rect(), *m_Background);
        }
    }

  private:
    float m_PageRatio;
    const QPixmap* m_Background;
};

class PageImageContainer : public QWidget
{
  public:
    PageImageContainer(const PageImageTransforms& transforms,
                       Size page_size)
        : m_Transforms{ transforms }
        , m_PageSize{ page_size }
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void AddImage(PrintPreviewCardImage* image,
                  QWidget* companion)
    {
        image->setParent(this);
        companion->setParent(this);
        m_Images.push_back(image);
    }

    virtual void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);

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
    }

  private:
    const PageImageTransforms& m_Transforms;
    const Size m_PageSize;

    std::vector<PrintPreviewCardImage*> m_Images;
};

PagePreview::PagePreview(PagePreviewViewModel* view_model,
                         QObject* event_filter,
                         const Page& page,
                         const PageImageTransforms& transforms)
{
    view_model->setParent(this);

    {
        auto* bg_widget{ new PageBackground{ view_model->GetPageSize() } };
        QObject::connect(view_model,
                         &PagePreviewViewModel::PageBackgroundChanged,
                         bg_widget,
                         &PageBackground::SetPageBackgroud);

        auto* bg_layout{ new QVBoxLayout };
        bg_layout->setContentsMargins(0, 0, 0, 0);
        bg_layout->setSpacing(0);
        bg_layout->addWidget(bg_widget);

        setLayout(bg_layout);
    }

    const bool is_backside{ view_model->IsBackside() };

    m_ImageContainer = new PageImageContainer{ transforms, view_model->GetPageSize() };
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
                if (!backside_short_edge || !is_backside)
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

        auto* image_view_model{ view_model->MakeCardViewModel(card_name.value(), rotation) };

        auto* image_widget{
            new PrintPreviewCardImage{
                image_view_model,
                index,
                image_companion,
                widget_clip_rect,
                size,
            },
        };
        image_widget->EnableContextMenu(true,
                                        CardContextMenuFeatures::Default | CardContextMenuFeatures::SkipSlot);
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
                         view_model,
                         &PagePreviewViewModel::ReorderCards);
        QObject::connect(image_view_model,
                         &CardViewModel::SkipThisSlot,
                         view_model,
                         std::bind_front(&PagePreviewViewModel::SkipSlot, view_model, slot));

        m_ImageContainer->AddImage(image_widget, image_companion);
    }

    m_Guides = new GuidesOverlay{ view_model->MakeGuidesOverlayViewModel(), transforms };
    m_Guides->setParent(this);

    m_Borders = new BordersOverlay{ view_model->MakeBordersOverlayViewModel(is_backside), transforms };
    m_Borders->setParent(this);

    m_Margins = new MarginsOverlay{ view_model->MakeMarginsOverlayViewModel(is_backside) };
    m_Margins->setParent(this);
}

void PagePreview::resizeEvent(QResizeEvent* event)
{
    m_ImageContainer->resize(event->size());
    m_Guides->resize(event->size());
    m_Borders->resize(event->size());
    m_Margins->resize(event->size());
}
