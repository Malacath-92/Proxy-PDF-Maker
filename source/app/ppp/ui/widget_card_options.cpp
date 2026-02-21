#include <ppp/ui/widget_card_options.hpp>

#include <QCheckBox>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <magic_enum/magic_enum.hpp>

#include <nlohmann/json.hpp>

#include <ppp/config.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/default_project_value_actions.hpp>
#include <ppp/ui/linked_spin_boxes.hpp>
#include <ppp/ui/widget_card.hpp>
#include <ppp/ui/widget_double_spin_box.hpp>
#include <ppp/ui/widget_label.hpp>

#include <ppp/ui/popups.hpp>
#include <ppp/ui/popups/image_browse_popup.hpp>

#include <ppp/profile/profile.hpp>

class DefaultBacksidePreview : public QWidget
{
  public:
    DefaultBacksidePreview(const Project& project)
        : m_Project{ project }
    {
        TRACY_AUTO_SCOPE();

        const fs::path& backside_name{ project.m_Data.m_BacksideDefault };

        auto* backside_default_image{ new BacksideImage{ backside_name, c_MinimumWidth, project } };

        const auto backside_height{ backside_default_image->heightForWidth(c_MinimumWidth.value) };
        backside_default_image->setFixedWidth(c_MinimumWidth.value);
        backside_default_image->setFixedHeight(backside_height);

        auto* backside_default_label{ new QLabel{ ClampName(ToQString(backside_name.c_str())) } };

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
        TRACY_AUTO_SCOPE();

        const fs::path& backside_name{ m_Project.m_Data.m_BacksideDefault };
        m_DefaultImage->Refresh(backside_name, c_MinimumWidth, m_Project);
        m_DefaultLabel->setText(ClampName(ToQString(backside_name.c_str())));
    }

  private:
    static QString ClampName(const QString& name)
    {
        static constexpr auto c_MaxNameLength{ 20 };
        static constexpr auto c_EllipsisSize{ 3 };
        return name.size() > c_MaxNameLength + c_EllipsisSize
                   ? name.left(c_MaxNameLength / 2) + "..." + name.right(c_MaxNameLength / 2)
                   : name;
    }

    inline static constexpr auto c_MinimumWidth{ 60_pix };

    const Project& m_Project;

    BacksideImage* m_DefaultImage{ nullptr };
    QLabel* m_DefaultLabel{ nullptr };
};

