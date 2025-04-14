#include <ppp/ui/widget_card.hpp>

#include <QCommonStyle>
#include <QHBoxLayout>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedLayout>
#include <QSvgRenderer>
#include <QSvgWidget>

#include <ppp/constants.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

class SpinnerWidget : public QSvgWidget
{
  public:
    SpinnerWidget()
        : QSvgWidget{ ":/res/spinner.svg" }
    {
    }

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }

    virtual int heightForWidth(int width) const override
    {
        return width;
    }
};

CardImage::CardImage(const fs::path& image_name, const Project& project, Params params)
{
    Refresh(image_name, project, params);
}

void CardImage::Refresh(const fs::path& image_name, const Project& project, Params params)
{
    delete layout();

    ImageName = image_name;
    OriginalParams = params;

    const bool has_image{ project.HasPreview(image_name) };
    const bool has_bleed_edge{ params.BleedEdge > 0_mm };

    QPixmap pixmap{
        [&]()
        {
            if (has_image)
            {
                if (has_bleed_edge)
                {
                    const Image& uncropped_image{ project.GetUncroppedPreview(image_name) };
                    Image image{ CropImage(uncropped_image, image_name, project.Data.BleedEdge, 6800_dpi, nullptr) };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
                else
                {
                    const Image& image{ project.GetCroppedPreview(image_name) };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
            }
            else
            {
                const int width{ static_cast<int>(CFG.BasePreviewWidth.value) };
                const int height{ static_cast<int>(width / CFG.CardRatio) };
                QPixmap raw_pixmap{ width, height };
                raw_pixmap.fill(QColor::fromRgb(0x808080));
                return raw_pixmap;
            }
        }()
    };
    setPixmap(FinalizePixmap(pixmap));

    setSizePolicy(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::MinimumExpanding);
    setScaledContents(true);

    setMinimumWidth(params.MinimumWidth.value);

    if (has_image)
    {
        Spinner = nullptr;
    }
    else
    {
        auto* spinner{ new SpinnerWidget };

        QBoxLayout* layout = new QBoxLayout(QBoxLayout::LeftToRight);
        layout->addWidget(spinner, 0, Qt::AlignCenter);
        setLayout(layout);

        Spinner = spinner;
    }

    QObject::connect(&project, &Project::PreviewUpdated, this, &CardImage::PreviewUpdated);

    Rotated = params.Rotation == Image::Rotation::Degree90 or params.Rotation == Image::Rotation::Degree270;
    BleedEdge = params.BleedEdge;
}

int CardImage::heightForWidth(int width) const
{
    float card_ratio{ CFG.CardRatio };
    if (BleedEdge > 0_mm)
    {
        const auto card_size{ CFG.CardSizeWithoutBleed.Dimensions + 2.0f * BleedEdge };
        card_ratio = card_size.x / card_size.y;
    }

    if (Rotated)
    {
        return int(std::round(width * card_ratio));
    }
    else
    {
        return int(std::round(width / card_ratio));
    }
}

void CardImage::PreviewUpdated(const fs::path& image_name, const ImagePreview& preview)
{
    if (ImageName == image_name)
    {
        if (Spinner != nullptr)
        {
            delete Spinner;
            Spinner = nullptr;
        }

        QPixmap pixmap{
            [&, this]()
            {
                if (BleedEdge > 0_mm)
                {
                    const Image& uncropped_image{ preview.UncroppedImage };
                    Image image{ CropImage(uncropped_image, image_name, BleedEdge, 6800_dpi, nullptr) };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
                else
                {
                    const Image& image{ preview.CroppedImage };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
            }()
        };
        setPixmap(FinalizePixmap(pixmap));
    }
}

QPixmap CardImage::FinalizePixmap(const QPixmap& pixmap)
{
    QPixmap finalized_pixmap{ pixmap };

    if (OriginalParams.RoundedCorners)
    {
        const Length card_corner_radius_inch{ CFG.CardCornerRadius.Dimension };
        const Pixel card_corner_radius_pixels{ card_corner_radius_inch * pixmap.width() / CFG.CardSizeWithoutBleed.Dimensions.x };

        QPixmap clipped_pixmap{ pixmap.size() };
        clipped_pixmap.fill(Qt::GlobalColor::transparent);

        QPainterPath path{};
        path.addRoundedRect(
            QRectF(pixmap.rect()),
            card_corner_radius_pixels.value,
            card_corner_radius_pixels.value);

        QPainter painter{ &clipped_pixmap };
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);
        painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform, true);
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, pixmap);

        finalized_pixmap = clipped_pixmap;
    }

    if (OriginalParams.Rotation != Image::Rotation::None)
    {
        QTransform transform{};

        switch (OriginalParams.Rotation)
        {
        case Image::Rotation::Degree90:
            transform.rotate(90);
            break;
        case Image::Rotation::Degree270:
            transform.rotate(-90);
            break;
        case Image::Rotation::Degree180:
            transform.rotate(180);
            break;
        default:
            break;
        }
        finalized_pixmap = pixmap.transformed(transform);
    }

    return finalized_pixmap;
}

BacksideImage::BacksideImage(const fs::path& backside_name, const Project& project)
    : BacksideImage{ backside_name, CardImage::Params{}.MinimumWidth, project }
{
}
BacksideImage::BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project)
    : CardImage{
        backside_name,
        project,
        CardImage::Params{ .MinimumWidth{ minimum_width } }
    }
{
}

