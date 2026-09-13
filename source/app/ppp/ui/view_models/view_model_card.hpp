#pragma once

#include <QObject>

#include <ppp/util.hpp>
#include <ppp/util/bit_field.hpp>
#include <ppp/util/flexible_ref.hpp>

#include <ppp/project/project_types.hpp>

#include <ppp/ui/view_models/card_view_params.hpp>

class Project;

class Image;
struct ImagePreview;

enum class CardContextMenuEntries
{
    None = 0,

    RemoveExternal = Bit(0),

    ClearBackside = Bit(1),
    ResetBackside = Bit(2),
    Backside = ClearBackside | ResetBackside,

    InferBleed = Bit(3),
    ForceFullBleed = Bit(4),
    ForceNoBleed = Bit(5),
    BleedType = InferBleed | ForceFullBleed | ForceNoBleed,

    RatioIgnore = Bit(6),
    RatioExpand = Bit(7),
    RatioCrop = Bit(8),
    RatioStretch = Bit(9),
    BadRatioHandling = RatioIgnore | RatioExpand | RatioCrop | RatioStretch,

    RotateLeft = Bit(10),
    RotateRight = Bit(11),
    RotateImage = RotateLeft | RotateRight,

    SkipSlot = Bit(12),
};
ENABLE_BITFIELD_OPERATORS(CardContextMenuEntries);

class CardViewModel : public QObject
{
    Q_OBJECT

    friend class CardImage;

  public:
    CardViewModel(fs::path card_name,
                  CardViewParams params,
                  FlexibleRef<Project> project);

    void SetCardName(const fs::path& card_name);
    const fs::path& GetCardName() const;

    float GetCardAspectRatio() const;

  signals:
    // forward

    void SkipThisSlot();

  signals:
    void CardNameChanged(const fs::path& card_name);

    void CardAspectRatioChanged(float aspect_ratio);

    void MinimumWidthChanged(Pixel minimum_width);

    void PixmapChanged(const QPixmap& pixmap);

    void CardWarningChanged(const QString& warning = "");
    void SpinnerVisibleChanged(bool spinner_visible);

  public slots:
    void PreviewUpdated(const ImagePreview& preview);
    void PreviewRemoved();

    void CardSizeChanged(Size card_size);

  private slots:
    void RemoveExternalCard();

    void ClearBackside();
    void ResetBackside();

    void ChangeBleedType(BleedType bleed_type);
    void ChangeBadAspectRatioHandling(BadAspectRatioHandling ratio_handling);

    void RotateImageLeft(const QPixmap& pixmap);
    void RotateImageRight(const QPixmap& pixmap);

  private:
    void EmitDefaults();

    CardContextMenuEntries GetVisibleContextMenuEntries() const;
    CardContextMenuEntries GetEnabledContextMenuEntries() const;

    fs::path m_CardName;
    CardViewParams m_ViewParams;

    FlexibleRef<Project> m_Project;
};
