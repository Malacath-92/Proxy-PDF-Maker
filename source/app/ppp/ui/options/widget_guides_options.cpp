#include <ppp/ui/options/widget_guides_options.hpp>

#include <charconv>

#include <nlohmann/json.hpp>

#include <QCheckBox>
#include <QColorDialog>
#include <QPushButton>
#include <QVBoxLayout>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/default_project_value_actions.hpp>
#include <ppp/ui/widget_util/widget_label.hpp>

#include <ppp/ui/view_models/options/view_model_guides_options.hpp>
#include <ppp/ui/view_models/util.hpp>

#include <ppp/profile/profile.hpp>

GuidesOptionsWidget::GuidesOptionsWidget(GuidesOptionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Guides Options");
    m_ViewModel.setParent(this);

    const auto config_reqs{ m_ViewModel.GetDefaultDataRequirements() };
    const auto base_unit{ m_ViewModel.GetBaseUnit() };

    m_ExportExactGuidesCheckbox = new QCheckBox{ "Export Exact Guides" };
    m_ExportExactGuidesCheckbox->setToolTip("Decides whether a .svg file will be generated that contains the exact guides for the current layout");
    EnableOptionWidgetForDefaults(m_ExportExactGuidesCheckbox, config_reqs, "export_exact_guides");

    m_EnableGuidesCheckbox = new QCheckBox{ "Enable Guides" };
    m_EnableGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered on the output");
    EnableOptionWidgetForDefaults(m_EnableGuidesCheckbox, config_reqs, "enable_guides");

    m_EnableBacksideGuidesCheckbox = new QCheckBox{ "Enable Backside Guides" };
    m_EnableBacksideGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered on backside pages");
    EnableOptionWidgetForDefaults(m_EnableBacksideGuidesCheckbox, config_reqs, "enable_backside_guides");

    m_CornerGuidesCheckbox = new QCheckBox{ "Enable Corner Guides" };
    m_CornerGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered in the corner of each card");
    EnableOptionWidgetForDefaults(m_CornerGuidesCheckbox, config_reqs, "corner_guides");

    m_CrossGuidesCheckbox = new QCheckBox{ "Cross Guides" };
    m_CrossGuidesCheckbox->setToolTip("Decides whether cutting guides are crosses or just corners");
    EnableOptionWidgetForDefaults(m_CrossGuidesCheckbox, config_reqs, "cross_guides");

    auto* guides_offset{ new LengthSpinBoxWithLabel{ "Guides O&ffset", base_unit } };
    m_GuidesOffsetSpin = guides_offset->GetWidget();
    m_GuidesOffsetSpin->ConnectUnitSignals(this);
    m_GuidesOffsetSpin->setDecimals(3);
    m_GuidesOffsetSpin->setSingleStep(0.1);
    m_GuidesOffsetSpin->setToolTip("Decides where to place the guides, at 0 the guides' center will align with the card corner");
    EnableOptionWidgetForDefaults(m_GuidesOffsetSpin, config_reqs, "guides_offset_cm");

    auto* guides_length{ new LengthSpinBoxWithLabel{ "Guides &Length", base_unit } };
    m_GuidesLengthSpin = guides_length->GetWidget();
    m_GuidesLengthSpin->ConnectUnitSignals(this);
    m_GuidesLengthSpin->setDecimals(2);
    m_GuidesLengthSpin->setSingleStep(0.1);
    m_GuidesLengthSpin->setToolTip("Decides how long the guides are");
    EnableOptionWidgetForDefaults(m_GuidesLengthSpin, config_reqs, "guides_length_cm");

    m_ExtendedGuidesCheckbox = new QCheckBox{ "Extended Guides" };
    m_ExtendedGuidesCheckbox->setToolTip("Decides whether cutting guides extend to the edge of the page");
    EnableOptionWidgetForDefaults(m_ExtendedGuidesCheckbox, config_reqs, "extended_guides");

    auto* guides_color_a_button{ new QPushButton };
    m_GuidesColorA = new WidgetWithLabel{ "Guides Color A", guides_color_a_button };
    EnableOptionWidgetForDefaults(
        m_GuidesColorA,
        config_reqs,
        "guides_color_a",
        [this](nlohmann::json default_value)
        {
            const ColorRGB8 guides_color{
                default_value[0],
                default_value[1],
                default_value[2]
            };
            m_ViewModel.ChangeGuidesColorA(guides_color);
        },
        [this]()
        {
            const auto guides_color{ ColorFromBackgroundStyle(m_GuidesColorA->styleSheet()) };
            return std::array{
                guides_color.r,
                guides_color.g,
                guides_color.b
            };
        });

    auto* guides_color_b_button{ new QPushButton };
    m_GuidesColorB = new WidgetWithLabel{ "Guides Color B", guides_color_b_button };
    EnableOptionWidgetForDefaults(
        m_GuidesColorB,
        config_reqs,
        "guides_color_b",
        [this](nlohmann::json default_value)
        {
            const ColorRGB8 guides_color{
                default_value[0],
                default_value[1],
                default_value[2]
            };
            m_ViewModel.ChangeGuidesColorB(guides_color);
        },
        [this]()
        {
            const auto guides_color{ ColorFromBackgroundStyle(m_GuidesColorB->styleSheet()) };
            return std::array{
                guides_color.r,
                guides_color.g,
                guides_color.b
            };
        });

    auto* guides_thickness{ new LengthSpinBoxWithLabel{ "Guides Thic&kness", base_unit } };
    m_GuidesThicknessSpin = guides_thickness->GetWidget();
    m_GuidesThicknessSpin->ConnectUnitSignals(this);
    m_GuidesThicknessSpin->setDecimals(4);
    m_GuidesThicknessSpin->setSingleStep(0.01);
    m_GuidesThicknessSpin->setToolTip("Decides how thick the guides are");
    EnableOptionWidgetForDefaults(m_GuidesThicknessSpin, config_reqs, "guides_thickness_cm");

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_ExportExactGuidesCheckbox);
    layout->addWidget(m_EnableGuidesCheckbox);
    layout->addWidget(m_EnableBacksideGuidesCheckbox);
    layout->addWidget(m_CornerGuidesCheckbox);
    layout->addWidget(m_CrossGuidesCheckbox);
    layout->addWidget(guides_offset);
    layout->addWidget(guides_length);
    layout->addWidget(m_ExtendedGuidesCheckbox);
    layout->addWidget(m_GuidesColorA);
    layout->addWidget(m_GuidesColorB);
    layout->addWidget(guides_thickness);
    setLayout(layout);

    auto pick_color{
        [](const QWidget& button) -> std::optional<ColorRGB8>
        {
            const auto color{ ColorFromBackgroundStyle(button.styleSheet()) };
            const QColor initial_color{ color.r, color.g, color.b };
            const QColor picked_color{ QColorDialog::getColor(initial_color) };
            if (picked_color.isValid())
            {
                const std::string new_color{ picked_color.name().toStdString() };
                uint32_t color_uint{};
                std::from_chars(new_color.c_str() + 1, new_color.c_str() + new_color.size(), color_uint, 16);
                return ColorRGB8{
                    static_cast<uint8_t>((color_uint >> 16) & 0xff),
                    static_cast<uint8_t>((color_uint >> 8) & 0xff),
                    static_cast<uint8_t>(color_uint & 0xff),
                };
            }
            return std::nullopt;
        }
    };

    auto pick_color_a{
        [=, this]()
        {
            if (const auto picked_color{ pick_color(*m_GuidesColorA->GetWidget()) })
            {
                m_ViewModel.ChangeGuidesColorA(picked_color.value());
            }
        }
    };

    auto pick_color_b{
        [=, this]()
        {
            if (const auto picked_color{ pick_color(*m_GuidesColorB->GetWidget()) })
            {
                m_ViewModel.ChangeGuidesColorB(picked_color.value());
            }
        }
    };

    QObject::connect(m_ExportExactGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeExportExactGuides);
    QObject::connect(m_EnableGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeGuidesEnabled);
    QObject::connect(m_EnableBacksideGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeBacksideGuidesEnabled);
    QObject::connect(m_CornerGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeCornerGuidesEnabled);
    QObject::connect(m_CrossGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeCrossGuidesEnabled);
    QObject::connect(m_GuidesOffsetSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeGuidesOffset);
    QObject::connect(m_GuidesLengthSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeGuidesLength);
    QObject::connect(m_ExtendedGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeExtendedGuidesEnabled);
    QObject::connect(guides_color_a_button,
                     &QPushButton::clicked,
                     this,
                     pick_color_a);
    QObject::connect(guides_color_b_button,
                     &QPushButton::clicked,
                     this,
                     pick_color_b);
    QObject::connect(m_GuidesThicknessSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     &GuidesOptionsViewModel::ChangeGuidesThickness);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(AdvancedModeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ExportExactGuidesChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(GuidesEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideGuidesEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CornerGuidesEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CrossGuidesEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ExtendedGuidesEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(GuidesColorAChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(GuidesColorBChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(GuidesOffsetChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(GuidesLengthChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(GuidesThicknessChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardSizeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BleedEdgeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideEnabledChanged);

    m_ViewModel.EmitDefaults();
}

void GuidesOptionsWidget::AdvancedModeChanged(bool advanced_mode)
{
    // Always enabled: m_EnableGuidesCheckbox, m_CornerGuidesCheckbox, m_ExtendedGuidesCheckbox, m_GuidesColorA, m_GuidesColorB
    m_ExportExactGuidesCheckbox->setVisible(advanced_mode);
    m_EnableBacksideGuidesCheckbox->setVisible(advanced_mode);
    m_CrossGuidesCheckbox->setVisible(advanced_mode);
    m_GuidesOffsetSpin->parentWidget()->setVisible(advanced_mode);
    m_GuidesLengthSpin->parentWidget()->setVisible(advanced_mode);
    m_GuidesThicknessSpin->parentWidget()->setVisible(advanced_mode);
}

void GuidesOptionsWidget::ExportExactGuidesChanged(bool export_exact_guides)
{
    m_ExportExactGuidesCheckbox->blockSignals(true);
    m_ExportExactGuidesCheckbox->setChecked(export_exact_guides);
    m_ExportExactGuidesCheckbox->blockSignals(false);
}
void GuidesOptionsWidget::GuidesEnabledChanged(bool guides_enabled)
{
    m_EnableGuidesCheckbox->blockSignals(true);
    m_EnableGuidesCheckbox->setChecked(guides_enabled);
    m_EnableGuidesCheckbox->blockSignals(false);

    m_EnableBacksideGuidesCheckbox->setEnabled(guides_enabled);
    m_ExtendedGuidesCheckbox->setEnabled(guides_enabled);
    m_CornerGuidesCheckbox->setEnabled(guides_enabled);
    m_GuidesColorA->setEnabled(guides_enabled);
    m_GuidesColorB->setEnabled(guides_enabled);
    m_GuidesThicknessSpin->setEnabled(guides_enabled);

    const bool corner_guides_enabled{ m_ViewModel.GetCornerGuidesEnabled() };
    m_CrossGuidesCheckbox->setEnabled(corner_guides_enabled && guides_enabled);
    m_GuidesOffsetSpin->setEnabled(corner_guides_enabled && guides_enabled);
    m_GuidesLengthSpin->setEnabled(corner_guides_enabled && guides_enabled);
}
void GuidesOptionsWidget::BacksideGuidesEnabledChanged(bool backside_guides_enabled)
{
    m_EnableBacksideGuidesCheckbox->blockSignals(true);
    m_EnableBacksideGuidesCheckbox->setChecked(backside_guides_enabled);
    m_EnableBacksideGuidesCheckbox->blockSignals(false);
}
void GuidesOptionsWidget::CornerGuidesEnabledChanged(bool corner_guides_enabled)
{
    m_CornerGuidesCheckbox->blockSignals(true);
    m_CornerGuidesCheckbox->setChecked(corner_guides_enabled);
    m_CornerGuidesCheckbox->blockSignals(false);

    m_CrossGuidesCheckbox->setEnabled(corner_guides_enabled);
    m_GuidesOffsetSpin->setEnabled(corner_guides_enabled);
    m_GuidesLengthSpin->setEnabled(corner_guides_enabled);
}
void GuidesOptionsWidget::CrossGuidesEnabledChanged(bool cross_guides_enabled)
{
    m_CrossGuidesCheckbox->blockSignals(true);
    m_CrossGuidesCheckbox->setChecked(cross_guides_enabled);
    m_CrossGuidesCheckbox->blockSignals(false);
}
void GuidesOptionsWidget::ExtendedGuidesEnabledChanged(bool extended_guides_enabled)
{
    m_ExtendedGuidesCheckbox->blockSignals(true);
    m_ExtendedGuidesCheckbox->setChecked(extended_guides_enabled);
    m_ExtendedGuidesCheckbox->blockSignals(false);
}
void GuidesOptionsWidget::GuidesColorAChanged(ColorRGB8 guides_color)
{
    m_GuidesColorA->GetWidget()->setStyleSheet(ColorToBackgroundStyle(guides_color));
}
void GuidesOptionsWidget::GuidesColorBChanged(ColorRGB8 guides_color)
{
    m_GuidesColorB->GetWidget()->setStyleSheet(ColorToBackgroundStyle(guides_color));
}
void GuidesOptionsWidget::GuidesOffsetChanged(Length guides_offset)
{
    m_GuidesOffsetSpin->blockSignals(true);
    m_GuidesOffsetSpin->SetValue(guides_offset);
    m_GuidesOffsetSpin->blockSignals(false);
}
void GuidesOptionsWidget::GuidesLengthChanged(Length guides_length)
{
    m_GuidesLengthSpin->blockSignals(true);
    m_GuidesLengthSpin->SetValue(guides_length);
    m_GuidesLengthSpin->blockSignals(false);
}
void GuidesOptionsWidget::GuidesThicknessChanged(Length guides_thickness)
{
    m_GuidesThicknessSpin->blockSignals(true);
    m_GuidesThicknessSpin->SetValue(guides_thickness);
    m_GuidesThicknessSpin->blockSignals(false);
}

void GuidesOptionsWidget::CardSizeChanged(Size card_size)
{
    m_GuidesLengthSpin->blockSignals(true);
    m_GuidesLengthSpin->SetRange(0_mm, dla::math::min(card_size.x, card_size.y) / 2.0f);
    m_GuidesLengthSpin->blockSignals(false);

    if (const auto corner_radius{ m_ViewModel.GetCardCornerRadius() })
    {
        m_GuidesLengthSpin->SetValue(corner_radius.value() / 2.0f);
    }
}
void GuidesOptionsWidget::BleedEdgeChanged(Length bleed_edge)
{
    m_GuidesOffsetSpin->SetRange(0_mm, bleed_edge + m_ViewModel.GetEnvelopeBleedEdge());
}
void GuidesOptionsWidget::BacksideEnabledChanged(bool backside_enabled)
{
    m_EnableBacksideGuidesCheckbox->setEnabled(backside_enabled);
}

ColorRGB8 GuidesOptionsWidget::ColorFromBackgroundStyle(const QString& style)
{
    const auto hex{
        style
            .split("#")
            .back()
            .split(";")
            .front()
    };
    const auto r{ hex.mid(0, 2) };
    const auto g{ hex.mid(2, 2) };
    const auto b{ hex.mid(4, 2) };
    return ColorRGB8{
        static_cast<ColorRGB8::value_type>(r.toInt(nullptr, 16)),
        static_cast<ColorRGB8::value_type>(g.toInt(nullptr, 16)),
        static_cast<ColorRGB8::value_type>(b.toInt(nullptr, 16)),
    };
}
QString GuidesOptionsWidget::ColorToBackgroundStyle(ColorRGB8 color)
{
    return ToQString(fmt::format(":enabled {{ background-color:#{:0>6x}; }}", ColorToInt(color)));
}
