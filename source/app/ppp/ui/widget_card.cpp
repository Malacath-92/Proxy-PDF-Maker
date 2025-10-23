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
#include <ppp/qt_util.hpp>

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

CardImage::CardImage(const fs::path& card_name, const Project& project, Params params)
{
    {
        auto* layout{ new QBoxLayout(QBoxLayout::TopToBottom) };
        layout->addStretch();
        layout->addStretch();
        setLayout(layout);
    }

    setStyleSheet("background-color: transparent;");
    Refresh(card_name, project, params);
}

void CardImage::Refresh(const fs::path& card_name, const Project& project, Params params)
{
    ClearChildren();

    setToolTip(ToQString(card_name));

    m_CardName = card_name;
    m_OriginalParams = params;

    m_Rotated = params.m_Rotation == Image::Rotation::Degree90 || params.m_Rotation == Image::Rotation::Degree270;
    m_CardSize = project.CardSize();
    m_FullBleed = project.CardFullBleed();
    m_CardRatio = m_CardSize.x / m_CardSize.y;
    m_BleedEdge = params.m_BleedEdge;
    m_CornerRadius = project.CardCornerRadius();

    const bool has_image{ project.HasPreview(card_name) };
    const bool has_bleed_edge{ params.m_BleedEdge > 0_mm };

    QPixmap pixmap{
        [&]()
        {
            if (has_image)
            {
                if (has_bleed_edge)
                {
                    const Image& uncropped_image{ project.GetUncroppedPreview(card_name) };
                    Image image{ CropImage(uncropped_image, card_name, m_CardSize, m_FullBleed, project.m_Data.m_BleedEdge, 6800_dpi) };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
                else
                {
                    const Image& image{ project.GetCroppedPreview(card_name) };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
            }
            else
            {
                const int width{ static_cast<int>(g_Cfg.m_BasePreviewWidth.value) };
                const int height{ static_cast<int>(width / m_CardRatio) };
                QPixmap raw_pixmap{ width, height };
                raw_pixmap.fill(QColor::fromRgb(0x808080));
                return raw_pixmap;
            }
        }()
    };
    setPixmap(FinalizePixmap(pixmap));

    setSizePolicy(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::MinimumExpanding);
    setScaledContents(true);

    setMinimumWidth(params.m_MinimumWidth.value);

    if (has_image)
    {
        const auto& preview{ project.m_Data.m_Previews.at(card_name) };
        const bool bad_aspect_ration{ preview.m_BadAspectRatio };
        const bool bad_rotation{ preview.m_BadRotation };
        const bool bad_format{ bad_aspect_ration || bad_rotation };
        if (bad_format)
        {
            AddBadFormatWarning(preview);
        }
    }
    else
    {
        auto* spinner{ new SpinnerWidget };

        QBoxLayout* layout{ static_cast<QBoxLayout*>(this->layout()) };
        layout->insertWidget(1, spinner, 0, Qt::AlignCenter);

        m_Spinner = spinner;
    }

    QObject::connect(&project, &Project::PreviewUpdated, this, &CardImage::PreviewUpdated);
}

void CardImage::RotateImage()
{
    const auto pixmap{ this->pixmap() };
    const auto rotated{
        pixmap
            .transformed(QTransform().rotate(90))
            .scaled(pixmap.size())
    };
    setPixmap(rotated);
}

int CardImage::heightForWidth(int width) const
{
    float card_ratio{ m_CardRatio };
    if (m_BleedEdge > 0_mm)
    {
        const auto card_size{ m_CardSize + 2.0f * m_BleedEdge };
        card_ratio = card_size.x / card_size.y;
    }

    if (m_Rotated)
    {
        return int(std::round(width * card_ratio));
    }
    else
    {
        return int(std::round(width / card_ratio));
    }
}

void CardImage::PreviewUpdated(const fs::path& card_name, const ImagePreview& preview)
{
    if (m_CardName == card_name)
    {
        ClearChildren();

        QPixmap pixmap{
            [&, this]()
            {
                if (m_BleedEdge > 0_mm)
                {
                    const Image& uncropped_image{ preview.m_UncroppedImage };
                    Image image{ CropImage(uncropped_image, card_name, m_CardSize, m_FullBleed, m_BleedEdge, 6800_dpi) };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
                else
                {
                    const Image& image{ preview.m_CroppedImage };
                    QPixmap raw_pixmap{ image.StoreIntoQtPixmap() };
                    return raw_pixmap;
                }
            }()
        };
        setPixmap(FinalizePixmap(pixmap));

        const bool bad_aspect_ration{ preview.m_BadAspectRatio };
        const bool bad_rotation{ preview.m_BadRotation };
        const bool bad_format{ bad_aspect_ration || bad_rotation };
        if (bad_format)
        {
            AddBadFormatWarning(preview);
        }
    }
}

QPixmap CardImage::FinalizePixmap(const QPixmap& pixmap)
{
    QPixmap finalized_pixmap{ pixmap };

    if (m_OriginalParams.m_RoundedCorners)
    {
        const Pixel card_corner_radius_pixels{ m_CornerRadius * pixmap.width() / m_CardSize.x };

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

    if (m_OriginalParams.m_Rotation != Image::Rotation::None)
    {
        QTransform transform{};

        switch (m_OriginalParams.m_Rotation)
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

void CardImage::AddBadFormatWarning(const ImagePreview& preview)
{
    static constexpr int c_WarningSize{ 24 };
    const static QPixmap s_WarningPixmap{
        []()
        {
            QCommonStyle style{};
            return style
                .standardIcon(QStyle::StandardPixmap::SP_MessageBoxWarning)
                .pixmap(c_WarningSize);
        }()
    };

    auto* format_warning{ new QLabel };
    format_warning->setPixmap(s_WarningPixmap);
    if (preview.m_BadRotation)
    {
        format_warning->setToolTip("Bad rotation. Use the rotate button to fix this.");
    }
    else
    {
        format_warning->setToolTip("Bad aspect ratio. Check image file or change card size.");
    }
    format_warning->setFixedWidth(c_WarningSize);
    format_warning->setFixedHeight(c_WarningSize);

    QBoxLayout* layout{ static_cast<QBoxLayout*>(this->layout()) };
    layout->insertWidget(0, format_warning, 0, Qt::AlignLeft);

    m_Warning = format_warning;
}

void CardImage::ClearChildren()
{
    auto* layout{ static_cast<QBoxLayout*>(this->layout()) };

    if (m_Warning != nullptr)
    {
        layout->removeWidget(m_Warning);
        delete m_Warning;
        m_Warning = nullptr;
    }

    if (m_Spinner != nullptr)
    {
        layout->removeWidget(m_Spinner);
        delete m_Spinner;
        m_Spinner = nullptr;
    }
}

BacksideImage::BacksideImage(const fs::path& backside_name, const Project& project)
    : BacksideImage{ backside_name, CardImage::Params{}.m_MinimumWidth, project }
{
}
BacksideImage::BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project)
    : CardImage{
        backside_name,
        project,
        CardImage::Params{ .m_MinimumWidth{ minimum_width } }
    }
{
}

void BacksideImage::Refresh(const fs::path& backside_name, const Project& project)
{
    Refresh(backside_name, CardImage::Params{}.m_MinimumWidth, project);
}
void BacksideImage::Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project)
{
    CardImage::Refresh(
        backside_name,
        project,
        CardImage::Params{ .m_MinimumWidth{ minimum_width } });
}

StackedCardBacksideView::StackedCardBacksideView(CardImage* image, QWidget* backside)
{
    const static QIcon s_ClearIcon{
        []()
        {
            QCommonStyle style{};
            return style
                .standardIcon(QStyle::StandardPixmap::SP_DialogResetButton);
        }()
    };

    auto* reset_button{ new QPushButton };
    reset_button->setIcon(s_ClearIcon);
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

    m_Image = image;
    m_Backside = backside;
    m_BacksideContainer = backside_container;
}

void StackedCardBacksideView::RefreshBackside(QWidget* new_backside)
{
    new_backside->setMouseTracking(true);

    auto* backside_layout{ static_cast<QHBoxLayout*>(m_BacksideContainer->layout()) };

    m_Backside->setParent(nullptr);
    delete m_Backside;

    backside_layout->addWidget(new_backside);
    backside_layout->addWidget(new_backside, 0, Qt::AlignmentFlag::AlignBottom);
    m_Backside = new_backside;

    RefreshSizes(rect().size());
}

void StackedCardBacksideView::RotateImage()
{
    m_Image->RotateImage();
}

int StackedCardBacksideView::heightForWidth(int width) const
{
    return m_Image->heightForWidth(width);
}

void StackedCardBacksideView::RefreshSizes(QSize size)
{
    const auto width{ size.width() };
    const auto height{ size.height() };

    const auto img_width{ int(width * 0.9) };
    const auto img_height{ int(height * 0.9) };

    const auto backside_width{ int(width * 0.45) };
    const auto backside_height{ int(height * 0.45) };

    m_Image->setFixedWidth(img_width);
    m_Image->setFixedHeight(img_height);
    m_Backside->setFixedWidth(backside_width);
    m_Backside->setFixedHeight(backside_height);
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

    const auto neg_backside_width{ rect().width() - m_Backside->rect().size().width() };
    const auto neg_backside_height{ rect().height() - m_Backside->rect().size().height() };

    if (x >= neg_backside_width && y >= neg_backside_height)
    {
        setCurrentWidget(m_BacksideContainer);
    }
    else
    {
        setCurrentWidget(m_Image);
    }
}

void StackedCardBacksideView::leaveEvent(QEvent* event)
{
    QStackedWidget::leaveEvent(event);

    setCurrentWidget(m_Image);
}

void StackedCardBacksideView::mouseReleaseEvent(QMouseEvent* event)
{
    QStackedWidget::mouseReleaseEvent(event);

    if (currentWidget() == m_BacksideContainer)
    {
        BacksideClicked();
    }
}
