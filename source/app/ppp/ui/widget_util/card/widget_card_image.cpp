#include <ppp/ui/widget_util/card/widget_card_image.hpp>

#include <QCommonStyle>
#include <QMenu>
#include <QStyle>
#include <QVBoxLayout>

#include <ppp/image.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>
#include <ppp/ui/widget_util/widget_spinner.hpp>

#include <ppp/profile/profile.hpp>

CardImage::CardImage(const fs::path& card_name, const Project& project, CardImageWidgetParams params)
    : WidgetWithCardSize{ GetCardWidgetAspectRatio(project, params.m_Rotation, params.m_BleedEdge) }
    , m_Project{ project }
{
    TRACY_AUTO_SCOPE();

    {
        auto* layout{ new QVBoxLayout };
        layout->addStretch();
        layout->addStretch();
        setLayout(layout);
    }

    setStyleSheet("QLabel{ background-color: transparent; }");
    Refresh(card_name, project, params);
}

void CardImage::Refresh(const fs::path& card_name, const Project& project, CardImageWidgetParams params)
{
    TRACY_AUTO_SCOPE();

    ClearChildren();

    setToolTip(ToQString(card_name));

    m_CardName = card_name;
    m_OriginalParams = params;

    RefreshSize(project);

    m_FullBleed = project.CardFullBleed();
    m_CornerRadius = project.CardCornerRadius();

    m_IsExternalCard = project.IsCardExternal(card_name);
    m_BacksideEnabled = project.m_Data.m_BacksideEnabled;
    m_HasClearBackside = project.HasClearBacksideImage(card_name);
    m_HasNonDefaultBackside = project.HasNonDefaultBacksideImage(card_name);
    m_BadAspectRatio = project.HasBadAspectRatio(card_name);
    m_BleedType = project.GetCardBleedType(card_name);
    m_BadAspectRatioHandling = project.GetCardBadAspectRatioHandling(card_name);

    const bool has_image{ project.HasPreview(card_name) };

    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_NAME(set_pixmap);
        TRACY_SCOPE_INFO_FMT("Card: \"{}\"", has_image ? card_name.string().c_str() : "<none>");

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

        QVBoxLayout* layout{ static_cast<QVBoxLayout*>(this->layout()) };
        layout->insertWidget(1, spinner, 0, Qt::AlignCenter);

        m_Spinner = spinner;
    }

    QObject::connect(&project, &Project::PreviewRemoved, this, &CardImage::PreviewRemoved);
    QObject::connect(&project, &Project::PreviewUpdated, this, &CardImage::PreviewUpdated);
    QObject::connect(&project, &Project::CardBacksideChanged, this, &CardImage::CardBacksideChanged);
    QObject::connect(&project, &Project::BacksideEnabledChanged, this, [this](bool enabled)
                     { m_BacksideEnabled = enabled; });
}

void CardImage::RefreshSize(const Project& project)
{
    WidgetWithCardSize::RefreshSize(
        GetCardWidgetAspectRatio(project,
                                 m_OriginalParams.m_Rotation,
                                 m_OriginalParams.m_BleedEdge));
    m_FullBleed = project.CardFullBleed();
}

