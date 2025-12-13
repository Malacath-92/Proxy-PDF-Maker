#include <ppp/ui/widget_guides_options.hpp>

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
#include <ppp/ui/widget_label.hpp>

GuidesOptionsWidget::GuidesOptionsWidget(Project& project)
    : m_Project{ project }
{
    setObjectName("Guides Options");

    m_ExportExactGuidesCheckbox = new QCheckBox{ "Export Exact Guides" };
    m_ExportExactGuidesCheckbox->setToolTip("Decides whether a .svg file will be generated that contains the exact guides for the current layout");
    EnableOptionWidgetForDefaults(m_ExportExactGuidesCheckbox, "export_exact_guides");

    m_EnableGuidesCheckbox = new QCheckBox{ "Enable Guides" };
    m_EnableGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered on the output");
    EnableOptionWidgetForDefaults(m_EnableGuidesCheckbox, "enable_guides");

    m_EnableBacksideGuidesCheckbox = new QCheckBox{ "Enable Backside Guides" };
    m_EnableBacksideGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered on backside pages");
    EnableOptionWidgetForDefaults(m_EnableBacksideGuidesCheckbox, "enable_backside_guides");

    m_CornerGuidesCheckbox = new QCheckBox{ "Enable Corner Guides" };
    m_CornerGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered in the corner of each card");
    EnableOptionWidgetForDefaults(m_CornerGuidesCheckbox, "corner_guides");

    m_CrossGuidesCheckbox = new QCheckBox{ "Cross Guides" };
    m_CrossGuidesCheckbox->setToolTip("Decides whether cutting guides are crosses or just corners");
    EnableOptionWidgetForDefaults(m_CrossGuidesCheckbox, "cross_guides");

    auto* guides_offset{ new LengthSpinBoxWithLabel{ "Guides O&ffset" } };
    m_GuidesOffsetSpin = guides_offset->GetWidget();
    m_GuidesOffsetSpin->ConnectUnitSignals(this);
    m_GuidesOffsetSpin->setDecimals(3);
    m_GuidesOffsetSpin->setSingleStep(0.1);
    m_GuidesOffsetSpin->setToolTip("Decides where to place the guides, at 0 the guides' center will align with the card corner");
    EnableOptionWidgetForDefaults(m_GuidesOffsetSpin, "guides_offset_cm");

    auto* guides_length{ new LengthSpinBoxWithLabel{ "Guides &Length" } };
    m_GuidesLengthSpin = guides_length->GetWidget();
    m_GuidesLengthSpin->ConnectUnitSignals(this);
    m_GuidesLengthSpin->setDecimals(2);
    m_GuidesLengthSpin->setSingleStep(0.1);
    m_GuidesLengthSpin->setToolTip("Decides how long the guides are");
    EnableOptionWidgetForDefaults(m_GuidesLengthSpin, "guides_length_cm");

    m_ExtendedGuidesCheckbox = new QCheckBox{ "Extended Guides" };
    m_ExtendedGuidesCheckbox->setToolTip("Decides whether cutting guides extend to the edge of the page");
    EnableOptionWidgetForDefaults(m_ExtendedGuidesCheckbox, "extended_guides");

    auto* guides_color_a_button{ new QPushButton };
    m_GuidesColorA = new WidgetWithLabel{ "Guides Color A", guides_color_a_button };
    EnableOptionWidgetForDefaults(
        m_GuidesColorA,
        "guides_color_a",
        [this, guides_color_a_button, &project](nlohmann::json default_value)
        {
            project.m_Data.m_GuidesColorA.r = default_value[0];
            project.m_Data.m_GuidesColorA.g = default_value[1];
            project.m_Data.m_GuidesColorA.b = default_value[2];
            guides_color_a_button->setStyleSheet(ColorToBackgroundStyle(m_Project.m_Data.m_GuidesColorA));
            GuidesColorChanged();
        },
        [&project]()
        {
            return std::array{
                project.m_Data.m_GuidesColorA.r,
                project.m_Data.m_GuidesColorA.g,
                project.m_Data.m_GuidesColorA.b
            };
        });

    auto* guides_color_b_button{ new QPushButton };
    m_GuidesColorB = new WidgetWithLabel{ "Guides Color B", guides_color_b_button };
    EnableOptionWidgetForDefaults(
        m_GuidesColorB,
        "guides_color_b",
        [this, guides_color_b_button, &project](nlohmann::json default_value)
        {
            project.m_Data.m_GuidesColorB.r = default_value[0];
            project.m_Data.m_GuidesColorB.g = default_value[1];
            project.m_Data.m_GuidesColorB.b = default_value[2];
            guides_color_b_button->setStyleSheet(ColorToBackgroundStyle(m_Project.m_Data.m_GuidesColorB));
            GuidesColorChanged();
        },
        [&project]()
        {
            return std::array{
                project.m_Data.m_GuidesColorB.r,
                project.m_Data.m_GuidesColorB.g,
                project.m_Data.m_GuidesColorB.b
            };
        });

    auto* guides_thickness{ new LengthSpinBoxWithLabel{ "Guides Thic&kness" } };
    m_GuidesThicknessSpin = guides_thickness->GetWidget();
    m_GuidesThicknessSpin->ConnectUnitSignals(this);
    m_GuidesThicknessSpin->setDecimals(4);
    m_GuidesThicknessSpin->setSingleStep(0.01);
    m_GuidesThicknessSpin->setToolTip("Decides how thick the guides are");
    EnableOptionWidgetForDefaults(m_GuidesThicknessSpin, "guides_thickness_cm");

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

    SetDefaults();

    auto change_export_exact_guides{
        [this](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            m_Project.m_Data.m_ExportExactGuides = enabled;

            ExactGuidesEnabledChanged();
        }
    };

    auto change_enable_guides{
        [this](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            m_Project.m_Data.m_EnableGuides = enabled;

            m_EnableBacksideGuidesCheckbox->setEnabled(enabled);
            m_ExtendedGuidesCheckbox->setEnabled(enabled);
            m_CornerGuidesCheckbox->setEnabled(enabled);
            m_CrossGuidesCheckbox->setEnabled(m_Project.m_Data.m_CornerGuides && enabled);
            m_GuidesOffsetSpin->setEnabled(m_Project.m_Data.m_CornerGuides && enabled);
            m_GuidesLengthSpin->setEnabled(m_Project.m_Data.m_CornerGuides && enabled);
            m_GuidesColorA->setEnabled(enabled);
            m_GuidesColorB->setEnabled(enabled);
            m_GuidesThicknessSpin->setEnabled(enabled);

            GuidesEnabledChanged();
        }
    };

    auto change_enable_backside_guides{
        [this, &project](Qt::CheckState s)
        {
            project.m_Data.m_BacksideEnableGuides = s == Qt::CheckState::Checked;
            BacksideGuidesEnabledChanged();
        }
    };

    auto change_corner_guides{
        [this](Qt::CheckState s)
        {
            m_Project.m_Data.m_CornerGuides = s == Qt::CheckState::Checked;
            m_CrossGuidesCheckbox->setEnabled(m_Project.m_Data.m_CornerGuides);
            m_GuidesOffsetSpin->setEnabled(m_Project.m_Data.m_CornerGuides);
            m_GuidesLengthSpin->setEnabled(m_Project.m_Data.m_CornerGuides);
            CornerGuidesChanged();
        }
    };

    auto change_cross_guides{
        [this](Qt::CheckState s)
        {
            m_Project.m_Data.m_CrossGuides = s == Qt::CheckState::Checked;
            CrossGuidesChanged();
        }
    };

    auto change_guides_offset{
        [this, &project](Length v)
        {
            if (dla::math::abs(project.m_Data.m_GuidesOffset - v) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_GuidesOffset = v;
            GuidesOffsetChanged();
        }
    };

    auto change_guides_length{
        [this, &project](Length v)
        {
            project.m_Data.m_GuidesLength = v;
            GuidesLengthChanged();
        }
    };

    auto change_extended_guides{
        [this](Qt::CheckState s)
        {
            m_Project.m_Data.m_ExtendedGuides = s == Qt::CheckState::Checked;
            ExtendedGuidesChanged();
        }
    };

    auto pick_color{
        [](const ColorRGB8& color) -> std::optional<ColorRGB8>
        {
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
            if (const auto picked_color{ pick_color(m_Project.m_Data.m_GuidesColorA) })
            {
                m_Project.m_Data.m_GuidesColorA = picked_color.value();
                guides_color_a_button->setStyleSheet(ColorToBackgroundStyle(m_Project.m_Data.m_GuidesColorA));
                GuidesColorChanged();
            }
        }
    };

    auto pick_color_b{
        [=, this]()
        {
            if (const auto picked_color{ pick_color(m_Project.m_Data.m_GuidesColorB) })
            {
                m_Project.m_Data.m_GuidesColorB = picked_color.value();
                guides_color_b_button->setStyleSheet(ColorToBackgroundStyle(m_Project.m_Data.m_GuidesColorB));
                GuidesColorChanged();
            }
        }
    };

    auto change_guides_thickness{
        [this, &project](Length v)
        {
            project.m_Data.m_GuidesThickness = v;
            GuidesThicknessChanged();
        }
    };

    QObject::connect(m_ExportExactGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_export_exact_guides);
    QObject::connect(m_EnableGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_enable_guides);
    QObject::connect(m_EnableBacksideGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_enable_backside_guides);
    QObject::connect(m_CornerGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_corner_guides);
    QObject::connect(m_CrossGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_cross_guides);
    QObject::connect(m_GuidesOffsetSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_guides_offset);
    QObject::connect(m_GuidesLengthSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_guides_length);
    QObject::connect(m_ExtendedGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     change_extended_guides);
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
                     this,
                     change_guides_thickness);
}

