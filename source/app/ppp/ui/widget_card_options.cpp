#include <ppp/ui/widget_card_options.hpp>

#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <magic_enum/magic_enum.hpp>

#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/linked_spin_boxes.hpp>
#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_card.hpp>
#include <ppp/ui/widget_label.hpp>

class DefaultBacksidePreview : public QWidget
{
  public:
    DefaultBacksidePreview(const Project& project)
        : m_Project{ project }
    {
        const fs::path& backside_name{ project.m_Data.m_BacksideDefault };

        auto* backside_default_image{ new BacksideImage{ backside_name, c_MinimumWidth, project } };

        const auto backside_height{ backside_default_image->heightForWidth(c_MinimumWidth.value) };
        backside_default_image->setFixedWidth(c_MinimumWidth.value);
        backside_default_image->setFixedHeight(backside_height);

        auto* backside_default_label{ new QLabel{ ToQString(backside_name.c_str()) } };

        auto* layout{ new QVBoxLayout };
        layout->addWidget(backside_default_image);
        layout->addWidget(backside_default_label);
        layout->setAlignment(backside_default_image, Qt::AlignmentFlag::AlignHCenter);
        layout->setAlignment(backside_default_label, Qt::AlignmentFlag::AlignHCenter);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        setLayout(layout);

        setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);

        m_DefaultImage = backside_default_image;
        m_DefaultLabel = backside_default_label;
    }

    void Refresh()
    {
        const fs::path& backside_name{ m_Project.m_Data.m_BacksideDefault };
        m_DefaultImage->Refresh(backside_name, c_MinimumWidth, m_Project);
        m_DefaultLabel->setText(ToQString(backside_name.c_str()));
    }

  private:
    inline static constexpr auto c_MinimumWidth{ 60_pix };

    const Project& m_Project;

    BacksideImage* m_DefaultImage{ nullptr };
    QLabel* m_DefaultLabel{ nullptr };
};