void BacksideImage::Refresh(const fs::path& backside_name, const Project& project)
{
    Refresh(backside_name, CardImage::Params{}.MinimumWidth, project);
}
void BacksideImage::Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project)
{
    CardImage::Refresh(
        backside_name,
        project,
        CardImage::Params{ .MinimumWidth{ minimum_width } });
}

StackedCardBacksideView::StackedCardBacksideView(QWidget* image, QWidget* backside)
{
    QCommonStyle style{};

    auto* reset_button{ new QPushButton };
    reset_button->setIcon(
        style.standardIcon(QStyle::StandardPixmap::SP_DialogResetButton));
    reset_button->setToolTip("Reset Backside to Default");
    reset_button->setFixedWidth(20);
    reset_button->setFixedHeight(20);
    QObject::connect(reset_button,
                     &QPushButton::clicked,
                     this,
                     &StackedCardBacksideView::BacksideReset);

    backside->setToolTip("Choose individual Backside");

    auto* backside_layout{ new QHBoxLayout };
    backside_layout->addStretch();
    backside_layout->addWidget(reset_button, 0, Qt::AlignmentFlag::AlignBottom);
    backside_layout->addWidget(backside, 0, Qt::AlignmentFlag::AlignBottom);
    backside_layout->setContentsMargins(0, 0, 0, 0);

    auto* backside_container{ new QWidget{ this } };
    backside_container->setLayout(backside_layout);

    image->setMouseTracking(true);
    backside->setMouseTracking(true);
    backside_container->setMouseTracking(true);
    setMouseTracking(true);

    addWidget(image);
    addWidget(backside_container);

    auto* this_layout{ static_cast<QStackedLayout*>(layout()) };
    this_layout->setStackingMode(QStackedLayout::StackingMode::StackAll);
    this_layout->setAlignment(image, Qt::AlignmentFlag::AlignTop | Qt::AlignmentFlag::AlignLeft);
    this_layout->setAlignment(backside, Qt::AlignmentFlag::AlignBottom | Qt::AlignmentFlag::AlignRight);

    Image = image;
    Backside = backside;
    BacksideContainer = backside_container;
}

void StackedCardBacksideView::RefreshBackside(QWidget* new_backside)
{
    new_backside->setMouseTracking(true);

    auto* backside_layout{ static_cast<QHBoxLayout*>(BacksideContainer->layout()) };
    Backside->setParent(nullptr);
    backside_layout->addWidget(new_backside);
    backside_layout->addWidget(new_backside, 0, Qt::AlignmentFlag::AlignBottom);
    Backside = new_backside;

    RefreshSizes(rect().size());
}

int StackedCardBacksideView::heightForWidth(int width) const
{
    return Image->heightForWidth(width);
}

void StackedCardBacksideView::RefreshSizes(QSize size)
{
    const auto width{ size.width() };
    const auto height{ size.height() };

    const auto img_width{ int(width * 0.9) };
    const auto img_height{ int(height * 0.9) };

    const auto backside_width{ int(width * 0.45) };
    const auto backside_height{ int(height * 0.45) };

    Image->setFixedWidth(img_width);
    Image->setFixedHeight(img_height);
    Backside->setFixedWidth(backside_width);
    Backside->setFixedHeight(backside_height);
}

void StackedCardBacksideView::resizeEvent(QResizeEvent* event)
{
    QStackedWidget::resizeEvent(event);

    RefreshSizes(event->size());
}

void StackedCardBacksideView::mouseMoveEvent(QMouseEvent* event)
{
    QStackedWidget::mouseMoveEvent(event);

    const auto x{ event->pos().x() };
    const auto y{ event->pos().y() };

    const auto neg_backside_width{ rect().width() - Backside->rect().size().width() };
    const auto neg_backside_height{ rect().height() - Backside->rect().size().height() };

    if (x >= neg_backside_width && y >= neg_backside_height)
    {
        setCurrentWidget(BacksideContainer);
    }
    else
    {
        setCurrentWidget(Image);
    }
}

void StackedCardBacksideView::leaveEvent(QEvent* event)
{
    QStackedWidget::leaveEvent(event);

    setCurrentWidget(Image);
}

void StackedCardBacksideView::mouseReleaseEvent(QMouseEvent* event)
{
    QStackedWidget::mouseReleaseEvent(event);

    if (currentWidget() == BacksideContainer)
    {
        BacksideClicked();
    }
}
