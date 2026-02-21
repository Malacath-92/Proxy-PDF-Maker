#include <ppp/ui/widget_card.hpp>

#include <QAction>
#include <QCommonStyle>
#include <QHBoxLayout>
#include <QMenu>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedLayout>
#include <QSvgRenderer>
#include <QSvgWidget>

#include <opencv2/opencv.hpp>

#include <ppp/constants.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/profile/profile.hpp>

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

QPixmap StoreIntoQtPixmap(const Image& img)
{
    TRACY_AUTO_SCOPE();

    const auto& img_impl{ img.GetUnderlying() };
    switch (img_impl.channels())
    {
    case 1:
        return QPixmap::fromImage(QImage(img_impl.ptr(), img_impl.cols, img_impl.rows, img_impl.step, QImage::Format_Grayscale8));
    case 3:
        return QPixmap::fromImage(QImage(img_impl.ptr(), img_impl.cols, img_impl.rows, img_impl.step, QImage::Format_BGR888));
    case 4:
    {
        cv::Mat cvt_img;
        cv::cvtColor(img_impl, cvt_img, cv::COLOR_BGR2RGBA);
        return QPixmap::fromImage(QImage(cvt_img.ptr(), cvt_img.cols, cvt_img.rows, cvt_img.step, QImage::Format_RGBA8888));
    }
    default:
        return QPixmap{ img_impl.cols, img_impl.rows };
    }
}

CardImage::CardImage(const fs::path& card_name, const Project& project, Params params)
    : m_Project{ project }
{
    TRACY_AUTO_SCOPE();

    {
        auto* layout{ new QBoxLayout(QBoxLayout::TopToBottom) };
        layout->addStretch();
        layout->addStretch();
        setLayout(layout);
    }

    setStyleSheet("QLabel{ background-color: transparent; }");
    Refresh(card_name, project, params);
}

void CardImage::Refresh(const fs::path& card_name, const Project& project, Params params)
{
    TRACY_AUTO_SCOPE();

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

    m_IsExternalCard = project.IsCardExternal(card_name);
    m_BacksideEnabled = project.m_Data.m_BacksideEnabled;
    m_HasNonDefaultBackside = project.HasNonDefaultBacksideImage(card_name);
    m_BadAspectRatio = project.HasBadAspectRatio(card_name);
    m_BleedType = project.GetCardBleedType(card_name);
    m_BadAspectRatioHandling = project.GetCardBadAspectRatioHandling(card_name);

    const bool has_image{ project.HasPreview(card_name) };

    {
        TRACY_AUTO_SCOPE();
        TRACY_AUTO_SCOPE_NAME(set_pixmap);
        TRACY_AUTO_SCOPE_INFO("Card: \"%s\"", has_image ? card_name.string().c_str() : "<none>");

        Image image{
            [&]()
            {
                if (has_image)
                {
                    return GetImage(project.GetPreview(card_name));
                }
                else
                {
                    return GetEmptyImage();
                }
            }()
        };
        setPixmap(FinalizePixmap(std::move(image)));
    }

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

    QObject::connect(&project, &Project::PreviewRemoved, this, &CardImage::PreviewRemoved);
    QObject::connect(&project, &Project::PreviewUpdated, this, &CardImage::PreviewUpdated);
    QObject::connect(&project, &Project::CardBacksideChanged, this, &CardImage::CardBacksideChanged);
    QObject::connect(&project, &Project::BacksideEnabledChanged, this, [this](bool enabled)
                     { m_BacksideEnabled = enabled; });
}

