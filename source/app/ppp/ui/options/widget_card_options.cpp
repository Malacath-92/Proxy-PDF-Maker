#include <ppp/ui/options/widget_card_options.hpp>

#include <QCheckBox>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QResizeEvent>

#include <magic_enum/magic_enum.hpp>

#include <nlohmann/json.hpp>

#include <ppp/qt_util.hpp>

#include <ppp/project/project.hpp>

#include <ppp/ui/default_project_value_actions.hpp>
#include <ppp/ui/options/widget_double_spin_box.hpp>
#include <ppp/ui/widget_util/linked_spin_boxes.hpp>
#include <ppp/ui/widget_util/widget_card.hpp>
#include <ppp/ui/widget_util/widget_label.hpp>

#include <ppp/ui/popups/image_browse_popup.hpp>
#include <ppp/ui/popups/popups.hpp>

#include <ppp/ui/view_models/options/view_model_card_options.hpp>
#include <ppp/ui/view_models/util.hpp>

#include <ppp/profile/profile.hpp>

class DefaultBacksidePreview : public QWidget
{
  public:
    DefaultBacksidePreview(const Project& project)
        : m_Project{ project }
    {
        TRACY_AUTO_SCOPE();

        auto* backside_default_image{ MakeBacksideImage() };

        auto* layout{ new QVBoxLayout };
        layout->addWidget(backside_default_image);
        layout->setAlignment(backside_default_image, Qt::AlignmentFlag::AlignHCenter);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        setLayout(layout);

        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

        m_DefaultImage = backside_default_image;
    }

    void Refresh()
    {
        TRACY_AUTO_SCOPE();

        const auto& backside_name{ m_Project.m_Data.m_BacksideDefault };

        auto* backside_image{ dynamic_cast<BacksideImage*>(m_DefaultImage) };
        const bool had_backside{ backside_image != nullptr };
        const bool has_backside{ backside_name.has_value() };

        if (had_backside != has_backside)
        {
            auto* backside_default_image{ MakeBacksideImage() };
            layout()->replaceWidget(m_DefaultImage, backside_default_image);
            delete m_DefaultImage;
            m_DefaultImage = backside_default_image;
        }
        else if (had_backside)
        {
            backside_image->Refresh(backside_name.value(), c_ImageWidth, m_Project);
        }
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

    QWidget* MakeBacksideImage()
    {
        const auto& backside_name{ m_Project.m_Data.m_BacksideDefault };

        QWidget* backside_default_image{
            backside_name.has_value()
                ? static_cast<QWidget*>(new BacksideImage{ backside_name.value(), c_ImageWidth, m_Project })
                : new BlankCardImage{ m_Project, CardImageWidgetParams{ .m_MinimumWidth{ c_ImageWidth } } }
        };

        backside_default_image->setFixedWidth(c_ImageWidth.value);

        return backside_default_image;
    }

    inline static constexpr auto c_ImageWidth{ 60_pix };

    const Project& m_Project;

    QWidget* m_DefaultImage{ nullptr };
};

CardOptionsWidget::CardOptionsWidget(CardOptionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Card Options");
    m_ViewModel.setParent(this);

    const auto config_reqs{ m_ViewModel.GetDefaultDataRequirements() };
    const auto base_unit{ m_ViewModel.GetBaseUnit() };

    auto* bleed_edge{ new LengthSpinBoxWithLabel{ "&Bleed Edge", base_unit } };
    m_BleedEdgeSpin = bleed_edge->GetWidget();
    m_BleedEdgeSpin->ConnectUnitSignals(this);
    m_BleedEdgeSpin->setDecimals(2);
    m_BleedEdgeSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_BleedEdgeSpin, config_reqs, "bleed_edge_cm");

    auto* envelope{ new LengthSpinBoxWithLabel{ "En&velope", base_unit } };
    m_EnvelopeSpin = envelope->GetWidget();
    m_EnvelopeSpin->ConnectUnitSignals(this);
    m_EnvelopeSpin->setDecimals(2);
    m_EnvelopeSpin->setSingleStep(0.1);
    m_EnvelopeSpin->setToolTip("Similar to bleed edge, but doesn't increase space between cards.");
    EnableOptionWidgetForDefaults(m_EnvelopeSpin, config_reqs, "envelope_bleed_edge_cm");

