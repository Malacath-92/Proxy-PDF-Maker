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

class CardViewModel;

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
    CardImage(CardViewModel* view_model);

    void EnableContextMenu(bool enable,
                           CardContextMenuFeatures features = CardContextMenuFeatures::Default);

  private slots:
    void CardNameChanged(const fs::path& card_name);

    void CardAspectRatioChanged(float aspect_ratio);

    void MinimumWidthChanged(Pixel minimum_width);

    void PixmapChanged(const QPixmap& pixmap);

    void CardWarningChanged(const QString& warning);
    void SpinnerVisibleChanged(bool spinner_visible);

  private:
    void ContextMenuRequested(QPoint pos);

  private:
    CardViewModel& m_ViewModel;

    QLabel* m_Warning{ nullptr };
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
