#pragma once

#include <QLabel>
#include <QStackedWidget>

#include <ppp/image.hpp>
#include <ppp/util.hpp>

#include <ppp/util/bit_field.hpp>

#include <ppp/project/card_info.hpp>

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

class CardSizedLabel : public QLabel
{
  public:
    struct Params
    {
        Image::Rotation m_Rotation{ Image::Rotation::None };
        Length m_BleedEdge{ 0_mm };
    };

    CardSizedLabel(const Project& project, Params params = {});

    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

  protected:
    bool m_Rotated;
    Size m_CardSize;
    float m_CardRatio;
    Length m_BleedEdge;
};

class BlankCardImage : public CardSizedLabel
{
  public:
    struct Params
    {
        bool m_RoundedCorners{ true };
        Image::Rotation m_Rotation{ Image::Rotation::None };
        Length m_BleedEdge{ 0_mm };
        Pixel m_MinimumWidth{ 0_pix };
    };

    BlankCardImage(const Project& project, Params params = {});
};

class CardImage : public CardSizedLabel
{
    Q_OBJECT

  public:
    struct Params
    {
        bool m_RoundedCorners{ true };
        Image::Rotation m_Rotation{ Image::Rotation::None };
        Length m_BleedEdge{ 0_mm };
        Pixel m_MinimumWidth{ 0_pix };
    };

    CardImage(const fs::path& card_name, const Project& project, Params params);

    void Refresh(const fs::path& card_name, const Project& project, Params params);

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
    Params m_OriginalParams;

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
    QAction* m_FixRatioStretchAction{ nullptr };

    QAction* m_RotateLeftAction{ nullptr };
    QAction* m_RotateRightAction{ nullptr };

    QAction* m_SkipSlotAction{ nullptr };
};

class BacksideImage : public CardImage
{
  public:
    BacksideImage(const fs::path& backside_name, const Project& project);
    BacksideImage(const fs::path& backside_name, Pixel minimum_width, const Project& project);

    void Refresh(const fs::path& backside_name, const Project& project);
    void Refresh(const fs::path& backside_name, Pixel minimum_width, const Project& project);
};

class StackedCardBacksideView : public QStackedWidget
{
    Q_OBJECT

  public:
    StackedCardBacksideView(CardImage* image, QWidget* backside);

    void RefreshBackside(QWidget* new_backside);

    void RotateImageLeft();
    void RotateImageRight();

    virtual bool hasHeightForWidth() const override
    {
        return true;
    }
    virtual int heightForWidth(int width) const override;

  signals:
    void BacksideClicked();

  private:
    void RefreshSizes(QSize size);

    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;

    CardImage* m_Image;
    QWidget* m_Backside;
    QWidget* m_BacksideContainer;
};