void CardImage::EnableContextMenu(bool enable, Project& project)
{
    if (enable && contextMenuPolicy() != Qt::ContextMenuPolicy::CustomContextMenu)
    {
        TRACY_AUTO_SCOPE();

        setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
        QObject::connect(this,
                         &QWidget::customContextMenuRequested,
                         this,
                         &CardImage::ContextMenuRequested);

        static QIcon clear_icon{ QPixmap{ ":/res/clear.png" } };
        static QIcon bulb_icon{ QPixmap{ ":/res/bulb.png" } };
        static QIcon full_bleed_icon{ QPixmap{ ":/res/full_bleed.png" } };
        static QIcon no_bleed_icon{ QPixmap{ ":/res/no_bleed.png" } };
        static QIcon reset_icon{ QPixmap{ ":/res/reset.png" } };
        static QIcon expand_icon{ QPixmap{ ":/res/expand.png" } };
        static QIcon stretch_icon{ QPixmap{ ":/res/stretch.png" } };
        static QIcon untap_icon{ QPixmap{ ":/res/untap.png" } };
        static QIcon tap_icon{ QPixmap{ ":/res/tap.png" } };

        m_RemoveExternalCardAction = new QAction{ "Remove External Card", this };
        m_RemoveExternalCardAction->setIcon(clear_icon);

        m_ResetBacksideAction = new QAction{ "Reset Backside", this };
        m_ResetBacksideAction->setIcon(clear_icon);

        m_InferBleedAction = new QAction{ "Infer Input Bleed", this };
        m_InferBleedAction->setIcon(bulb_icon);
        m_ForceFullBleedAction = new QAction{ "Assume Full Bleed", this };
        m_ForceFullBleedAction->setIcon(full_bleed_icon);
        m_ForceNoBleedAction = new QAction{ "Assume No Bleed", this };
        m_ForceNoBleedAction->setIcon(no_bleed_icon);

        m_FixRatioIgnoreAction = new QAction{ "Reset Aspect Ratio", this };
        m_FixRatioIgnoreAction->setIcon(reset_icon);
        m_FixRatioExpandAction = new QAction{ "Fix Aspect Ratio: Expand", this };
        m_FixRatioExpandAction->setIcon(expand_icon);
        m_FixRatioStretchAction = new QAction{ "Fix Aspect Ratio: Stretch", this };
        m_FixRatioStretchAction->setIcon(stretch_icon);

        m_RotateLeftAction = new QAction{ "Rotate Left", this };
        m_RotateLeftAction->setIcon(untap_icon);
        m_RotateRightAction = new QAction{ "Rotate Right", this };
        m_RotateRightAction->setIcon(tap_icon);

        QObject::connect(m_RemoveExternalCardAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::RemoveExternalCard, this, std::ref(project)));
        QObject::connect(m_ResetBacksideAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ResetBackside, this, std::ref(project)));
        QObject::connect(m_InferBleedAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ChangeBleedType, this, std::ref(project), BleedType::Infer));
        QObject::connect(m_ForceFullBleedAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ChangeBleedType, this, std::ref(project), BleedType::FullBleed));
        QObject::connect(m_ForceNoBleedAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ChangeBleedType, this, std::ref(project), BleedType::NoBleed));

        QObject::connect(m_FixRatioIgnoreAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Ignore));
        QObject::connect(m_FixRatioExpandAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Expand));
        QObject::connect(m_FixRatioStretchAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Stretch));

        QObject::connect(m_RotateLeftAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::RotateImageLeft, this, std::ref(project)));
        QObject::connect(m_RotateRightAction,
                         &QAction::triggered,
                         this,
                         std::bind_front(&CardImage::RotateImageRight, this, std::ref(project)));
    }
    else if (!enable)
    {
        LogError("Disabling context menu currently not supported.");
    }
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

void CardImage::PreviewRemoved(const fs::path& card_name)
{
    if (m_CardName == card_name)
    {
        TRACY_AUTO_SCOPE();

        setPixmap(FinalizePixmap(GetEmptyImage()));

        m_BadAspectRatio = false;

        ClearChildren();

        auto* spinner{ new SpinnerWidget };

        QBoxLayout* layout{ static_cast<QBoxLayout*>(this->layout()) };
        layout->insertWidget(1, spinner, 0, Qt::AlignCenter);

        m_Spinner = spinner;
    }
}

void CardImage::PreviewUpdated(const fs::path& card_name, const ImagePreview& preview)
{
    if (m_CardName == card_name)
    {
        TRACY_AUTO_SCOPE();
        TRACY_AUTO_SCOPE_INFO("Card: \"%s\"", card_name.string().c_str());

        m_BadAspectRatio = preview.m_BadAspectRatio;

        ClearChildren();

        setPixmap(FinalizePixmap(GetImage(preview)));

        const bool bad_aspect_ration{ preview.m_BadAspectRatio };
        const bool bad_rotation{ preview.m_BadRotation };
        const bool bad_format{ bad_aspect_ration || bad_rotation };
        if (bad_format)
        {
            AddBadFormatWarning(preview);
        }
    }
}

void CardImage::CardBacksideChanged(const fs::path& card_name, const fs::path& backside)
{
    if (m_CardName == card_name)
    {
        m_HasNonDefaultBackside = !backside.empty();
    }
}

Image CardImage::GetImage(const ImagePreview& preview) const
{
    TRACY_AUTO_SCOPE();

    if (m_BleedEdge > 0_mm)
    {
        const Image& uncropped_image{ preview.m_UncroppedImage };
        const Image image{ CropImage(uncropped_image, m_CardName, m_CardSize, m_FullBleed, m_BleedEdge, 6800_dpi) };
        return image;
    }
    else
    {
        if (m_OriginalParams.m_RoundedCorners)
        {
            if (m_Project.IsCardRoundedRect())
            {
                return preview
                    .m_CroppedImage
                    .RoundCorners(m_CardSize, m_CornerRadius)
                    .Rotate(m_OriginalParams.m_Rotation);
            }
            else if (m_Project.IsCardSvg())
            {
                return preview
                    .m_CroppedImage
                    .ClipSvg(m_Project.CardSvgData())
                    .Rotate(m_OriginalParams.m_Rotation);
            }
        }

        return preview
            .m_CroppedImage
            .Rotate(m_OriginalParams.m_Rotation);
    }
}

Image CardImage::GetEmptyImage() const
{
    TRACY_AUTO_SCOPE();

    const auto width{ g_Cfg.m_BasePreviewWidth };
    const auto height{ width / m_CardRatio };
    return Image::PlainColor({ width, height }, ColorRGBA8{ 0x80, 0x80, 0x80, 0xff });
}