void CardImage::EnableContextMenu(bool enable,
                                  Project& project,
                                  CardContextMenuFeatures features)
{
    if (enable && contextMenuPolicy() != Qt::ContextMenuPolicy::CustomContextMenu)
    {
        TRACY_AUTO_SCOPE();

        setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
        QObject::connect(this,
                         &QWidget::customContextMenuRequested,
                         this,
                         &CardImage::ContextMenuRequested);

        static const QIcon s_ClearIcon{ QPixmap{ ":/res/clear.png" } };
        static const QIcon s_BulbIcon{ QPixmap{ ":/res/bulb.png" } };
        static const QIcon s_FullBleedIcon{ QPixmap{ ":/res/full_bleed.png" } };
        static const QIcon s_NoBleedIcon{ QPixmap{ ":/res/no_bleed.png" } };
        static const QIcon s_ResetIcon{ QPixmap{ ":/res/reset.png" } };
        static const QIcon s_ExpandIcon{ QPixmap{ ":/res/expand.png" } };
        static const QIcon s_CropIcon{ QPixmap{ ":/res/crop.png" } };
        static const QIcon s_StretchIcon{ QPixmap{ ":/res/stretch.png" } };
        static const QIcon s_UntapIcon{ QPixmap{ ":/res/untap.png" } };
        static const QIcon s_TapIcon{ QPixmap{ ":/res/tap.png" } };

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_RemoveExternalCardAction = new QAction{ "Remove External Card", this };
            m_RemoveExternalCardAction->setIcon(s_ClearIcon);

            QObject::connect(m_RemoveExternalCardAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::RemoveExternalCard, this, std::ref(project)));
        }

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_ClearBacksideAction = new QAction{ "Clear Backside", this };
            // m_ClearBacksideAction->setIcon(s_ClearIcon);
            m_ResetBacksideAction = new QAction{ "Reset Backside", this };
            m_ResetBacksideAction->setIcon(s_ClearIcon);

            QObject::connect(m_ClearBacksideAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::ClearBackside, this, std::ref(project)));
            QObject::connect(m_ResetBacksideAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::ResetBackside, this, std::ref(project)));
        }

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_InferBleedAction = new QAction{ "Infer Input Bleed", this };
            m_InferBleedAction->setIcon(s_BulbIcon);
            m_ForceFullBleedAction = new QAction{ "Assume Full Bleed", this };
            m_ForceFullBleedAction->setIcon(s_FullBleedIcon);
            m_ForceNoBleedAction = new QAction{ "Assume No Bleed", this };
            m_ForceNoBleedAction->setIcon(s_NoBleedIcon);

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
        }

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_FixRatioIgnoreAction = new QAction{ "Reset Aspect Ratio", this };
            m_FixRatioIgnoreAction->setIcon(s_ResetIcon);
            m_FixRatioExpandAction = new QAction{ "Fix Aspect Ratio: Expand", this };
            m_FixRatioExpandAction->setIcon(s_ExpandIcon);
            m_FixRatioCropAction = new QAction{ "Fix Aspect Ratio: Crop", this };
            m_FixRatioCropAction->setIcon(s_CropIcon);
            m_FixRatioStretchAction = new QAction{ "Fix Aspect Ratio: Stretch", this };
            m_FixRatioStretchAction->setIcon(s_StretchIcon);

            QObject::connect(m_FixRatioIgnoreAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Ignore));
            QObject::connect(m_FixRatioExpandAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Expand));
            QObject::connect(m_FixRatioCropAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Crop));
            QObject::connect(m_FixRatioStretchAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::ChangeBadAspectRatioHandling, this, std::ref(project), BadAspectRatioHandling::Stretch));
        }

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_RotateLeftAction = new QAction{ "Rotate Left", this };
            m_RotateLeftAction->setIcon(s_UntapIcon);
            m_RotateRightAction = new QAction{ "Rotate Right", this };
            m_RotateRightAction->setIcon(s_TapIcon);

            QObject::connect(m_RotateLeftAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::RotateImageLeft, this, std::ref(project)));
            QObject::connect(m_RotateRightAction,
                             &QAction::triggered,
                             this,
                             std::bind_front(&CardImage::RotateImageRight, this, std::ref(project)));
        }

        if (IsSet(features, CardContextMenuFeatures::SkipSlot))
        {
            m_SkipSlotAction = new QAction{ "Skip Slot", this };
            // m_SkipSlotAction->setIcon(s_UntapIcon);

            QObject::connect(m_SkipSlotAction,
                             &QAction::triggered,
                             this,
                             &CardImage::SkipThisSlot);
        }
    }
    else if (!enable)
    {
        LogError("Disabling context menu currently not supported.");
    }
}

void CardImage::PreviewRemoved(const fs::path& card_name)
{
    if (m_CardName == card_name)
    {
        TRACY_AUTO_SCOPE();

        RefreshSize(m_Project);

        setPixmap(FinalizePixmap(GetEmptyImage()));

        m_BadAspectRatio = false;

        ClearChildren();

        auto* spinner{ new SpinnerWidget };

        QVBoxLayout* layout{ static_cast<QVBoxLayout*>(this->layout()) };
        layout->insertWidget(1, spinner, 0, Qt::AlignCenter);

        m_Spinner = spinner;
    }
}

