#include <ppp/ui/widget_guides_options.hpp>

#include <QCheckBox>
#include <QColorDialog>
#include <QPushButton>
#include <QVBoxLayout>

#include <ppp/color.hpp>
#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/widget_label.hpp>

GuidesOptionsWidget::GuidesOptionsWidget(Project& project)
    : m_Project{ project }
{
    setObjectName("Guides Options");

    m_ExportExactGuidesCheckbox = new QCheckBox{ "Export Exact Guides" };
    m_ExportExactGuidesCheckbox->setToolTip("Decides whether a .svg file will be generated that contains the exact guides for the current layout");

    m_EnableGuidesCheckbox = new QCheckBox{ "Enable Guides" };
    m_EnableGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered on the output");

    m_BacksideGuidesCheckbox = new QCheckBox{ "Enable Backside Guides" };
    m_BacksideGuidesCheckbox->setToolTip("Decides whether cutting guides are rendered on backside pages");

    m_ExtendedGuidesCheckbox = new QCheckBox{ "Extended Guides" };
    m_ExtendedGuidesCheckbox->setToolTip("Decides whether cutting extend to the edge of the page");

    auto* guides_color_a_button{ new QPushButton };
    m_GuidesColorA = new WidgetWithLabel{ "Guides Color A", guides_color_a_button };

    auto* guides_color_b_button{ new QPushButton };
    m_GuidesColorB = new WidgetWithLabel{ "Guides Color B", guides_color_b_button };
    
    auto* guides_offset{ new DoubleSpinBoxWithLabel{ "Guides O&ffset" } };
    m_GuidesOffsetSpin = guides_offset->GetWidget();
    m_GuidesOffsetSpin->setDecimals(3);
    m_GuidesOffsetSpin->setSingleStep(0.1);
    m_GuidesOffsetSpin->setSuffix(ToQString(CFG.BaseUnit.m_ShortName));
    m_GuidesOffsetSpin->setToolTip("Decides where to place the guides, at 0 the guides' center will align with the card corner");

    SetDefaults();

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_ExportExactGuidesCheckbox);
    layout->addWidget(m_EnableGuidesCheckbox);
    layout->addWidget(m_BacksideGuidesCheckbox);
    layout->addWidget(m_ExtendedGuidesCheckbox);
    layout->addWidget(m_GuidesColorA);
    layout->addWidget(m_GuidesColorB);
    layout->addWidget(guides_offset);
    setLayout(layout);

    auto change_export_exact_guides{
        [=, this](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            m_Project.Data.ExportExactGuides = enabled;

            ExactGuidesEnabledChanged();
        }
    };

    auto change_enable_guides{
        [=, this](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            m_Project.Data.EnableGuides = enabled;

            m_ExtendedGuidesCheckbox->setEnabled(enabled);
            m_GuidesColorA->setEnabled(enabled);
            m_GuidesColorB->setEnabled(enabled);

            GuidesEnabledChanged();
        }
    };

    auto switch_backside_guides_enabled{
        [=, &project](Qt::CheckState s)
        {
            project.Data.BacksideEnableGuides = s == Qt::CheckState::Checked;
            BacksideGuidesEnabledChanged();
        }
    };

    auto change_extended_guides{
        [=, this](Qt::CheckState s)
        {
            m_Project.Data.ExtendedGuides = s == Qt::CheckState::Checked;
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
            if (const auto picked_color{ pick_color(m_Project.Data.GuidesColorA) })
            {
                m_Project.Data.GuidesColorA = picked_color.value();
                guides_color_a_button->setStyleSheet(ColorToBackgroundStyle(m_Project.Data.GuidesColorA));
                GuidesColorChanged();
            }
        }
    };

    auto pick_color_b{
        [=, this]()
        {
            if (const auto picked_color{ pick_color(m_Project.Data.GuidesColorB) })
            {
                m_Project.Data.GuidesColorB = picked_color.value();
                guides_color_b_button->setStyleSheet(ColorToBackgroundStyle(m_Project.Data.GuidesColorB));
                GuidesColorChanged();
            }
        }
    };

    auto change_guides_offset{
        [=, &project](double v)
        {
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            const auto new_guides_offset{ base_unit * static_cast<float>(v) };
            if (dla::math::abs(project.Data.GuidesOffset - new_guides_offset) < 0.001_mm)
            {
                return;
            }

            project.Data.GuidesOffset = new_guides_offset;
            GuidesOffsetChanged();
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
    QObject::connect(m_BacksideGuidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     switch_backside_guides_enabled);
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
    QObject::connect(m_GuidesOffsetSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_guides_offset);
}

void GuidesOptionsWidget::NewProjectOpened()
{
    SetDefaults();
}

void GuidesOptionsWidget::BleedChanged()
{
    m_GuidesOffsetSpin->setRange(0, m_Project.Data.BleedEdge / CFG.BaseUnit.m_Unit);
}

void GuidesOptionsWidget::BacksideEnabledChanged()
{
    m_BacksideGuidesCheckbox->setEnabled(m_Project.Data.BacksideEnabled);
    m_BacksideGuidesCheckbox->setVisible(m_Project.Data.BacksideEnabled);
}

void GuidesOptionsWidget::BaseUnitChanged()
{
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto base_unit_name{ ToQString(CFG.BaseUnit.m_ShortName) };
    
    m_GuidesOffsetSpin->setSuffix(ToQString(CFG.BaseUnit.m_ShortName));
    m_GuidesOffsetSpin->setRange(0, m_Project.Data.BleedEdge / base_unit);
    m_GuidesOffsetSpin->setValue(m_Project.Data.GuidesOffset / base_unit);
}


void GuidesOptionsWidget::SetDefaults()
{
    m_ExportExactGuidesCheckbox->setChecked(m_Project.Data.ExportExactGuides);

    m_EnableGuidesCheckbox->setChecked(m_Project.Data.EnableGuides);

    m_BacksideGuidesCheckbox->setChecked(m_Project.Data.BacksideEnableGuides);
    m_BacksideGuidesCheckbox->setEnabled(m_Project.Data.BacksideEnabled);
    m_BacksideGuidesCheckbox->setVisible(m_Project.Data.BacksideEnabled);

    m_ExtendedGuidesCheckbox->setChecked(m_Project.Data.ExtendedGuides);
    m_ExtendedGuidesCheckbox->setEnabled(m_Project.Data.EnableGuides);

    m_GuidesColorA->GetWidget()->setStyleSheet(ColorToBackgroundStyle(m_Project.Data.GuidesColorA));
    m_GuidesColorA->setEnabled(m_Project.Data.EnableGuides);

    m_GuidesColorB->GetWidget()->setStyleSheet(ColorToBackgroundStyle(m_Project.Data.GuidesColorB));
    m_GuidesColorB->setEnabled(m_Project.Data.EnableGuides);

    const auto base_unit{ CFG.BaseUnit.m_Unit };
    m_GuidesOffsetSpin->setRange(0, m_Project.Data.BleedEdge / base_unit);
    m_GuidesOffsetSpin->setValue(m_Project.Data.GuidesOffset / base_unit);
}

QString GuidesOptionsWidget::ColorToBackgroundStyle(ColorRGB8 color)
{
    return ToQString(fmt::format(":enabled {{ background-color:#{:0>6x}; }}", ColorToInt(color)));
}