void GuidesOptionsWidget::NewProjectOpened()
{
    SetDefaults();
}

void GuidesOptionsWidget::CardSizeChanged()
{
    const auto card_size{ m_Project.CardSize() };

    m_GuidesLengthSpin->SetRange(0_mm, dla::math::min(card_size.x, card_size.y) / 2.0f);
    m_GuidesLengthSpin->SetValue(m_Project.CardCornerRadius() / 2.0f);
}

void GuidesOptionsWidget::BleedChanged()
{
    m_GuidesOffsetSpin->SetRange(0_mm, m_Project.m_Data.m_BleedEdge + m_Project.m_Data.m_EnvelopeBleedEdge);
}

void GuidesOptionsWidget::BacksideEnabledChanged()
{
    m_EnableBacksideGuidesCheckbox->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_EnableBacksideGuidesCheckbox->setVisible(m_Project.m_Data.m_BacksideEnabled);
}

void GuidesOptionsWidget::AdvancedModeChanged()
{
    SetAdvancedWidgetsVisibility();
}

void GuidesOptionsWidget::SetDefaults()
{
    m_ExportExactGuidesCheckbox->setChecked(m_Project.m_Data.m_ExportExactGuides);

    m_EnableGuidesCheckbox->setChecked(m_Project.m_Data.m_EnableGuides);

    m_EnableBacksideGuidesCheckbox->setChecked(m_Project.m_Data.m_BacksideEnableGuides);
    m_EnableBacksideGuidesCheckbox->setEnabled(m_Project.m_Data.m_EnableGuides && m_Project.m_Data.m_BacksideEnabled);
    m_EnableBacksideGuidesCheckbox->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_CornerGuidesCheckbox->setChecked(m_Project.m_Data.m_CornerGuides);
    m_CornerGuidesCheckbox->setEnabled(m_Project.m_Data.m_EnableGuides);

    m_CrossGuidesCheckbox->setChecked(m_Project.m_Data.m_CrossGuides);
    m_CrossGuidesCheckbox->setEnabled(m_Project.m_Data.m_CornerGuides && m_Project.m_Data.m_EnableGuides);

    m_ExtendedGuidesCheckbox->setChecked(m_Project.m_Data.m_ExtendedGuides);
    m_ExtendedGuidesCheckbox->setEnabled(m_Project.m_Data.m_EnableGuides);

    m_GuidesColorA->GetWidget()->setStyleSheet(ColorToBackgroundStyle(m_Project.m_Data.m_GuidesColorA));
    m_GuidesColorA->setEnabled(m_Project.m_Data.m_EnableGuides);

    m_GuidesColorB->GetWidget()->setStyleSheet(ColorToBackgroundStyle(m_Project.m_Data.m_GuidesColorB));
    m_GuidesColorB->setEnabled(m_Project.m_Data.m_EnableGuides);

    m_GuidesOffsetSpin->SetRange(0_mm, m_Project.m_Data.m_BleedEdge);
    m_GuidesOffsetSpin->SetValue(m_Project.m_Data.m_GuidesOffset);
    m_GuidesOffsetSpin->setEnabled(m_Project.m_Data.m_CornerGuides && m_Project.m_Data.m_EnableGuides);

    const auto card_size{ m_Project.CardSize() };
    m_GuidesLengthSpin->SetRange(0_mm, dla::math::min(card_size.x, card_size.y) / 2.0f);
    m_GuidesLengthSpin->SetValue(m_Project.m_Data.m_GuidesLength);
    m_GuidesLengthSpin->setEnabled(m_Project.m_Data.m_CornerGuides && m_Project.m_Data.m_EnableGuides);

    m_GuidesThicknessSpin->SetRange(0_mm, 5_mm);
    m_GuidesThicknessSpin->SetValue(m_Project.m_Data.m_GuidesThickness);
    m_GuidesThicknessSpin->setEnabled(m_Project.m_Data.m_EnableGuides);

    SetAdvancedWidgetsVisibility();
}

void GuidesOptionsWidget::SetAdvancedWidgetsVisibility()
{
    // Always enabled: m_EnableGuidesCheckbox, m_CornerGuidesCheckbox, m_ExtendedGuidesCheckbox, m_GuidesColorA, m_GuidesColorB
    m_ExportExactGuidesCheckbox->setVisible(g_Cfg.m_AdvancedMode);
    m_EnableBacksideGuidesCheckbox->setVisible(g_Cfg.m_AdvancedMode);
    m_CrossGuidesCheckbox->setVisible(g_Cfg.m_AdvancedMode);
    m_GuidesOffsetSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_GuidesLengthSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
    m_GuidesThicknessSpin->parentWidget()->setVisible(g_Cfg.m_AdvancedMode);
}

QString GuidesOptionsWidget::ColorToBackgroundStyle(ColorRGB8 color)
{
    return ToQString(fmt::format(":enabled {{ background-color:#{:0>6x}; }}", ColorToInt(color)));
}
