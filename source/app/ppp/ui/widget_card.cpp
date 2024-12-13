#include <ppp/ui/widget_card.hpp>

#include <QCommonStyle>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedLayout>

#include <ppp/constants.hpp>

#include <ppp/project/project.hpp>

CardImage::CardImage(const Image& image, Params params)
{
    Refresh(image, params);
}

void CardImage::Refresh(const Image& image, Params params)
{
    const QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
    QPixmap pixmap{ raw_pixmap };

    if (params.RoundedCorners)
    {
        const Length card_corner_radius_inch{ 1_in / 8 };
        const Pixel card_corner_radius_pixels{ card_corner_radius_inch * image.Size().x / CardSizeWithoutBleed.x };

        QPixmap clipped_pixmap{ int(image.Width().value), int(image.Height().value) };
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

        pixmap = clipped_pixmap;
    }

    if (params.Rotation != Image::Rotation::None)
    {
        QTransform transform{};

        switch (params.Rotation)
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
        pixmap = pixmap.transformed(transform);
    }

    setPixmap(pixmap);
    setSizePolicy(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::MinimumExpanding);
    setScaledContents(true);

    static constexpr Pixel card_size_minimum_width_pixels{ 130_pix };
    setMinimumWidth(card_size_minimum_width_pixels.value);

    Rotated = params.Rotation == Image::Rotation::Degree90 or params.Rotation == Image::Rotation::Degree270;
}

int CardImage::heightForWidth(int width) const
{
    if (Rotated)
    {
        return int(width * CardRatio);
    }
    else
    {
        return int(width / CardRatio);
    }
}

BacksideImage::BacksideImage(const fs::path& backside_name, const Project& project)
    : CardImage{
        project.GetPreview(backside_name).CroppedImage,
        CardImage::Params{}
    }
{
}

void BacksideImage::Refresh(const fs::path& backside_name, const Project& project)
{
    CardImage::Refresh(
        project.Previews.contains(backside_name)
            ? project.Previews.at(backside_name).CroppedImage
            : project.Previews.at("fallback.png"_p).CroppedImage,
        CardImage::Params{});
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
