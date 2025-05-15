#include <ppp/ui/widget_card_options.hpp>

#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

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
    : m_Project{ project }
{
    setObjectName("Card Options");

    const auto base_unit_name{ ToQString(CFG.BaseUnit.m_ShortName) };

    auto* bleed_edge{ new DoubleSpinBoxWithLabel{ "&Bleed Edge" } };
    m_BleedEdgeSpin = bleed_edge->GetWidget();
    m_BleedEdgeSpin->setDecimals(2);
    m_BleedEdgeSpin->setSingleStep(0.1);
    m_BleedEdgeSpin->setSuffix(base_unit_name);

    auto* bleed_back_divider{ new QFrame };
    bleed_back_divider->setFrameShape(QFrame::Shape::HLine);
    bleed_back_divider->setFrameShadow(QFrame::Shadow::Sunken);

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
    layout->addWidget(bleed_back_divider);
    layout->addWidget(m_BacksideOffset);
    layout->addWidget(m_BacksideDefaultButton);
    layout->addWidget(m_BacksideDefaultPreview);
    layout->addWidget(m_BacksideOffset);

    layout->setAlignment(m_BacksideDefaultPreview, Qt::AlignmentFlag::AlignHCenter);
    setLayout(layout);

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
            BleedChanged();
        }
    };

    auto switch_backside_enabled{
        [=, &project](Qt::CheckState s)
        {
            project.Data.BacksideEnabled = s == Qt::CheckState::Checked;
            m_BacksideDefaultButton->setEnabled(project.Data.BacksideEnabled);
            m_BacksideDefaultButton->setVisible(project.Data.BacksideEnabled);
            m_BacksideDefaultPreview->setVisible(project.Data.BacksideEnabled);
            m_BacksideOffset->setEnabled(project.Data.BacksideEnabled);
            m_BacksideOffset->setVisible(project.Data.BacksideEnabled);
            BacksideEnabledChanged();
        }
    };

    auto pick_backside{
        [=, &project]()
        {
            if (const auto default_backside_choice{ OpenImageDialog(project.Data.ImageDir) })
            {
                project.Data.BacksideDefault = default_backside_choice.value();
                BacksideDefaultChanged();
            }
        }
    };

    auto change_backside_offset{
        [=, &project](double v)
        {
            const auto base_unit{ CFG.BaseUnit.m_Unit };
            project.Data.BacksideOffset = base_unit * static_cast<float>(v);
            BacksideOffsetChanged();
        }
    };

    QObject::connect(m_BleedEdgeSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_bleed_edge);
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

void CardOptionsWidget::SetDefaults()
{
    const auto base_unit{ CFG.BaseUnit.m_Unit };
    const auto full_bleed{ m_Project.CardFullBleed() };

    m_BleedEdgeSpin->setRange(0, full_bleed / base_unit);
    m_BleedEdgeSpin->setValue(m_Project.Data.BleedEdge / base_unit);

    m_BacksideCheckbox->setChecked(m_Project.Data.BacksideEnabled);

    m_BacksideDefaultButton->setEnabled(m_Project.Data.BacksideEnabled);
    m_BacksideDefaultButton->setVisible(m_Project.Data.BacksideEnabled);

    m_BacksideDefaultPreview->setVisible(m_Project.Data.BacksideEnabled);

    m_BacksideOffsetSpin->setRange(-0.3_in / base_unit, 0.3_in / base_unit);
    m_BacksideOffsetSpin->setValue(m_Project.Data.BacksideOffset / base_unit);

    m_BacksideOffset->setEnabled(m_Project.Data.BacksideEnabled);
    m_BacksideOffset->setVisible(m_Project.Data.BacksideEnabled);
}
