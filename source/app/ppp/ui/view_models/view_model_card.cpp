#include <ppp/ui/view_models/view_model_card.hpp>

#include <QPixmap>

#include <ppp/config.hpp>
#include <ppp/image.hpp>

#include <ppp/util/log.hpp>

#include <ppp/project/image_ops.hpp>
#include <ppp/project/project.hpp>

#include <ppp/ui/widget_util/card/card_widget_util.hpp>

#include <ppp/profile/profile.hpp>

CardViewModel::CardViewModel(fs::path card_name,
                             CardViewParams params,
                             FlexibleRef<Project> project)
    : m_CardName{ std::move(card_name) }
    , m_ViewParams{ params }
    , m_Project{ project }
{
    const auto& connect_signals{
        [this](const fs::path& card_name, ProjectCardSignaller* sig)
        {
            if (card_name == m_CardName)
            {
                QObject::connect(sig, &ProjectCardSignaller::PreviewRemoved, this, &CardViewModel::PreviewRemoved);
                QObject::connect(sig, &ProjectCardSignaller::PreviewUpdated, this, &CardViewModel::PreviewUpdated);
            }
        }
    };

    if (m_Project.Get().m_CardSignallers.contains(m_CardName))
    {
        connect_signals(m_CardName, m_Project.Get().m_CardSignallers.at(m_CardName).get());
    }
    else
    {
        QObject::connect(
            &m_Project.Get(),
            &Project::CardSignallerAdded,
            this,
            connect_signals);
    }

    QObject::connect(&project.Get(), &Project::CardSizeChanged, this, &CardViewModel::CardSizeChanged);
}

void CardViewModel::SetCardName(const fs::path& card_name)
{
    m_CardName = card_name;
    CardNameChanged(m_CardName);

    const bool has_image{ m_Project->HasPreview(m_CardName) };
    if (has_image)
    {
        PreviewUpdated(m_Project->GetPreview(m_CardName));
    }
    else
    {
        PreviewRemoved();
    }
}
const fs::path& CardViewModel::GetCardName() const
{
    return m_CardName;
}

float CardViewModel::GetCardAspectRatio() const
{
    return GetCardWidgetAspectRatio(m_Project.Get(), m_ViewParams.m_Rotation, m_ViewParams.m_BleedEdge);
}

void CardViewModel::PreviewUpdated(const ImagePreview& preview)
{
    const auto get_image{
        [&]()
        {
            if (m_ViewParams.m_BleedEdge > 0_mm)
            {
                if (m_ViewParams.m_RoundedCorners)
                {
                    const auto finalize_image{
                        [&, this](const Image& base_image)
                        {
                            return UncropImage(base_image,
                                               m_CardName,
                                               m_Project->CardSize(),
                                               m_ViewParams.m_BleedEdge,
                                               UncropMode::Transparent)
                                .Rotate(m_ViewParams.m_Rotation);
                        }
                    };
                    if (m_Project->IsCardRoundedRect())
                    {
                        return finalize_image(
                            preview.m_CroppedImage
                                .RoundCorners(m_Project->CardSize(),
                                              m_Project->CardCornerRadius()));
                    }
                    else if (m_Project->IsCardSvg())
                    {
                        return finalize_image(
                            preview.m_CroppedImage
                                .Mirror(false, m_ViewParams.m_Backside)
                                .ClipSvg(m_Project->CardSvgData())
                                .Mirror(false, m_ViewParams.m_Backside));
                    }
                }
                return CropImage(preview.m_UncroppedImage,
                                 m_CardName,
                                 m_Project->CardSize(),
                                 m_Project->CardFullBleed(),
                                 m_ViewParams.m_BleedEdge,
                                 6800_dpi)
                    .Rotate(m_ViewParams.m_Rotation);
            }
            else
            {
                if (m_ViewParams.m_RoundedCorners)
                {
                    if (m_Project->IsCardRoundedRect())
                    {
                        return preview
                            .m_CroppedImage
                            .RoundCorners(m_Project->CardSize(),
                                          m_Project->CardCornerRadius())
                            .Rotate(m_ViewParams.m_Rotation);
                    }
                    else if (m_Project->IsCardSvg())
                    {
                        return preview
                            .m_CroppedImage
                            .Mirror(false, m_ViewParams.m_Backside)
                            .ClipSvg(m_Project->CardSvgData())
                            .Mirror(false, m_ViewParams.m_Backside)
                            .Rotate(m_ViewParams.m_Rotation);
                    }
                }

                return preview
                    .m_CroppedImage
                    .Rotate(m_ViewParams.m_Rotation);
            }
        }
    };

    const auto image{ get_image() };
    const auto pixmap{ StoreIntoQtPixmap(image) };
    PixmapChanged(pixmap);

    const bool bad_aspect_ratio{ preview.m_BadAspectRatio };
    const bool bad_rotation{ preview.m_BadRotation };
    if (bad_rotation || bad_aspect_ratio)
    {
        CardWarningChanged(GetCardWarning(bad_aspect_ratio, bad_rotation));
    }
    else
    {
        CardWarningChanged();
    }

    SpinnerVisibleChanged(false);
}
void CardViewModel::PreviewRemoved()
{
    const auto get_empty_image{
        [this]()
        {
            TRACY_AUTO_SCOPE();

            const auto width{ m_Project->m_Cfg.m_BasePreviewWidth };
            const auto height{ width * m_Project->CardRatio() };
            return Image::PlainColor({ width, height }, ColorRGBA8{ 0x80, 0x80, 0x80, 0xff })
                .Rotate(m_ViewParams.m_Rotation);
        }
    };

    const auto image{ get_empty_image() };
    const auto pixmap{ StoreIntoQtPixmap(image) };
    PixmapChanged(pixmap);

    SpinnerVisibleChanged(true);
}

