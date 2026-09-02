#pragma once

#include <QLabel>

#include <ppp/util.hpp>
#include <ppp/util/bit_field.hpp>

#include <ppp/project/project_types.hpp>

#include <ppp/ui/widget_util/card/card_widget_params.hpp>
#include <ppp/ui/widget_util/card/widget_with_card_size.hpp>

class QAction;

class Project;
struct ImagePreview;

enum class CardContextMenuFeatures
{
    RemoveExternal = Bit(0),
    Backside = Bit(1),
    BleedControls = Bit(2),
    RatioControls = Bit(3),
    Rotation = Bit(4),

    SkipSlot = Bit(5),

    Default = RemoveExternal | Backside | BleedControls | RatioControls | Rotation,
};
ENABLE_BITFIELD_OPERATORS(CardContextMenuFeatures);

class CardImage : public WidgetWithCardSize<QLabel>
{
    Q_OBJECT

  public:
    CardImage(const fs::path& card_name, const Project& project, CardImageWidgetParams params);

    void Refresh(const fs::path& card_name, const Project& project, CardImageWidgetParams params);
    void RefreshSize(const Project& project);

    void EnableContextMenu(bool enable,
                           Project& project,
                           CardContextMenuFeatures features = CardContextMenuFeatures::Default);

    const fs::path& GetCardName() const
    {
        return m_CardName;
    }

  private slots:
    void PreviewRemoved(const fs::path& card_name);
    void PreviewUpdated(const fs::path& card_name, const ImagePreview& preview);
    void CardBacksideChanged(const fs::path& card_name, OptionalImageRef backside);

  private:
    Image GetImage(const ImagePreview& preview) const;
    Image GetEmptyImage() const;
    QPixmap FinalizePixmap(Image image) const;
    void AddBadFormatWarning(const ImagePreview& preview);

    void ContextMenuRequested(QPoint pos);

    void RemoveExternalCard(Project& project);

    void ClearBackside(Project& project);
    void ResetBackside(Project& project);

    void ChangeBleedType(Project& project, BleedType bleed_type);
    void ChangeBadAspectRatioHandling(Project& project,
                                      BadAspectRatioHandling ratio_handling);

    void RotateImageLeft(Project& project);
    void RotateImageRight(Project& project);

    void ClearChildren();

  signals:
    void SkipThisSlot();

  private:
    const Project& m_Project;

    fs::path m_CardName;
    CardImageWidgetParams m_OriginalParams;

    Length m_FullBleed;
    Length m_CornerRadius;

    bool m_IsExternalCard{ false };
    bool m_BacksideEnabled{ false };
    bool m_HasClearBackside{ false };
    bool m_HasNonDefaultBackside{ false };
    bool m_BadAspectRatio{ false };
    BleedType m_BleedType{ BleedType::Default };
    BadAspectRatioHandling m_BadAspectRatioHandling{ BadAspectRatioHandling::Default };

    QWidget* m_Warning{ nullptr };
    QWidget* m_Spinner{ nullptr };

    QAction* m_RemoveExternalCardAction{ nullptr };

    QAction* m_ClearBacksideAction{ nullptr };
    QAction* m_ResetBacksideAction{ nullptr };

    QAction* m_InferBleedAction{ nullptr };
    QAction* m_ForceFullBleedAction{ nullptr };
    QAction* m_ForceNoBleedAction{ nullptr };

    QAction* m_FixRatioIgnoreAction{ nullptr };
    QAction* m_FixRatioExpandAction{ nullptr };
    QAction* m_FixRatioCropAction{ nullptr };
    QAction* m_FixRatioStretchAction{ nullptr };

    QAction* m_RotateLeftAction{ nullptr };
    QAction* m_RotateRightAction{ nullptr };

    QAction* m_SkipSlotAction{ nullptr };
};