    m_Spacing = new LinkedSpinBoxes{ true, base_unit };

    m_HorizontalSpacingSpin = m_Spacing->First();
    m_HorizontalSpacingSpin->ConnectUnitSignals(this);
    m_HorizontalSpacingSpin->setDecimals(2);
    m_HorizontalSpacingSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_HorizontalSpacingSpin, config_reqs, "spacing.horizontal");

    m_VerticalSpacingSpin = m_Spacing->Second();
    m_VerticalSpacingSpin->ConnectUnitSignals(this);
    m_VerticalSpacingSpin->setDecimals(2);
    m_VerticalSpacingSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_VerticalSpacingSpin, config_reqs, "spacing.vertical");

    auto* spacing{ new WidgetWithLabel{ "Card Spacing", m_Spacing } };
    spacing->layout()->setAlignment(spacing->GetLabel(), Qt::AlignTop);

    auto* corners{ new ComboBoxWithLabel{ "Cor&ners",
                                          magic_enum::enum_names<CardCorners>(),
                                          "Rounded" } };
    m_Corners = corners->GetWidget();
    m_Corners->setToolTip("Determines if corners in the rendered pdf are square or rounded, only available if bleed edge is zero.");
    EnableOptionWidgetForDefaults(m_Corners, config_reqs, "corners");

    m_BacksideCheckbox = new QCheckBox{ "Enable Backside" };
    EnableOptionWidgetForDefaults(m_BacksideCheckbox, config_reqs, "backside_enabled");

    m_SeparateBacksidesCheckbox = new QCheckBox{ "Separate Backsides-PDF" };
    m_SeparateBacksidesCheckbox->setToolTip("Generate two PDFs, one from the frontsides and one for the backsides.");
    EnableOptionWidgetForDefaults(m_SeparateBacksidesCheckbox, config_reqs, "separate_backsides");

    m_BacksideDefaultButton = new QPushButton{ "Choose Default" };

    m_BacksideDefaultPreview = new DefaultBacksidePreview{ m_ViewModel.GetProject() };
    EnableOptionWidgetForDefaults(
        m_BacksideDefaultPreview,
        config_reqs,
        "backside_default",
        [this](nlohmann::json default_value)
        {
            if (default_value.is_null())
            {
                m_ViewModel.ClearBacksideDefault();
            }
            else
            {
                const auto& backside_default{ default_value.get_ref<const std::string&>() };
                m_ViewModel.ChangeBacksideDefault(ToQString(backside_default));
            }
        },
        [this]() -> nlohmann::json
        {
            if (auto backside_default{ m_ViewModel.GetBacksideDefault() })
            {
                return backside_default.value();
            }
            else
            {
                return nlohmann::json{};
            }
        });

    {
        m_BacksideOffsetHorizontalSpin = MakeLengthSpinBox(base_unit);
        m_BacksideOffsetHorizontalSpin->ConnectUnitSignals(this);
        m_BacksideOffsetHorizontalSpin->setDecimals(2);
        m_BacksideOffsetHorizontalSpin->setSingleStep(0.1);
        m_BacksideOffsetHorizontalSpin->SetRange(-0.3_in, 0.3_in);
        EnableOptionWidgetForDefaults(m_BacksideOffsetHorizontalSpin, config_reqs, "backside_offset.horizontal");

        m_BacksideOffsetVerticalSpin = MakeLengthSpinBox(base_unit);
        m_BacksideOffsetVerticalSpin->ConnectUnitSignals(this);
        m_BacksideOffsetVerticalSpin->setDecimals(2);
        m_BacksideOffsetVerticalSpin->setSingleStep(0.1);
        m_BacksideOffsetVerticalSpin->SetRange(-0.3_in, 0.3_in);
        EnableOptionWidgetForDefaults(m_BacksideOffsetVerticalSpin, config_reqs, "backside_offset.vertical");

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

    auto* backside_bleed{ new LengthSpinBoxWithLabel{ "Backside Extra Bleed", base_unit } };
    m_BacksideExtraBleedEdgeSpin = backside_bleed->GetWidget();
    m_BacksideExtraBleedEdgeSpin->ConnectUnitSignals(this);
    m_BacksideExtraBleedEdgeSpin->setDecimals(2);
    m_BacksideExtraBleedEdgeSpin->setSingleStep(0.1);
    EnableOptionWidgetForDefaults(m_BacksideExtraBleedEdgeSpin, config_reqs, "backside_bleed");

    m_BacksideExtraBleedEdge = backside_bleed;

    auto* backside_rotation{ new DoubleSpinBoxWithLabel{ "Backside Rotation" } };
    m_BacksideRotationSpin = backside_rotation->GetWidget();
    m_BacksideRotationSpin->setDecimals(2);
    m_BacksideRotationSpin->setSingleStep(0.1);
    m_BacksideRotationSpin->setRange(-10, 10);
    m_BacksideRotationSpin->setSuffix("deg");
    EnableOptionWidgetForDefaults(m_BacksideRotationSpin, config_reqs, "backside_rotation");

    m_BacksideRotation = backside_rotation;

    m_BacksideAutoPattern = new QLineEdit{ "Auto-Pattern" };
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
    layout->addWidget(m_BacksideExtraBleedEdge);
    layout->addWidget(m_BacksideRotation);
    layout->addWidget(m_BacksideAuto);

    layout->setAlignment(m_BacksideDefaultPreview, Qt::AlignmentFlag::AlignHCenter);
    setLayout(layout);

    auto pick_backside{
        [this]()
        {
            ImageBrowsePopup image_browser{ window(), m_ViewModel.GetProject() };
            image_browser.setWindowTitle("Choose default backside");

            if (const auto default_backside_choice{ image_browser.Show() })
            {
                m_ViewModel.ChangeBacksideDefault(ToQString(default_backside_choice.value()));
            }
            else if (image_browser.GetChoice() == ImageBrowsePopup::Choice::Clear)
            {
                m_ViewModel.ClearBacksideDefault();
            }
            else if (image_browser.GetChoice() == ImageBrowsePopup::Choice::Reset)
            {
                ResetToDefault(m_BacksideDefaultPreview);
            }
        }
    };

    QObject::connect(m_BleedEdgeSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeBleedEdge);
    QObject::connect(m_EnvelopeSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeEnvelopeBleedEdge);
    QObject::connect(m_Spacing,
                     &LinkedSpinBoxes::LinkChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeSpacingLinked);
    QObject::connect(m_HorizontalSpacingSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     [this](Length v)
                     { m_ViewModel.ChangeSpacing(
                           { v, m_VerticalSpacingSpin->Value() }); });
    QObject::connect(m_VerticalSpacingSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     [this](Length v)
                     { m_ViewModel.ChangeSpacing(
                           { m_HorizontalSpacingSpin->Value(), v }); });
    QObject::connect(m_Corners,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeCorners);
    QObject::connect(m_BacksideCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeBacksideEnabled);
    QObject::connect(m_SeparateBacksidesCheckbox,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeSeparateBacksidesEnabled);
    QObject::connect(m_BacksideDefaultButton,
                     &QPushButton::clicked,
                     &m_ViewModel,
                     pick_backside);
    QObject::connect(m_BacksideOffsetHorizontalSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     [this](Length v)
                     { m_ViewModel.ChangeBacksideOffset(
                           { v, m_BacksideOffsetVerticalSpin->Value() }); });
    QObject::connect(m_BacksideOffsetVerticalSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     [this](Length v)
                     { m_ViewModel.ChangeBacksideOffset(
                           { m_BacksideOffsetHorizontalSpin->Value(), v }); });
    QObject::connect(m_BacksideExtraBleedEdgeSpin,
                     &LengthSpinBox::ValueChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeBacksideExtraBleedEdge);
    QObject::connect(m_BacksideRotationSpin,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeBacksideRotation);
    QObject::connect(m_BacksideAutoPattern,
                     &QLineEdit::textChanged,
                     &m_ViewModel,
                     &CardOptionsViewModel::ChangeBacksideAutoPattern);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(AdvancedModeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BaseUnitChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(SeparateBacksidesEnabledChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideDefaultChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideOffsetChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideRotationChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideExtraBleedEdgeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BacksideAutoPatternChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BleedEdgeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(EnvelopeBleedEdgeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(SpacingChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(SpacingLinkedChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CornersChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ImageDirChanged);

    m_ViewModel.EmitDefaults();
}

void CardOptionsWidget::AdvancedModeChanged(bool advanced_mode)
{
    // Note: Everything else currently available in basic mode
    m_BacksideExtraBleedEdge->setVisible(m_BacksideCheckbox->isChecked() && advanced_mode);
    m_BacksideRotation->setVisible(m_BacksideCheckbox->isChecked() && advanced_mode);
}

void CardOptionsWidget::BacksideEnabledChanged(bool backside_enabled)
{
    TRACY_AUTO_SCOPE();

    m_BacksideCheckbox->setChecked(backside_enabled);

    m_SeparateBacksidesCheckbox->setEnabled(backside_enabled);
    m_SeparateBacksidesCheckbox->setVisible(backside_enabled);

    m_BacksideDefaultButton->setEnabled(backside_enabled);
    m_BacksideDefaultButton->setVisible(backside_enabled);

    m_BacksideDefaultPreview->setVisible(backside_enabled);

    m_BacksideOffset->setEnabled(backside_enabled);
    m_BacksideOffset->setVisible(backside_enabled);

    m_BacksideAuto->setEnabled(backside_enabled);
    m_BacksideAuto->setVisible(backside_enabled);

    m_BacksideExtraBleedEdge->setVisible(backside_enabled && m_ViewModel.GetAdvancedMode());
    m_BacksideRotation->setVisible(backside_enabled && m_ViewModel.GetAdvancedMode());

    m_BacksideCheckbox->blockSignals(true);
    m_BacksideCheckbox->setChecked(backside_enabled);
    m_BacksideCheckbox->blockSignals(false);
}
void CardOptionsWidget::SeparateBacksidesEnabledChanged(bool separate_backsides)
{
    m_SeparateBacksidesCheckbox->blockSignals(true);
    m_SeparateBacksidesCheckbox->setChecked(separate_backsides);
    m_SeparateBacksidesCheckbox->blockSignals(false);
}

void CardOptionsWidget::BacksideDefaultChanged(OptionalImageRef /*backside_card_name*/)
{
    m_BacksideDefaultPreview->Refresh();
}

void CardOptionsWidget::BacksideOffsetChanged(Size offset)
{
    m_BacksideOffsetVerticalSpin->blockSignals(true);
    m_BacksideOffsetVerticalSpin->SetValue(offset.x);
    m_BacksideOffsetVerticalSpin->blockSignals(false);

    m_BacksideOffsetHorizontalSpin->blockSignals(true);
    m_BacksideOffsetHorizontalSpin->SetValue(offset.y);
    m_BacksideOffsetHorizontalSpin->blockSignals(false);
}
void CardOptionsWidget::BacksideRotationChanged(Angle backside_rotation)
{
    m_BacksideRotationSpin->blockSignals(true);
    m_BacksideRotationSpin->setValue(backside_rotation / 1_deg);
    m_BacksideRotationSpin->blockSignals(false);
}
void CardOptionsWidget::BacksideExtraBleedEdgeChanged(Length backside_extra_bleed_edge)
{
    m_BacksideExtraBleedEdgeSpin->blockSignals(true);
    m_BacksideExtraBleedEdgeSpin->SetValue(backside_extra_bleed_edge);
    m_BacksideExtraBleedEdgeSpin->blockSignals(false);
}

void CardOptionsWidget::BacksideAutoPatternChanged(const std::string& pattern_std)
{
    TRACY_AUTO_SCOPE();

    const auto pattern{ ToQString(pattern_std) };

    m_BacksideAutoPattern->blockSignals(true);

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
    }
    else if (pattern == '$')
    {
        m_BacksideAutoPattern->setToolTip("Pattern can't be only $");
        m_BacksideAutoPattern->setStyleSheet(c_WarningStyle);
    }
    else
    {
        m_BacksideAutoPattern->setStyleSheet("");
    }

    m_BacksideAutoPattern->setText(pattern);

    auto auto_hint{
        QString{ "Matches e.g. Esika.png with %1.png" }
            .arg(m_BacksideAutoPattern->text())
            .replace("$", "Esika"),
    };
    m_BacksideAutoPattern->setToolTip(std::move(auto_hint));

    m_BacksideAutoPattern->blockSignals(true);
}

void CardOptionsWidget::BleedEdgeChanged(Length bleed_edge)
{
    m_BleedEdgeSpin->blockSignals(true);
    m_BleedEdgeSpin->SetValue(bleed_edge);
    m_BleedEdgeSpin->blockSignals(false);

    const auto total_bleed{ m_ViewModel.GetTotalBleed() };
    const bool has_no_bleed_edge{ total_bleed == 0_mm };
    if (m_Corners->isEnabled() != has_no_bleed_edge)
    {
        m_Corners->setEnabled(has_no_bleed_edge);
    }

    const auto full_bleed{ m_ViewModel.GetFullBleed() };
    m_EnvelopeSpin->SetRange(0_mm, full_bleed - bleed_edge);
    m_BacksideExtraBleedEdgeSpin->SetRange(0_mm, full_bleed - total_bleed);
}
void CardOptionsWidget::EnvelopeBleedEdgeChanged(Length envelope_bleed_edge)
{
    m_EnvelopeSpin->blockSignals(true);
    m_EnvelopeSpin->SetValue(envelope_bleed_edge);
    m_EnvelopeSpin->blockSignals(false);

    const auto total_bleed{ m_ViewModel.GetTotalBleed() };
    const bool has_no_bleed_edge{ total_bleed == 0_mm };
    if (m_Corners->isEnabled() != has_no_bleed_edge)
    {
        m_Corners->setEnabled(has_no_bleed_edge);
    }

    const auto full_bleed{ m_ViewModel.GetFullBleed() };
    m_BleedEdgeSpin->SetRange(0_mm, full_bleed - envelope_bleed_edge);
    m_BacksideExtraBleedEdgeSpin->SetRange(0_mm, full_bleed - total_bleed);
}

void CardOptionsWidget::SpacingChanged(Size spacing)
{
    m_HorizontalSpacingSpin->blockSignals(true);
    m_HorizontalSpacingSpin->SetValue(spacing.x);
    m_HorizontalSpacingSpin->blockSignals(false);

    m_VerticalSpacingSpin->blockSignals(true);
    m_VerticalSpacingSpin->SetValue(spacing.y);
    m_VerticalSpacingSpin->blockSignals(false);
}
void CardOptionsWidget::SpacingLinkedChanged(bool spacing_linked)
{
    m_HorizontalSpacingSpin->blockSignals(true);
    m_VerticalSpacingSpin->blockSignals(true);
    m_Spacing->blockSignals(true);

    m_Spacing->SetLinked(spacing_linked);

    m_Spacing->blockSignals(false);
    m_VerticalSpacingSpin->blockSignals(false);
    m_HorizontalSpacingSpin->blockSignals(false);
}

void CardOptionsWidget::CornersChanged(CardCorners corners)
{
    m_Corners->blockSignals(true);
    m_Corners->setCurrentText(ToQString(magic_enum::enum_name(corners)));
    m_Corners->blockSignals(false);
}

void CardOptionsWidget::ImageDirChanged(const fs::path& /*old_path*/,
                                        const fs::path& /*new_path*/)
{
    m_BacksideDefaultPreview->Refresh();
}