CardOptionsWidget::CardOptionsWidget(Project& project)
    : m_Project{ project }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Card Options");

    auto* bleed_edge{ new LengthSpinBoxWithLabel{ "&Bleed Edge" } };
    m_BleedEdgeSpin = bleed_edge->GetWidget();
    m_BleedEdgeSpin->ConnectUnitSignals(this);
    m_BleedEdgeSpin->setDecimals(2);
    m_BleedEdgeSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_BleedEdgeSpin, "bleed_edge_cm");

    auto* envelope{ new LengthSpinBoxWithLabel{ "En&velope" } };
    m_EnvelopeSpin = envelope->GetWidget();
    m_EnvelopeSpin->ConnectUnitSignals(this);
    m_EnvelopeSpin->setDecimals(2);
    m_EnvelopeSpin->setSingleStep(0.1);
    m_EnvelopeSpin->setToolTip("Similar to bleed edge, but doesn't increase space between cards.");
    EnableOptionWidgetForDefaults(m_EnvelopeSpin, "envelope_bleed_edge_cm");

    auto* spacing_spin_boxes{ new LinkedSpinBoxes{ project.m_Data.m_SpacingLinked } };

    m_HorizontalSpacingSpin = spacing_spin_boxes->First();
    m_HorizontalSpacingSpin->ConnectUnitSignals(this);
    m_HorizontalSpacingSpin->setDecimals(2);
    m_HorizontalSpacingSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_HorizontalSpacingSpin, "spacing.horizontal");

    m_VerticalSpacingSpin = spacing_spin_boxes->Second();
    m_VerticalSpacingSpin->ConnectUnitSignals(this);
    m_VerticalSpacingSpin->setDecimals(2);
    m_VerticalSpacingSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_VerticalSpacingSpin, "spacing.vertical");

    auto* spacing{ new WidgetWithLabel{ "Card Spacing", spacing_spin_boxes } };
    spacing->layout()->setAlignment(spacing->GetLabel(), Qt::AlignTop);

    auto* corners{ new ComboBoxWithLabel{ "Cor&ners",
                                          magic_enum::enum_names<CardCorners>(),
                                          magic_enum::enum_name(project.m_Data.m_Corners) } };
    corners->setEnabled(project.m_Data.m_BleedEdge == 0_mm);
    m_Corners = corners->GetWidget();
    m_Corners->setToolTip("Determines if corners in the rendered pdf are square or rounded, only available if bleed edge is zero.");
    EnableOptionWidgetForDefaults(m_Corners, "corners");

    m_BacksideCheckbox = new QCheckBox{ "Enable Backside" };
    EnableOptionWidgetForDefaults(m_BacksideCheckbox, "backside_enabled");

    m_SeparateBacksidesCheckbox = new QCheckBox{ "Separate Backsides-PDF" };
    m_SeparateBacksidesCheckbox->setToolTip("Generate two PDFs, one from the frontsides and one for the backsides.");
    EnableOptionWidgetForDefaults(m_SeparateBacksidesCheckbox, "separate_backsides");

    m_BacksideDefaultButton = new QPushButton{ "Choose Default" };

    m_BacksideDefaultPreview = new DefaultBacksidePreview{ project };
    EnableOptionWidgetForDefaults(
        m_BacksideDefaultPreview,
        "backside_default",
        [this, &project](nlohmann::json default_value)
        {
            const auto& default_backside{ default_value.get_ref<const std::string&>() };
            project.m_Data.m_BacksideDefault = fs::path{ default_backside };
            m_BacksideDefaultPreview->Refresh();
            BacksideDefaultChanged();
        },
        [&project]()
        {
            return project.m_Data.m_BacksideDefault;
        });

    {
        m_BacksideOffsetHorizontalSpin = MakeLengthSpinBox();
        m_BacksideOffsetHorizontalSpin->ConnectUnitSignals(this);
        m_BacksideOffsetHorizontalSpin->setDecimals(2);
        m_BacksideOffsetHorizontalSpin->setSingleStep(0.1);
        EnableOptionWidgetForDefaults(m_BacksideOffsetHorizontalSpin, "backside_offset.horizontal");

        m_BacksideOffsetVerticalSpin = MakeLengthSpinBox();
        m_BacksideOffsetVerticalSpin->ConnectUnitSignals(this);
        m_BacksideOffsetVerticalSpin->setDecimals(2);
        m_BacksideOffsetVerticalSpin->setSingleStep(0.1);
        EnableOptionWidgetForDefaults(m_BacksideOffsetVerticalSpin, "backside_offset.vertical");

        auto* inner_layout{ new QVBoxLayout };
        inner_layout->addWidget(m_BacksideOffsetHorizontalSpin);
        inner_layout->addWidget(m_BacksideOffsetVerticalSpin);
        inner_layout->setContentsMargins(0, 0, 0, 0);

        auto* inner_widget{ new QWidget };
        inner_widget->setLayout(inner_layout);

        auto* backside_offset_widget{ new WidgetWithLabel{ "Backside Off&set", inner_widget } };
        backside_offset_widget->layout()->setAlignment(backside_offset_widget->GetLabel(), Qt::AlignTop);
        m_BacksideOffset = backside_offset_widget;
    }

    auto* backside_rotation{ new DoubleSpinBoxWithLabel{ "Backside Rotation" } };
    m_BacksideRotationSpin = backside_rotation->GetWidget();
    m_BacksideRotationSpin->setDecimals(2);
    m_BacksideRotationSpin->setSingleStep(0.1);
    m_BacksideRotationSpin->setRange(-10, 10);
    m_BacksideRotationSpin->setSuffix("deg");
    EnableOptionWidgetForDefaults(m_BacksideRotationSpin, "backside_rotation");

    m_BacksideRotation = backside_rotation;

    m_BacksideAutoPattern = new QLineEdit{ ToQString(project.m_Data.m_BacksideAutoPattern) };
    m_BacksideAuto = new WidgetWithLabel{ "Auto-&Pattern", m_BacksideAutoPattern };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(bleed_edge);
    layout->addWidget(envelope);
    layout->addWidget(spacing);
    layout->addWidget(corners);
    layout->addWidget(m_BacksideCheckbox);
    layout->addWidget(m_SeparateBacksidesCheckbox);
    layout->addWidget(m_BacksideDefaultButton);
    layout->addWidget(m_BacksideDefaultPreview);
    layout->addWidget(m_BacksideOffset);
    layout->addWidget(m_BacksideRotation);
    layout->addWidget(m_BacksideAuto);

    layout->setAlignment(m_BacksideDefaultPreview, Qt::AlignmentFlag::AlignHCenter);
    setLayout(layout);

    SetDefaults();

    auto change_bleed_edge{
        [this, &project, corners](Length v)
        {
            if (dla::math::abs(project.m_Data.m_BleedEdge - v) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_BleedEdge = v;
            BleedChanged();

            const auto total_bleed{ project.m_Data.m_BleedEdge +
                                    project.m_Data.m_EnvelopeBleedEdge };
            const bool has_no_bleed_edge{ total_bleed == 0_mm };
            if (corners->isEnabled() != has_no_bleed_edge)
            {
                corners->setEnabled(has_no_bleed_edge);
                if (project.m_Data.m_Corners == CardCorners::Rounded)
                {
                    CornersChanged();
                }
            }

            const auto full_bleed{ m_Project.CardFullBleed() };
            m_EnvelopeSpin->SetRange(0_mm, full_bleed - v);
        }
    };

    auto change_envelope{
        [this, &project, corners](Length v)
        {
            if (dla::math::abs(project.m_Data.m_EnvelopeBleedEdge - v) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_EnvelopeBleedEdge = v;
            EnvelopeBleedChanged();

            const auto total_bleed{ project.m_Data.m_BleedEdge +
                                    project.m_Data.m_EnvelopeBleedEdge };
            const bool has_no_bleed_edge{ total_bleed == 0_mm };
            if (corners->isEnabled() != has_no_bleed_edge)
            {
                corners->setEnabled(has_no_bleed_edge);
                if (project.m_Data.m_Corners == CardCorners::Rounded)
                {
                    CornersChanged();
                }
            }

            const auto full_bleed{ m_Project.CardFullBleed() };
            m_BleedEdgeSpin->SetRange(0_mm, full_bleed - v);
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
        [this, &project](Length v)
        {
            if (dla::math::abs(project.m_Data.m_Spacing.x - v) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_Spacing.x = v;
            SpacingChanged();
        }
    };

    auto change_vertical_spacing{
        [this, &project](Length v)
        {
            if (dla::math::abs(project.m_Data.m_Spacing.y - v) < 0.001_mm)
            {
                return;
            }

            project.m_Data.m_Spacing.y = v;
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
            const bool enabled{ s == Qt::CheckState::Checked };
            if (project.SetBacksideEnabled(enabled))
            {
                m_SeparateBacksidesCheckbox->setEnabled(enabled);
                m_SeparateBacksidesCheckbox->setVisible(enabled);
                m_BacksideDefaultButton->setEnabled(enabled);
                m_BacksideDefaultButton->setVisible(enabled);
                m_BacksideDefaultPreview->setVisible(enabled);
                m_BacksideOffset->setEnabled(enabled);
                m_BacksideOffset->setVisible(enabled);
                m_BacksideRotation->setEnabled(enabled);
                m_BacksideRotation->setVisible(enabled);
                m_BacksideAuto->setEnabled(enabled);
                m_BacksideAuto->setVisible(enabled);
                BacksideEnabledChanged();
            }
        }
    };

    auto switch_separate_backsides_enabled{
        [this, &project](Qt::CheckState s)
        {
            const bool enabled{ s == Qt::CheckState::Checked };
            project.m_Data.m_SeparateBacksides = enabled;
            SeparateBacksidesEnabledChanged();
        }
    };

    auto pick_backside{
        [this, &project]()
        {
            ImageBrowsePopup image_browser{ window(), project };
            image_browser.setWindowTitle("Choose default backside");
            if (const auto default_backside_choice{ image_browser.Show() })
            {
                project.m_Data.m_BacksideDefault = default_backside_choice.value();
                m_BacksideDefaultPreview->Refresh();
                BacksideDefaultChanged();
            }
        }
    };

    auto change_backside_offset_width{
        [this, &project](Length v)
        {
            project.m_Data.m_BacksideOffset.x = v;
            BacksideOffsetChanged();
        }
    };

    auto change_backside_offset_height{
        [this, &project](Length v)
        {
            project.m_Data.m_BacksideOffset.y = v;
            BacksideOffsetChanged();
        }
    };

    auto change_backside_rotation{
        [this, &project](float v)
        {
            project.m_Data.m_BacksideRotation = v * 1_deg;
            BacksideRotationChanged();
        }
    };

    auto change_backside_auto_pattern{
        [this, &project](const QString& pattern)
        {
            static constexpr const char c_WarningStyle[]{
                "QLineEdit{"
                "border-style: solid;"
                "border-width: 2px;"
                "border-color: red"
                "}"
            };
            if (pattern.count('$') != 1)
            {
                m_BacksideAutoPattern->setToolTip("Pattern must include exactly one $");
                m_BacksideAutoPattern->setStyleSheet(c_WarningStyle);
                return;
            }

            if (pattern == '$')
            {
                m_BacksideAutoPattern->setToolTip("Pattern can't be only $");
                m_BacksideAutoPattern->setStyleSheet(c_WarningStyle);
                return;
            }

            SetBacksideAutoPatternTooltip();
            m_BacksideAutoPattern->setStyleSheet("");

            if (project.SetBacksideAutoPattern(pattern.toStdString()))
            {
                CardBacksideChanged();
            }
        }
    };

    QObject::connect(m_BleedEdgeSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_bleed_edge);
    QObject::connect(m_EnvelopeSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_envelope);
    QObject::connect(spacing_spin_boxes,
                     &LinkedSpinBoxes::Linked,
                     this,
                     link_spacing);
    QObject::connect(spacing_spin_boxes,
                     &LinkedSpinBoxes::UnLinked,
                     this,
                     unlink_spacing);
    QObject::connect(m_HorizontalSpacingSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_horizontal_spacing);
    QObject::connect(m_VerticalSpacingSpin,
                     &LengthSpinBox::ValueChanged,
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
    QObject::connect(m_SeparateBacksidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     this,
                     switch_separate_backsides_enabled);
    QObject::connect(m_BacksideDefaultButton,
                     &QPushButton::clicked,
                     this,
                     pick_backside);
    QObject::connect(m_BacksideOffsetHorizontalSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_backside_offset_width);
    QObject::connect(m_BacksideOffsetVerticalSpin,
                     &LengthSpinBox::ValueChanged,
                     this,
                     change_backside_offset_height);
    QObject::connect(m_BacksideRotationSpin,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_backside_rotation);
    QObject::connect(m_BacksideAutoPattern,
                     &QLineEdit::textChanged,
                     this,
                     change_backside_auto_pattern);
}

void CardOptionsWidget::NewProjectOpened()
{
    TRACY_AUTO_SCOPE();

    SetDefaults();
    ImageDirChanged();
}

void CardOptionsWidget::ImageDirChanged()
{
    m_BacksideDefaultPreview->Refresh();
}

void CardOptionsWidget::AdvancedModeChanged()
{
    SetAdvancedWidgetsVisibility();
}

void CardOptionsWidget::BacksideEnabledChangedExternal()
{
    TRACY_AUTO_SCOPE();

    m_BacksideCheckbox->setChecked(m_Project.m_Data.m_BacksideEnabled);

    m_SeparateBacksidesCheckbox->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_SeparateBacksidesCheckbox->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideDefaultButton->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideDefaultButton->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideDefaultPreview->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideOffsetHorizontalSpin->SetRange(-0.3_in, 0.3_in);
    m_BacksideOffsetHorizontalSpin->SetValue(m_Project.m_Data.m_BacksideOffset.x);
    m_BacksideOffsetVerticalSpin->SetRange(-0.3_in, 0.3_in);
    m_BacksideOffsetVerticalSpin->SetValue(m_Project.m_Data.m_BacksideOffset.y);

    m_BacksideOffset->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideOffset->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideAutoPattern->setText(ToQString(m_Project.m_Data.m_BacksideAutoPattern));
    SetBacksideAutoPatternTooltip();

    m_BacksideAuto->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideAuto->setVisible(m_Project.m_Data.m_BacksideEnabled);
}

void CardOptionsWidget::BacksideAutoPatternChangedExternal(const std::string& pattern)
{
    TRACY_AUTO_SCOPE();

    m_BacksideAutoPattern->setText(ToQString(pattern));
    SetBacksideAutoPatternTooltip();
}

void CardOptionsWidget::SetDefaults()
{
    TRACY_AUTO_SCOPE();

    const auto full_bleed{ m_Project.CardFullBleed() };

    m_BleedEdgeSpin->SetRange(0_mm, full_bleed - m_Project.m_Data.m_EnvelopeBleedEdge);
    m_BleedEdgeSpin->SetValue(m_Project.m_Data.m_BleedEdge);

    m_EnvelopeSpin->SetRange(0_mm, full_bleed - m_Project.m_Data.m_BleedEdge);
    m_EnvelopeSpin->SetValue(m_Project.m_Data.m_EnvelopeBleedEdge);

    m_HorizontalSpacingSpin->SetRange(0_mm, 1_cm);
    m_HorizontalSpacingSpin->SetValue(m_Project.m_Data.m_Spacing.x);

    m_VerticalSpacingSpin->SetRange(0_mm, 1_cm);
    m_VerticalSpacingSpin->SetValue(m_Project.m_Data.m_Spacing.y);

    m_Corners->setCurrentText(ToQString(magic_enum::enum_name(m_Project.m_Data.m_Corners)));

    m_BacksideCheckbox->setChecked(m_Project.m_Data.m_BacksideEnabled);

    m_SeparateBacksidesCheckbox->setChecked(m_Project.m_Data.m_SeparateBacksides);
    m_SeparateBacksidesCheckbox->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_SeparateBacksidesCheckbox->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideDefaultButton->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideDefaultButton->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideDefaultPreview->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideOffsetHorizontalSpin->SetRange(-0.3_in, 0.3_in);
    m_BacksideOffsetHorizontalSpin->SetValue(m_Project.m_Data.m_BacksideOffset.x);

    m_BacksideOffsetVerticalSpin->SetRange(-0.3_in, 0.3_in);
    m_BacksideOffsetVerticalSpin->SetValue(m_Project.m_Data.m_BacksideOffset.y);

    m_BacksideOffset->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideOffset->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideRotationSpin->setValue(m_Project.m_Data.m_BacksideRotation / 1_deg);

    m_BacksideRotation->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideRotation->setVisible(m_Project.m_Data.m_BacksideEnabled);

    m_BacksideAutoPattern->setText(ToQString(m_Project.m_Data.m_BacksideAutoPattern));
    SetBacksideAutoPatternTooltip();

    m_BacksideAuto->setEnabled(m_Project.m_Data.m_BacksideEnabled);
    m_BacksideAuto->setVisible(m_Project.m_Data.m_BacksideEnabled);
}

void CardOptionsWidget::SetAdvancedWidgetsVisibility()
{
    // Note: Everything else currently available in basic mode
    m_BacksideRotation->setVisible(g_Cfg.m_AdvancedMode);
}

void CardOptionsWidget::SetBacksideAutoPatternTooltip()
{
    QString auto_hint{
        QString{ "Matches e.g. Esika.png with %1.png" }
            .arg(m_BacksideAutoPattern->text())
            .replace("$", "Esika"),
    };
    m_BacksideAutoPattern->setToolTip(std::move(auto_hint));
}