QPixmap CardImage::FinalizePixmap(Image image) const
{
    TRACY_AUTO_SCOPE();

    return StoreIntoQtPixmap(image);
}

void CardImage::AddBadFormatWarning(const ImagePreview& preview)
{
    TRACY_AUTO_SCOPE();

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

void CardImage::ContextMenuRequested(QPoint pos)
{
    auto* menu{ new QMenu{ this } };

    if (m_IsExternalCard)
    {
        menu->addAction(m_RemoveExternalCardAction);
        menu->addSeparator();
    }

    if (m_BacksideEnabled && m_HasNonDefaultBackside)
    {
        menu->addAction(m_ResetBacksideAction);
        menu->addSeparator();
    }

    menu->addAction(m_InferBleedAction);
    menu->addAction(m_ForceFullBleedAction);
    menu->addAction(m_ForceNoBleedAction);

    m_InferBleedAction->setEnabled(m_BleedType != BleedType::Infer);
    m_ForceFullBleedAction->setEnabled(m_BleedType != BleedType::FullBleed);
    m_ForceNoBleedAction->setEnabled(m_BleedType != BleedType::NoBleed);

    menu->addSeparator();

    if (m_BadAspectRatio || m_BadAspectRatioHandling != BadAspectRatioHandling::Default)
    {
        menu->addAction(m_FixRatioIgnoreAction);
        menu->addAction(m_FixRatioExpandAction);
        menu->addAction(m_FixRatioStretchAction);

        m_FixRatioIgnoreAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Ignore);
        m_FixRatioExpandAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Expand);
        m_FixRatioStretchAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Stretch);

        menu->addSeparator();
    }

    menu->addAction(m_RotateLeftAction);
    menu->addAction(m_RotateRightAction);

    menu->popup(mapToGlobal(pos));
}

void CardImage::RemoveExternalCard(Project& project)
{
    project.RemoveExternalCard(m_CardName);
}

void CardImage::ResetBackside(Project& project)
{
    project.SetBacksideImage(m_CardName, "");
}

void CardImage::ChangeBleedType(Project& project, BleedType bleed_type)
{
    m_BleedType = bleed_type;
    project.SetCardBleedType(m_CardName, bleed_type);
}

void CardImage::ChangeBadAspectRatioHandling(Project& project, BadAspectRatioHandling ratio_handling)
{
    m_BadAspectRatioHandling = ratio_handling;
    project.SetCardBadAspectRatioHandling(m_CardName, ratio_handling);
}

void CardImage::RotateImageLeft(Project& project)
{
    if (project.RotateCardLeft(m_CardName))
    {
        const auto pixmap{ this->pixmap() };
        const auto rotated{
            pixmap
                .transformed(QTransform().rotate(-90))
                .scaled(pixmap.size())
        };
        setPixmap(rotated);
    }
}

void CardImage::RotateImageRight(Project& project)
{
    if (project.RotateCardRight(m_CardName))
    {
        const auto pixmap{ this->pixmap() };
        const auto rotated{
            pixmap
                .transformed(QTransform().rotate(90))
                .scaled(pixmap.size())
        };
        setPixmap(rotated);
    }
}

void CardImage::ClearChildren()
{
    TRACY_AUTO_SCOPE();

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
    TRACY_AUTO_SCOPE();
}
BacksideImage::BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project)
    : CardImage{
        backside_name,
        project,
        CardImage::Params{ .m_MinimumWidth{ minimum_width } }
    }
{
    TRACY_AUTO_SCOPE();
}

void BacksideImage::Refresh(const fs::path& backside_name, const Project& project)
{
    Refresh(backside_name, CardImage::Params{}.m_MinimumWidth, project);
}
void BacksideImage::Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project)
{
    TRACY_AUTO_SCOPE();
    CardImage::Refresh(
        backside_name,
        project,
        CardImage::Params{ .m_MinimumWidth{ minimum_width } });
}

StackedCardBacksideView::StackedCardBacksideView(CardImage* image, QWidget* backside)
{
    TRACY_AUTO_SCOPE();

    backside->setToolTip("Choose individual Backside");

    auto* backside_layout{ new QHBoxLayout };
    backside_layout->addStretch();
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
    TRACY_AUTO_SCOPE();

    new_backside->setMouseTracking(true);

    auto* backside_layout{ static_cast<QHBoxLayout*>(m_BacksideContainer->layout()) };

    m_Backside->setParent(nullptr);
    delete m_Backside;

    backside_layout->addWidget(new_backside);
    backside_layout->addWidget(new_backside, 0, Qt::AlignmentFlag::AlignBottom);
    m_Backside = new_backside;

    RefreshSizes(rect().size());
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

    if (!event->isAccepted() &&
        event->button() == Qt::MouseButton::LeftButton &&
        currentWidget() == m_BacksideContainer)
    {
        BacksideClicked();
        event->accept();
    }
}