void CardViewModel::CardSizeChanged(Size /* card_size */)
{
    CardAspectRatioChanged(GetCardAspectRatio());
}

void CardViewModel::RemoveExternalCard()
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        project->RemoveExternalCard(m_CardName);
    }
    else
    {
        LogWarning("Attempted to remove external card {} but could not.",
                   m_CardName.string());
    }
}

void CardViewModel::ClearBackside()
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        project->ClearBacksideImage(m_CardName);
    }
    else
    {
        LogWarning("Attempted to clear backside of card {} but could not.",
                   m_CardName.string());
    }
}
void CardViewModel::ResetBackside()
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        project->SetBacksideImage(m_CardName, "");
    }
    else
    {
        LogWarning("Attempted to reset backside of card {} but could not.",
                   m_CardName.string());
    }
}

void CardViewModel::ChangeBleedType(BleedType bleed_type)
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        project->SetCardBleedType(m_CardName, bleed_type);
    }
    else
    {
        LogWarning("Attempted to change bleed type of card {} but could not.",
                   m_CardName.string());
    }
}
void CardViewModel::ChangeBadAspectRatioHandling(BadAspectRatioHandling ratio_handling)
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        project->SetCardBadAspectRatioHandling(m_CardName, ratio_handling);
    }
    else
    {
        LogWarning("Attempted to change aspect ratio handling of card {} but could not.",
                   m_CardName.string());
    }
}

void CardViewModel::RotateImageLeft(const QPixmap& pixmap)
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        if (project->RotateCardLeft(m_CardName))
        {
            const auto rotated{
                pixmap
                    .transformed(QTransform().rotate(-90))
                    .scaled(pixmap.size())
            };
            PixmapChanged(rotated);
        }
    }
    else
    {
        LogWarning("Attempted to rotate card {} but could not.",
                   m_CardName.string());
    }
}
void CardViewModel::RotateImageRight(const QPixmap& pixmap)
{
    if (auto* project{ m_Project.TryGetMutable() })
    {
        if (project->RotateCardRight(m_CardName))
        {
            const auto rotated{
                pixmap
                    .transformed(QTransform().rotate(90))
                    .scaled(pixmap.size())
            };
            PixmapChanged(rotated);
        }
    }
    else
    {
        LogWarning("Attempted to rotate card {} but could not.",
                   m_CardName.string());
    }
}

void CardViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    CardNameChanged(m_CardName);

    MinimumWidthChanged(m_ViewParams.m_MinimumWidth);

    const bool has_image{ m_Project->HasPreview(m_CardName) };
    if (has_image)
    {
        PreviewUpdated(m_Project->GetPreview(m_CardName));
    }
    else
    {
        PreviewRemoved();
    }
}

CardContextMenuEntries CardViewModel::GetVisibleContextMenuEntries() const
{
    CardContextMenuEntries visible_entries{ CardContextMenuEntries::None };

    if (m_Project->IsCardExternal(m_CardName))
    {
        visible_entries |= CardContextMenuEntries::RemoveExternal;
    }

    if (m_Project->m_Data.m_BacksideEnabled)
    {
        const auto has_clear_backside{ m_Project->HasClearBacksideImage(m_CardName) };
        if (!has_clear_backside)
        {
            visible_entries |= CardContextMenuEntries::ClearBackside;
        }

        const auto has_non_default_backside{ m_Project->HasNonDefaultBacksideImage(m_CardName) };
        if (has_non_default_backside)
        {
            visible_entries |= CardContextMenuEntries::ResetBackside;
        }
    }

    visible_entries |= CardContextMenuEntries::InferBleed;
    visible_entries |= CardContextMenuEntries::ForceFullBleed;
    visible_entries |= CardContextMenuEntries::ForceNoBleed;

    {
        const auto preview{ m_Project->GetPreview(m_CardName) };
        const auto bad_aspect_ratio{ preview.m_BadAspectRatio ||
                                     preview.m_BadRotation };
        const auto bad_aspect_ratio_handling{
            m_Project->GetCardBadAspectRatioHandling(m_CardName)
        };

        if (bad_aspect_ratio || bad_aspect_ratio_handling != BadAspectRatioHandling::Default)
        {
            visible_entries |= CardContextMenuEntries::RatioIgnore;
            visible_entries |= CardContextMenuEntries::RatioExpand;
            visible_entries |= CardContextMenuEntries::RatioCrop;
            visible_entries |= CardContextMenuEntries::RatioStretch;
        }
    }

    visible_entries |= CardContextMenuEntries::RotateLeft;
    visible_entries |= CardContextMenuEntries::RotateRight;

    visible_entries |= CardContextMenuEntries::SkipSlot;

    return visible_entries;
}
CardContextMenuEntries CardViewModel::GetEnabledContextMenuEntries() const

{
    CardContextMenuEntries enabled_entries{ CardContextMenuEntries::None };

    enabled_entries |= CardContextMenuEntries::RemoveExternal;

    enabled_entries |= CardContextMenuEntries::ClearBackside;
    enabled_entries |= CardContextMenuEntries::ResetBackside;

    {
        const auto bleed_type{ m_Project->GetCardBleedType(m_CardName) };

        if (bleed_type != BleedType::Infer)
        {
            enabled_entries |= CardContextMenuEntries::InferBleed;
        }
        if (bleed_type != BleedType::FullBleed)
        {
            enabled_entries |= CardContextMenuEntries::ForceFullBleed;
        }
        if (bleed_type != BleedType::NoBleed)
        {
            enabled_entries |= CardContextMenuEntries::ForceNoBleed;
        }
    }

    {
        const auto bad_aspect_ratio_handling{
            m_Project->GetCardBadAspectRatioHandling(m_CardName)
        };

        if (bad_aspect_ratio_handling != BadAspectRatioHandling::Ignore)
        {
            enabled_entries |= CardContextMenuEntries::RatioIgnore;
        }
        if (bad_aspect_ratio_handling != BadAspectRatioHandling::Expand)
        {
            enabled_entries |= CardContextMenuEntries::RatioExpand;
        }
        if (bad_aspect_ratio_handling != BadAspectRatioHandling::Crop)
        {
            enabled_entries |= CardContextMenuEntries::RatioCrop;
        }
        if (bad_aspect_ratio_handling != BadAspectRatioHandling::Stretch)
        {
            enabled_entries |= CardContextMenuEntries::RatioStretch;
        }
    }

    enabled_entries |= CardContextMenuEntries::RotateLeft;
    enabled_entries |= CardContextMenuEntries::RotateRight;

    enabled_entries |= CardContextMenuEntries::SkipSlot;

    return enabled_entries;
}
