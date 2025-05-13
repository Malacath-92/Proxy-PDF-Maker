#include <ppp/ui/widget_card_options.hpp>

#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/main_window.hpp>
#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_card.hpp>
#include <ppp/ui/widget_label.hpp>

class DefaultBacksidePreview : public QWidget
{
  public:
    DefaultBacksidePreview(const Project& project)
        : m_Project{ project }
    {
        const fs::path& backside_name{ project.Data.BacksideDefault };

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
        const fs::path& backside_name{ m_Project.Data.BacksideDefault };
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
    : QDockWidget{ "Card Options" }
    , m_Project{ project }
{
    setFeatures(DockWidgetMovable | DockWidgetFloatable);
    setAllowedAreas(Qt::AllDockWidgetAreas);

    const auto initial_base_unit{ CFG.BaseUnit.m_Unit };
    const auto initial_base_unit_name{ ToQString(CFG.BaseUnit.m_ShortName) };

    const auto full_bleed{ project.CardFullBleed() };
    auto* bleed_edge{ new DoubleSpinBoxWithLabel{ "&Bleed Edge" } };
    auto* bleed_edge_spin{ bleed_edge->GetWidget() };
    bleed_edge_spin->setDecimals(2);
    bleed_edge_spin->setRange(0, full_bleed / initial_base_unit);
    bleed_edge_spin->setSingleStep(0.1);
    bleed_edge_spin->setSuffix(initial_base_unit_name);
    bleed_edge_spin->setValue(project.Data.BleedEdge / initial_base_unit);

    auto* corner_weight_slider{ new QSlider{ Qt::Horizontal } };
    corner_weight_slider->setTickPosition(QSlider::NoTicks);
    corner_weight_slider->setRange(0, 1000);
    corner_weight_slider->setValue(static_cast<int>(project.Data.CornerWeight * 1000.0f));
    auto* corner_weight{ new WidgetWithLabel{ "&Corner Weight", corner_weight_slider } };

    auto* bleed_back_divider{ new QFrame };
    bleed_back_divider->setFrameShape(QFrame::Shape::HLine);
    bleed_back_divider->setFrameShadow(QFrame::Shadow::Sunken);

    auto* backside_checkbox{ new QCheckBox{ "Enable Backside" } };
    backside_checkbox->setChecked(project.Data.BacksideEnabled);

    auto* backside_default_button{ new QPushButton{ "Choose Default" } };
    backside_default_button->setEnabled(project.Data.BacksideEnabled);
    backside_default_button->setVisible(project.Data.BacksideEnabled);

    auto* backside_default_preview{ new DefaultBacksidePreview{ project } };
    backside_default_preview->setVisible(project.Data.BacksideEnabled);

    auto* backside_offset_spin{ new QDoubleSpinBox };
    backside_offset_spin->setDecimals(2);
    backside_offset_spin->setRange(-0.3_in / initial_base_unit, 0.3_in / initial_base_unit);
    backside_offset_spin->setSingleStep(0.1);
    backside_offset_spin->setSuffix(initial_base_unit_name);
    backside_offset_spin->setValue(project.Data.BacksideOffset / initial_base_unit);
    auto* backside_offset{ new WidgetWithLabel{ "Off&set", backside_offset_spin } };
    backside_offset->setEnabled(project.Data.BacksideEnabled);
    backside_offset->setVisible(project.Data.BacksideEnabled);

    auto* layout{ new QVBoxLayout };
    layout->addWidget(bleed_edge);
    layout->addWidget(corner_weight);
    layout->addWidget(bleed_back_divider);
    layout->addWidget(backside_checkbox);
    layout->addWidget(backside_default_button);
    layout->addWidget(backside_default_preview);
    layout->addWidget(backside_offset);

    layout->setAlignment(backside_default_preview, Qt::AlignmentFlag::AlignHCenter);

    auto* main_widget{ new QWidget };
    main_widget->setLayout(layout);
    main_widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setWidget(main_widget);

    auto main_window{
        [this]()
        { return static_cast<PrintProxyPrepMainWindow*>(window()); }
    };

    auto change_bleed_edge{
        [=, &project](double v)
        {
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            const auto new_bleed_edge{ base_unit * static_cast<float>(v) };
            if (dla::math::abs(project.Data.BleedEdge - new_bleed_edge) < 0.001_mm)
            {
                return;
            }

            project.Data.BleedEdge = new_bleed_edge;
            main_window()->BleedChangedDiff(project.Data.BleedEdge);
            main_window()->BleedChanged(project);
        }
    };

    auto change_corner_weight{
        [=, &project](int v)
        {
            project.Data.CornerWeight = static_cast<float>(v) / 1000.0f;
            main_window()->CornerWeightChanged(project);
        }
    };

    auto switch_backside_enabled{
        [=, &project](Qt::CheckState s)
        {
            project.Data.BacksideEnabled = s == Qt::CheckState::Checked;
            backside_default_button->setEnabled(project.Data.BacksideEnabled);
            backside_default_button->setVisible(project.Data.BacksideEnabled);
            backside_default_preview->setVisible(project.Data.BacksideEnabled);
            backside_offset->setEnabled(project.Data.BacksideEnabled);
            backside_offset->setVisible(project.Data.BacksideEnabled);
            main_window()->BacksideEnabledChanged(project);
        }
    };

    auto pick_backside{
        [=, &project]()
        {
            if (const auto default_backside_choice{ OpenImageDialog(project.Data.ImageDir) })
            {
                project.Data.BacksideDefault = default_backside_choice.value();
                main_window()->BacksideDefaultChanged(project);
            }
        }
    };

    auto change_backside_offset{
        [=, &project](double v)
        {
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            project.Data.BacksideOffset = base_unit * static_cast<float>(v);
            main_window()->BacksideOffsetChanged(project);
        }
    };

    QObject::connect(bleed_edge_spin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_bleed_edge);
    QObject::connect(corner_weight_slider,
                     &QSlider::valueChanged,
                     this,
                     change_corner_weight);
    QObject::connect(backside_checkbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     switch_backside_enabled);
    QObject::connect(backside_default_button,
                     &QPushButton::clicked,
                     this,
                     pick_backside);
    QObject::connect(backside_offset_spin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_backside_offset);

    m_BleedEdgeSpin = bleed_edge_spin;
    m_CornerWeightSlider = corner_weight_slider;
    m_BacksideCheckbox = backside_checkbox;
    m_BacksideOffsetSpin = backside_offset_spin;
    m_BacksideDefaultPreview = backside_default_preview;
}

void CardOptionsWidget::NewProjectOpened()
{
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto full_bleed{ m_Project.CardFullBleed() };
    const auto full_bleed_rounded{ QString::number(full_bleed / base_unit, 'f', 2).toFloat() };
    if (static_cast<int32_t>(m_BleedEdgeSpin->maximum() / 0.001) != static_cast<int32_t>(full_bleed_rounded / 0.001))
    {
        m_BleedEdgeSpin->setRange(0, full_bleed / base_unit);
        m_BleedEdgeSpin->setValue(0);
    }
    else
    {
        m_BleedEdgeSpin->setValue(m_Project.Data.BleedEdge / base_unit);
    }

    m_CornerWeightSlider->setValue(m_Project.Data.CornerWeight);

    m_BacksideCheckbox->setChecked(m_Project.Data.BacksideEnabled);
    m_BacksideOffsetSpin->setValue(m_Project.Data.BacksideOffset.value);

    ImageDirChanged();
}

void CardOptionsWidget::ImageDirChanged()
{
    m_BacksideDefaultPreview->Refresh();
}

void CardOptionsWidget::BaseUnitChanged()
{
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto base_unit_name{ CFG.BaseUnit.m_ShortName };
    const auto full_bleed{ m_Project.CardFullBleed() };
    const auto bleed{ m_Project.Data.BleedEdge };
    const auto backside_offset{ m_Project.Data.BacksideOffset };

    m_BleedEdgeSpin->setRange(0, full_bleed / base_unit);
    m_BleedEdgeSpin->setSuffix(ToQString(base_unit_name));
    m_BleedEdgeSpin->setValue(bleed / base_unit);

    m_BacksideOffsetSpin->setRange(-0.3_in / base_unit, 0.3_in / base_unit);
    m_BacksideOffsetSpin->setSuffix(ToQString(base_unit_name));
    m_BacksideOffsetSpin->setValue(backside_offset / base_unit);
}