CardOptionsWidget::CardOptionsWidget(Project& project)
    : m_Project{ project }
{
    setObjectName("Card Options");

    const auto base_unit_name{ ToQString(g_Cfg.m_BaseUnit.m_ShortName) };

    auto* bleed_edge{ new DoubleSpinBoxWithLabel{ "&Bleed Edge" } };
    m_BleedEdgeSpin = bleed_edge->GetWidget();
    m_BleedEdgeSpin->setDecimals(2);
    m_BleedEdgeSpin->setSingleStep(0.1);
    m_BleedEdgeSpin->setSuffix(base_unit_name);

    auto* spacing_spin_boxes{ new LinkedSpinBoxes{ project.m_Data.m_SpacingLinked } };

    m_HorizontalSpacingSpin = spacing_spin_boxes->First();
    m_HorizontalSpacingSpin->setDecimals(2);
    m_HorizontalSpacingSpin->setSingleStep(0.1);
    m_HorizontalSpacingSpin->setSuffix(base_unit_name);

    m_VerticalSpacingSpin = spacing_spin_boxes->Second();
    m_VerticalSpacingSpin->setDecimals(2);
    m_VerticalSpacingSpin->setSingleStep(0.1);
    m_VerticalSpacingSpin->setSuffix(base_unit_name);

    auto* spacing{ new WidgetWithLabel{ "Card Spacing", spacing_spin_boxes } };
    spacing->layout()->setAlignment(spacing->GetLabel(), Qt::AlignTop);

    auto* corners{ new ComboBoxWithLabel{ "Cor&ners",
                                          magic_enum::enum_names<CardCorners>(),
                                          magic_enum::enum_name(project.m_Data.m_Corners) } };
    corners->setEnabled(project.m_Data.m_BleedEdge == 0_mm);
    m_Corners = corners->GetWidget();
    m_Corners->setToolTip("Determines if corners in the rendered pdf are square or rounded, only available if bleed edge is zero.");

    m_BacksideCheckbox = new QCheckBox{ "Enable Backside" };

    m_BacksideDefaultButton = new QPushButton{ "Choose Default" };

    m_BacksideDefaultPreview = new DefaultBacksidePreview{ project };

    m_BacksideOffsetSpin = new QDoubleSpinBox;
    m_BacksideOffsetSpin->setDecimals(2);
    m_BacksideOffsetSpin->setSingleStep(0.1);
    m_BacksideOffsetSpin->setSuffix(base_unit_name);

    m_BacksideOffset = new WidgetWithLabel{ "Off&set", m_BacksideOffsetSpin };

    SetDefaults();

    auto* layout{ new QVBoxLayout };
    layout->addWidget(bleed_edge);
    layout->addWidget(spacing);
    layout->addWidget(corners);
    layout->addWidget(m_BacksideCheckbox);
    layout->addWidget(m_BacksideDefaultButton);
    layout->addWidget(m_BacksideDefaultPreview);
    layout->addWidget(m_BacksideOffset);

    layout->setAlignment(m_BacksideDefaultPreview, Qt::AlignmentFlag::AlignHCenter);
    setLayout(layout);

    auto change_bleed_edge{
        [this, &project, corners](double v)
        {
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            const auto new_bleed_edge{ base_unit * static_cast<float>(v) };
            if (dla::math::abs(project.m_Data.m_BleedEdge - new_bleed_edge) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_BleedEdge = new_bleed_edge;
            BleedChanged();

            const bool has_no_bleed_edge{ project.m_Data.m_BleedEdge == 0_mm };
            if (corners->isEnabled() != has_no_bleed_edge)
            {
                corners->setEnabled(has_no_bleed_edge);
                if (project.m_Data.m_Corners == CardCorners::Rounded)
                {
                    CornersChanged();
                }
            }
        }
    };

    auto link_spacing{
        [this]()
        {
            m_Project.m_Data.m_SpacingLinked = true;
        }
    };

    auto unlink_spacing{
        [this]()
        {
            m_Project.m_Data.m_SpacingLinked = false;
        }
    };

    auto change_horizontal_spacing{
        [this, &project](double v)
        {
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            const auto new_spacing{ base_unit * static_cast<float>(v) };
            if (dla::math::abs(project.m_Data.m_Spacing.x - new_spacing) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_Spacing.x = new_spacing;
            SpacingChanged();
        }
    };

    auto change_vertical_spacing{
        [this, &project](double v)
        {
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            const auto new_spacing{ base_unit * static_cast<float>(v) };
            if (dla::math::abs(project.m_Data.m_Spacing.y - new_spacing) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_Spacing.y = new_spacing;
            SpacingChanged();
        }
    };

    auto change_corners{
        [this](const QString& t)
        {
            const auto new_corners{ magic_enum::enum_cast<CardCorners>(t.toStdString())
                                        .value_or(CardCorners::Square) };
            if (new_corners != m_Project.m_Data.m_Corners)
            {
                m_Project.m_Data.m_Corners = new_corners;
                CornersChanged();
            }
        }
    };

    auto switch_backside_enabled{
        [this, &project](Qt::CheckState s)
        {
            project.m_Data.m_BacksideEnabled = s == Qt::CheckState::Checked;
            m_BacksideDefaultButton->setEnabled(project.m_Data.m_BacksideEnabled);
            m_BacksideDefaultButton->setVisible(project.m_Data.m_BacksideEnabled);
            m_BacksideDefaultPreview->setVisible(project.m_Data.m_BacksideEnabled);
            m_BacksideOffset->setEnabled(project.m_Data.m_BacksideEnabled);
            m_BacksideOffset->setVisible(project.m_Data.m_BacksideEnabled);
            BacksideEnabledChanged();
        }
    };

    auto pick_backside{
        [this, &project]()
        {
            if (const auto default_backside_choice{ OpenImageDialog(project.m_Data.m_ImageDir) })
            {
                project.m_Data.m_BacksideDefault = default_backside_choice.value();
                BacksideDefaultChanged();
            }
        }
    };

    auto change_backside_offset{
        [this, &project](double v)
        {
            const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
            project.m_Data.m_BacksideOffset = base_unit * static_cast<float>(v);
            BacksideOffsetChanged();
        }
    };

    QObject::connect(m_BleedEdgeSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_bleed_edge);
    QObject::connect(spacing_spin_boxes,
                     &LinkedSpinBoxes::Linked,
                     this,
                     link_spacing);
    QObject::connect(spacing_spin_boxes,
                     &LinkedSpinBoxes::UnLinked,
                     this,
                     unlink_spacing);
    QObject::connect(m_HorizontalSpacingSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_horizontal_spacing);
    QObject::connect(m_VerticalSpacingSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_vertical_spacing);
    QObject::connect(m_Corners,
                     &QComboBox::currentTextChanged,
                     this,
                     change_corners);
    QObject::connect(m_BacksideCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     switch_backside_enabled);
    QObject::connect(m_BacksideDefaultButton,
                     &QPushButton::clicked,
                     this,
                     pick_backside);
    QObject::connect(m_BacksideOffsetSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_backside_offset);
}

void CardOptionsWidget::NewProjectOpened()
{
    SetDefaults();
    ImageDirChanged();
}

void CardOptionsWidget::ImageDirChanged()
{
    m_BacksideDefaultPreview->Refresh();
}

void CardOptionsWidget::BaseUnitChanged()
{
    const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
    const auto base_unit_name{ g_Cfg.m_BaseUnit.m_ShortName };
    const auto full_bleed{ m_Project.CardFullBleed() };
    const auto backside_offset{ m_Project.m_Data.m_BacksideOffset };

    m_BleedEdgeSpin->setRange(0, full_bleed / base_unit);
    m_BleedEdgeSpin->setSuffix(ToQString(base_unit_name));
    m_BleedEdgeSpin->setValue(m_Project.m_Data.m_BleedEdge / base_unit);

    m_HorizontalSpacingSpin->setRange(0, 1_cm / base_unit);
    m_HorizontalSpacingSpin->setSuffix(ToQString(base_unit_name));
    m_HorizontalSpacingSpin->setValue(m_Project.m_Data.m_Spacing.x / base_unit);

    m_VerticalSpacingSpin->setRange(0, 1_cm / base_unit);
    m_VerticalSpacingSpin->setSuffix(ToQString(base_unit_name));
    m_VerticalSpacingSpin->setValue(m_Project.m_Data.m_Spacing.y / base_unit);

    m_BacksideOffsetSpin->setRange(-0.3_in / base_unit, 0.3_in / base_unit);
    m_BacksideOffsetSpin->setSuffix(ToQString(base_unit_name));
    m_BacksideOffsetSpin->setValue(backside_offset / base_unit);
}
#include <QMainWindow>
void CardOptionsWidget::SetDefaults()
{
    const auto base_unit{ g_Cfg.m_BaseUnit.m_Unit };
    const auto full_bleed{ m_Project.CardFullBleed() };

    auto set_visible_safe{
        [](QWidget* widget, bool visible)
        {
            const auto* window{ widget->window() };
            if ((window != nullptr && dynamic_cast<const QMainWindow*>(window)) || !visible)
            {
                widget->setVisible(visible);
            }
        }
    };

    m_BleedEdgeSpin->setRange(0, full_bleed / base_unit);
    m_BleedEdgeSpin->setValue(m_Project.m_Data.m_BleedEdge / base_unit);

    m_HorizontalSpacingSpin->setRange(0, 1_cm / base_unit);
    m_HorizontalSpacingSpin->setValue(m_Project.m_Data.m_Spacing.x / base_unit);

    m_VerticalSpacingSpin->setRange(0, 1_cm / base_unit);
    m_VerticalSpacingSpin->setValue(m_Project.m_Data.m_Spacing.y / base_unit);

    m_Corners->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_Corners)));

    m_BacksideCheckbox->setChecked(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideDefaultButton->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    set_visible_safe(m_BacksideDefaultButton, m_Project.m_Data.m_BacksideEnabled);

    set_visible_safe(m_BacksideDefaultPreview, m_Project.m_Data.m_BacksideEnabled);

    m_BacksideOffsetSpin->setRange(-0.3_in / base_unit, 0.3_in / base_unit);
    m_BacksideOffsetSpin->setValue(m_Project.m_Data.m_BacksideOffset / base_unit);

    m_BacksideOffset->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    set_visible_safe(m_BacksideOffset, m_Project.m_Data.m_BacksideEnabled);
}