void CardImage::PreviewUpdated(const fs::path& card_name, const ImagePreview& preview)
{
    if (m_CardName == card_name)
    {
        TRACY_AUTO_SCOPE();
        TRACY_SCOPE_INFO_FMT("Card: \"{}\"", card_name.string().c_str());

        RefreshSize(m_Project);

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

void CardImage::CardBacksideChanged(const fs::path& card_name, OptionalImageRef backside)
{
    if (m_CardName == card_name)
    {
        m_HasClearBackside = !backside.has_value();
        m_HasNonDefaultBackside = m_HasClearBackside || !backside.value().get().empty();
    }
}

Image CardImage::GetImage(const ImagePreview& preview) const
{
    TRACY_AUTO_SCOPE();

    if (m_OriginalParams.m_BleedEdge > 0_mm)
    {
        if (m_OriginalParams.m_RoundedCorners)
        {
            const auto finalize_image{
                [&, this](const Image& base_image)
                {
                    return UncropImage(base_image,
                                       m_CardName,
                                       m_Project.CardSize(),
                                       m_OriginalParams.m_BleedEdge,
                                       UncropMode::Transparent)
                        .Rotate(m_OriginalParams.m_Rotation);
                }
            };
            if (m_Project.IsCardRoundedRect())
            {
                return finalize_image(
                    preview.m_CroppedImage
                        .RoundCorners(m_Project.CardSize(), m_CornerRadius));
            }
            else if (m_Project.IsCardSvg())
            {
                return finalize_image(
                    preview.m_CroppedImage
                        .Mirror(false, m_OriginalParams.m_Backside)
                        .ClipSvg(m_Project.CardSvgData())
                        .Mirror(false, m_OriginalParams.m_Backside));
            }
        }
        return CropImage(preview.m_UncroppedImage,
                         m_CardName,
                         m_Project.CardSize(),
                         m_FullBleed,
                         m_OriginalParams.m_BleedEdge,
                         6800_dpi)
            .Rotate(m_OriginalParams.m_Rotation);
    }
    else
    {
        if (m_OriginalParams.m_RoundedCorners)
        {
            if (m_Project.IsCardRoundedRect())
            {
                return preview
                    .m_CroppedImage
                    .RoundCorners(m_Project.CardSize(), m_CornerRadius)
                    .Rotate(m_OriginalParams.m_Rotation);
            }
            else if (m_Project.IsCardSvg())
            {
                return preview
                    .m_CroppedImage
                    .Mirror(false, m_OriginalParams.m_Backside)
                    .ClipSvg(m_Project.CardSvgData())
                    .Mirror(false, m_OriginalParams.m_Backside)
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

    const auto width{ 512_pix };
    const auto height{ heightForWidth(width / 1_pix) * 1_pix };
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

    QVBoxLayout* layout{ static_cast<QVBoxLayout*>(this->layout()) };
    layout->insertWidget(0, format_warning, 0, Qt::AlignLeft);

    m_Warning = format_warning;
}

void CardImage::ContextMenuRequested(QPoint pos)
{
    auto* menu{ new QMenu{ this } };
    auto begin_section{
        [menu, place_separator = false]() mutable
        {
            if (place_separator)
            {
                menu->addSeparator();
            }
            place_separator = true;
        }
    };

    if (m_IsExternalCard && m_RemoveExternalCardAction != nullptr)
    {
        begin_section();
        menu->addAction(m_RemoveExternalCardAction);
    }

    if (m_BacksideEnabled && (!m_HasClearBackside || m_HasNonDefaultBackside) && m_ClearBacksideAction != nullptr)
    {
        begin_section();
        if (!m_HasClearBackside)
        {
            menu->addAction(m_ClearBacksideAction);
        }
        if (m_HasNonDefaultBackside)
        {
            menu->addAction(m_ResetBacksideAction);
        }
    }

    if (m_InferBleedAction != nullptr)
    {
        begin_section();

        menu->addAction(m_InferBleedAction);
        menu->addAction(m_ForceFullBleedAction);
        menu->addAction(m_ForceNoBleedAction);

        m_InferBleedAction->setEnabled(m_BleedType != BleedType::Infer);
        m_ForceFullBleedAction->setEnabled(m_BleedType != BleedType::FullBleed);
        m_ForceNoBleedAction->setEnabled(m_BleedType != BleedType::NoBleed);
    }

    if (m_FixRatioIgnoreAction != nullptr)
    {
        if (m_BadAspectRatio || m_BadAspectRatioHandling != BadAspectRatioHandling::Default)
        {
            begin_section();

            menu->addAction(m_FixRatioIgnoreAction);
            menu->addAction(m_FixRatioExpandAction);
            menu->addAction(m_FixRatioCropAction);
            menu->addAction(m_FixRatioStretchAction);

            m_FixRatioIgnoreAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Ignore);
            m_FixRatioExpandAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Expand);
            m_FixRatioCropAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Crop);
            m_FixRatioStretchAction->setEnabled(m_BadAspectRatioHandling != BadAspectRatioHandling::Stretch);
        }
    }

    if (m_RotateLeftAction != nullptr)
    {
        begin_section();

        menu->addAction(m_RotateLeftAction);
        menu->addAction(m_RotateRightAction);
    }

    if (m_SkipSlotAction != nullptr)
    {
        begin_section();

        menu->addAction(m_SkipSlotAction);
    }

    menu->popup(mapToGlobal(pos));
}

void CardImage::RemoveExternalCard(Project& project)
{
    project.RemoveExternalCard(m_CardName);
}

void CardImage::ClearBackside(Project& project)
{
    project.ClearBacksideImage(m_CardName);
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

    auto* layout{ static_cast<QVBoxLayout*>(this->layout()) };

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
