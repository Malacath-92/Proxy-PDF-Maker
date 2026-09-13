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

#include <ppp/ui/view_models/util.hpp>
#include <ppp/ui/view_models/view_model_card.hpp>

#include <ppp/profile/profile.hpp>

CardImage::CardImage(CardViewModel* view_model)
    : WidgetWithCardSize{ view_model->GetCardAspectRatio() }
    , m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    m_ViewModel.setParent(this);

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

        m_Warning = new QLabel;
        m_Warning->setPixmap(s_WarningPixmap);
        m_Warning->setFixedWidth(c_WarningSize);
        m_Warning->setFixedHeight(c_WarningSize);
        m_Warning->setVisible(false);

        m_Spinner = new SpinnerWidget;

        auto* layout{ new QVBoxLayout };
        layout->addWidget(m_Warning, 0, Qt::AlignLeft);
        layout->addStretch();
        layout->addWidget(m_Spinner, 0, Qt::AlignCenter);
        layout->addStretch();
        setLayout(layout);
    }

    setStyleSheet("QLabel{ background-color: transparent; }");
    setScaledContents(true);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardNameChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardAspectRatioChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(MinimumWidthChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PixmapChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardWarningChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(SpinnerVisibleChanged);

    m_ViewModel.EmitDefaults();
}

void CardImage::EnableContextMenu(bool enable,
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
                             &m_ViewModel,
                             &CardViewModel::RemoveExternalCard);
        }

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_ClearBacksideAction = new QAction{ "Clear Backside", this };
            // m_ClearBacksideAction->setIcon(s_ClearIcon);
            m_ResetBacksideAction = new QAction{ "Reset Backside", this };
            m_ResetBacksideAction->setIcon(s_ClearIcon);

            QObject::connect(m_ClearBacksideAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             &CardViewModel::ClearBackside);
            QObject::connect(m_ResetBacksideAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             &CardViewModel::ResetBackside);
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
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBleedType, &m_ViewModel, BleedType::Infer));
            QObject::connect(m_ForceFullBleedAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBleedType, &m_ViewModel, BleedType::FullBleed));
            QObject::connect(m_ForceNoBleedAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBleedType, &m_ViewModel, BleedType::NoBleed));
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
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBadAspectRatioHandling, &m_ViewModel, BadAspectRatioHandling::Ignore));
            QObject::connect(m_FixRatioExpandAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBadAspectRatioHandling, &m_ViewModel, BadAspectRatioHandling::Expand));
            QObject::connect(m_FixRatioCropAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBadAspectRatioHandling, &m_ViewModel, BadAspectRatioHandling::Crop));
            QObject::connect(m_FixRatioStretchAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             std::bind_front(&CardViewModel::ChangeBadAspectRatioHandling, &m_ViewModel, BadAspectRatioHandling::Stretch));
        }

        if (IsSet(features, CardContextMenuFeatures::RemoveExternal))
        {
            m_RotateLeftAction = new QAction{ "Rotate Left", this };
            m_RotateLeftAction->setIcon(s_UntapIcon);
            m_RotateRightAction = new QAction{ "Rotate Right", this };
            m_RotateRightAction->setIcon(s_TapIcon);

            QObject::connect(m_RotateLeftAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             [this]()
                             { m_ViewModel.RotateImageLeft(pixmap()); });
            QObject::connect(m_RotateRightAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             [this]()
                             { m_ViewModel.RotateImageRight(pixmap()); });
        }

        if (IsSet(features, CardContextMenuFeatures::SkipSlot))
        {
            m_SkipSlotAction = new QAction{ "Skip Slot", this };
            // m_SkipSlotAction->setIcon(s_UntapIcon);

            QObject::connect(m_SkipSlotAction,
                             &QAction::triggered,
                             &m_ViewModel,
                             &CardViewModel::SkipThisSlot);
        }
    }
    else if (!enable)
    {
        LogError("Disabling context menu currently not supported.");
    }
}

void CardImage::CardNameChanged(const fs::path& card_name)
{
    setToolTip(ToQString(card_name));
}

void CardImage::CardAspectRatioChanged(float aspect_ratio)
{
    WidgetWithCardSize::ChangeAspectRatio(aspect_ratio);
}

void CardImage::MinimumWidthChanged(Pixel minimum_width)
{
    setMinimumWidth(minimum_width / 1_pix);
}

void CardImage::PixmapChanged(const QPixmap& pixmap)
{
    setPixmap(pixmap);
}

void CardImage::CardWarningChanged(const QString& warning)
{
    if (warning.isEmpty())
    {
        m_Warning->setVisible(false);
    }
    else
    {
        m_Warning->setVisible(true);
        m_Warning->setToolTip(warning);
    }
}
void CardImage::SpinnerVisibleChanged(bool spinner_visible)
{
    m_Spinner->setVisible(spinner_visible);
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

    const auto visible{ m_ViewModel.GetVisibleContextMenuEntries() };
    const auto enabled{ m_ViewModel.GetEnabledContextMenuEntries() };
    auto add_item{
        [&](auto* item, auto flag)
        {
            if (IsSet(visible, flag) && item != nullptr)
            {
                menu->addAction(item);
                item->setEnabled(IsSet(enabled, flag));
            }
        }
    };

    using enum CardContextMenuEntries;

    if (IsAnySet(visible, RemoveExternal) && m_RemoveExternalCardAction != nullptr)
    {
        begin_section();
        add_item(m_RemoveExternalCardAction, RemoveExternal);
    }

    if (IsAnySet(visible, Backside) && m_ClearBacksideAction != nullptr)
    {
        begin_section();
        add_item(m_ClearBacksideAction, ClearBackside);
        add_item(m_ResetBacksideAction, ResetBackside);
    }

    if (IsAnySet(visible, BleedType) && m_InferBleedAction != nullptr)
    {
        begin_section();

        add_item(m_InferBleedAction, InferBleed);
        add_item(m_ForceFullBleedAction, ForceFullBleed);
        add_item(m_ForceNoBleedAction, ForceNoBleed);
    }

    if (IsAnySet(visible, BadRatioHandling) && m_FixRatioIgnoreAction != nullptr)
    {
        begin_section();

        add_item(m_FixRatioIgnoreAction, RatioIgnore);
        add_item(m_FixRatioExpandAction, RatioExpand);
        add_item(m_FixRatioCropAction, RatioCrop);
        add_item(m_FixRatioStretchAction, RatioStretch);
    }

    if (IsAnySet(visible, RotateImage) && m_RotateLeftAction != nullptr)
    {
        begin_section();

        add_item(m_RotateLeftAction, RotateLeft);
        add_item(m_RotateRightAction, RotateRight);
    }

    if (IsAnySet(visible, SkipSlot) && m_SkipSlotAction != nullptr)
    {
        begin_section();

        add_item(m_SkipSlotAction, SkipSlot);
    }

    menu->popup(mapToGlobal(pos));
}
