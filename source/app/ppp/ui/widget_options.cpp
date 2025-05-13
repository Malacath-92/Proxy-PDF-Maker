#include <ppp/ui/widget_options.hpp>

#include <atomic>
#include <charconv>
#include <cstdint>
#include <ranges>

#include <QCheckBox>
#include <QColorDialog>
#include <QCursor>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidget>

#include <magic_enum/magic_enum.hpp>

#include <dla/scalar_math.h>
#include <dla/vector_math.h>

#include <ppp/app.hpp>
#include <ppp/cubes.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/style.hpp>
#include <ppp/util/log.hpp>

#include <ppp/pdf/generate.hpp>
#include <ppp/pdf/util.hpp>
#include <ppp/svg/generate.hpp>

#include <ppp/project/image_ops.hpp>
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

        auto* backside_default_image{ new BacksideImage{ backside_name, MinimumWidth, project } };

        const auto backside_height{ backside_default_image->heightForWidth(MinimumWidth.value) };
        backside_default_image->setFixedWidth(MinimumWidth.value);
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

        DefaultImage = backside_default_image;
        DefaultLabel = backside_default_label;
    }

    void Refresh()
    {
        const fs::path& backside_name{ m_Project.Data.BacksideDefault };
        DefaultImage->Refresh(backside_name, MinimumWidth, m_Project);
        DefaultLabel->setText(ToQString(backside_name.c_str()));
    }

  private:
    inline static constexpr auto MinimumWidth{ 60_pix };

    BacksideImage* DefaultImage;
    QLabel* DefaultLabel;

    const Project& m_Project;
};

class CardOptionsWidget : public QGroupBox
{
  public:
    CardOptionsWidget(Project& project)
        : m_Project{ project }
    {
        setTitle("Card Options");

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

        auto* backside_guides_checkbox{ new QCheckBox{ "Enable Backside Guides" } };
        backside_guides_checkbox->setChecked(project.Data.BacksideEnableGuides);
        backside_guides_checkbox->setEnabled(project.Data.BacksideEnabled);
        backside_guides_checkbox->setVisible(project.Data.BacksideEnabled);

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

        auto* back_over_divider{ new QFrame };
        back_over_divider->setFrameShape(QFrame::Shape::HLine);
        back_over_divider->setFrameShadow(QFrame::Shadow::Sunken);

        auto* layout{ new QVBoxLayout };
        layout->addWidget(bleed_edge);
        layout->addWidget(corner_weight);
        layout->addWidget(bleed_back_divider);
        layout->addWidget(backside_checkbox);
        layout->addWidget(backside_guides_checkbox);
        layout->addWidget(backside_default_button);
        layout->addWidget(backside_default_preview);
        layout->addWidget(backside_offset);
        layout->addWidget(back_over_divider);

        layout->setAlignment(backside_default_preview, Qt::AlignmentFlag::AlignHCenter);

        setLayout(layout);

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
                backside_guides_checkbox->setEnabled(project.Data.BacksideEnabled);
                backside_guides_checkbox->setVisible(project.Data.BacksideEnabled);
                backside_default_button->setEnabled(project.Data.BacksideEnabled);
                backside_default_button->setVisible(project.Data.BacksideEnabled);
                backside_default_preview->setVisible(project.Data.BacksideEnabled);
                backside_offset->setEnabled(project.Data.BacksideEnabled);
                backside_offset->setVisible(project.Data.BacksideEnabled);
                main_window()->BacksideEnabledChanged(project);
            }
        };

        auto switch_backside_guides_enabled{
            [=, &project](Qt::CheckState s)
            {
                project.Data.BacksideEnableGuides = s == Qt::CheckState::Checked;
                main_window()->BacksideGuidesEnabledChanged(project);
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
        QObject::connect(backside_guides_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         switch_backside_guides_enabled);
        QObject::connect(backside_default_button,
                         &QPushButton::clicked,
                         this,
                         pick_backside);
        QObject::connect(backside_offset_spin,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_backside_offset);

        BleedEdgeSpin = bleed_edge_spin;
        BacksideCheckbox = backside_checkbox;
        BacksideOffsetSpin = backside_offset_spin;
        BacksideDefaultPreview = backside_default_preview;
    }

    void Refresh()
    {
        const auto base_unit{ CFG.BaseUnit.m_Unit };
        const auto full_bleed{ m_Project.CardFullBleed() };
        const auto full_bleed_rounded{ QString::number(full_bleed / base_unit, 'f', 2).toFloat() };
        if (static_cast<int32_t>(BleedEdgeSpin->maximum() / 0.001) != static_cast<int32_t>(full_bleed_rounded / 0.001))
        {
            BleedEdgeSpin->setRange(0, full_bleed / base_unit);
            BleedEdgeSpin->setValue(0);
        }
        else
        {
            BleedEdgeSpin->setValue(m_Project.Data.BleedEdge / base_unit);
        }
        BacksideCheckbox->setChecked(m_Project.Data.BacksideEnabled);
        BacksideOffsetSpin->setValue(m_Project.Data.BacksideOffset.value);
    }

    void RefreshWidgets()
    {
        BacksideDefaultPreview->Refresh();
    }

    void BaseUnitChanged()
    {
        const auto base_unit{ CFG.BaseUnit.m_Unit };
        const auto base_unit_name{ CFG.BaseUnit.m_ShortName };
        const auto full_bleed{ m_Project.CardFullBleed() };
        const auto bleed{ m_Project.Data.BleedEdge };
        const auto backside_offset{ m_Project.Data.BacksideOffset };

        BleedEdgeSpin->setRange(0, full_bleed / base_unit);
        BleedEdgeSpin->setSuffix(ToQString(base_unit_name));
        BleedEdgeSpin->setValue(bleed / base_unit);

        BacksideOffsetSpin->setRange(-0.3_in / base_unit, 0.3_in / base_unit);
        BacksideOffsetSpin->setSuffix(ToQString(base_unit_name));
        BacksideOffsetSpin->setValue(backside_offset / base_unit);
    }

  private:
    QDoubleSpinBox* BleedEdgeSpin{ nullptr };
    QCheckBox* BacksideCheckbox{ nullptr };
    QDoubleSpinBox* BacksideOffsetSpin{ nullptr };
    DefaultBacksidePreview* BacksideDefaultPreview{ nullptr };

    Project& m_Project;
};

class GlobalOptionsWidget : public QGroupBox
{
  public:
    GlobalOptionsWidget(PrintProxyPrepApplication& application, Project& project)
    {
        setTitle("Global Config");

        const auto base_unit_name{ CFG.BaseUnit.m_Name };
        auto* base_unit{ new ComboBoxWithLabel{
            "&Units",
            CFG.SupportedBaseUnits | std::views::transform(&UnitInfo::m_Name) | std::ranges::to<std::vector>(),
            base_unit_name } };
        base_unit->GetWidget()->setToolTip("Determines in which units measurements are given.");

        auto* display_columns_spin_box{ new QDoubleSpinBox };
        display_columns_spin_box->setDecimals(0);
        display_columns_spin_box->setRange(2, 10);
        display_columns_spin_box->setSingleStep(1);
        display_columns_spin_box->setValue(CFG.DisplayColumns);
        auto* display_columns{ new WidgetWithLabel{ "Display &Columns", display_columns_spin_box } };
        display_columns->setToolTip("Number columns in card view");

        auto* backend{ new ComboBoxWithLabel{
            "&Rendering Backend", magic_enum::enum_names<PdfBackend>(), magic_enum::enum_name(CFG.Backend) } };
        backend->GetWidget()->setToolTip("Determines how the backend used for rendering and the output format.");

        auto* precropped_checkbox{ new QCheckBox{ "Allow Precropped" } };
        precropped_checkbox->setChecked(CFG.EnableUncrop);
        precropped_checkbox->setToolTip("Allows putting pre-cropped images into images/crop");

        auto* color_cube{ new ComboBoxWithLabel{
            "Color C&ube", GetCubeNames(), CFG.ColorCube } };
        color_cube->GetWidget()->setToolTip("Requires rerunning cropper");

        auto* preview_width_spin_box{ new QDoubleSpinBox };
        preview_width_spin_box->setDecimals(0);
        preview_width_spin_box->setRange(120, 1000);
        preview_width_spin_box->setSingleStep(60);
        preview_width_spin_box->setSuffix("pixels");
        preview_width_spin_box->setValue(CFG.BasePreviewWidth / 1_pix);
        auto* preview_width{ new WidgetWithLabel{ "&Preview Width", preview_width_spin_box } };
        preview_width->setToolTip("Requires rerunning cropper to take effect");

        auto* max_dpi_spin_box{ new QDoubleSpinBox };
        max_dpi_spin_box->setDecimals(0);
        max_dpi_spin_box->setRange(300, 1200);
        max_dpi_spin_box->setSingleStep(100);
        max_dpi_spin_box->setValue(CFG.MaxDPI / 1_dpi);
        auto* max_dpi{ new WidgetWithLabel{ "&Max DPI", max_dpi_spin_box } };
        max_dpi->setToolTip("Requires rerunning cropper");

        auto* paper_sizes{ new ComboBoxWithLabel{
            "Default P&aper Size", std::views::keys(CFG.PageSizes) | std::ranges::to<std::vector>(), CFG.DefaultPageSize } };

        auto* themes{ new ComboBoxWithLabel{
            "&Theme", GetStyles(), application.GetTheme() } };

        auto* layout{ new QVBoxLayout };
        layout->addWidget(base_unit);
        layout->addWidget(display_columns);
        layout->addWidget(backend);
        layout->addWidget(precropped_checkbox);
        layout->addWidget(color_cube);
        layout->addWidget(preview_width);
        layout->addWidget(max_dpi);
        layout->addWidget(paper_sizes);
        layout->addWidget(themes);
        setLayout(layout);

        auto main_window{
            [this]()
            { return static_cast<PrintProxyPrepMainWindow*>(window()); }
        };

        auto change_base_units{
            [=, &project](const QString& t)
            {
                const auto base_unit{ CFG.GetUnitFromName(t.toStdString())
                                          .value_or(Config::SupportedBaseUnits[0]) };
                CFG.BaseUnit = base_unit;
                SaveConfig(CFG);
                main_window()->BaseUnitChanged(project);
            }
        };

        auto change_display_columns{
            [=, &project](double v)
            {
                CFG.DisplayColumns = static_cast<int>(v);
                SaveConfig(CFG);
                main_window()->DisplayColumnsChanged(project);
            }
        };

        auto change_render_backend{
            [=, &project](const QString& t)
            {
                CFG.SetPdfBackend(magic_enum::enum_cast<PdfBackend>(t.toStdString())
                                      .value_or(PdfBackend::LibHaru));
                if (CFG.Backend != PdfBackend::PoDoFo && project.Data.PageSize == Config::BasePDFSize)
                {
                    project.Data.PageSize = CFG.DefaultPageSize;
                    main_window()->PageSizeChanged(project);
                }
                SaveConfig(CFG);
                main_window()->RenderBackendChanged(project);
            }
        };

        auto change_precropped{
            [=, &project](Qt::CheckState s)
            {
                CFG.EnableUncrop = s == Qt::CheckState::Checked;
                SaveConfig(CFG);
                main_window()->EnableUncropChangedDiff(CFG.EnableUncrop);
                main_window()->EnableUncropChanged(project);
            }
        };

        auto change_color_cube{
            [=, &application, &project](const QString& t)
            {
                CFG.ColorCube = t.toStdString();
                SaveConfig(CFG);
                PreloadCube(application, CFG.ColorCube);
                main_window()->ColorCubeChangedDiff(CFG.ColorCube);
                main_window()->ColorCubeChanged(project);
            }
        };

        auto change_preview_width{
            [=, &project](double v)
            {
                CFG.BasePreviewWidth = static_cast<float>(v) * 1_pix;
                SaveConfig(CFG);
                main_window()->BasePreviewWidthChangedDiff(CFG.BasePreviewWidth);
                main_window()->BasePreviewWidthChanged(project);
            }
        };

        auto change_max_dpi{
            [=, &project](double v)
            {
                CFG.MaxDPI = static_cast<float>(v) * 1_dpi;
                SaveConfig(CFG);
                main_window()->MaxDPIChangedDiff(CFG.MaxDPI);
                main_window()->MaxDPIChanged(project);
            }
        };

        auto change_papersize{
            [=](const QString& t)
            {
                CFG.DefaultPageSize = t.toStdString();
                SaveConfig(CFG);
            }
        };

        auto change_theme{
            [=, &application](const QString& t)
            {
                application.SetTheme(t.toStdString());
                SetStyle(application, application.GetTheme());
            }
        };

        QObject::connect(base_unit->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_base_units);
        QObject::connect(display_columns_spin_box,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_display_columns);
        QObject::connect(backend->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_render_backend);
        QObject::connect(precropped_checkbox,
                         &QCheckBox::checkStateChanged,
                         this,
                         change_precropped);
        QObject::connect(color_cube->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_color_cube);
        QObject::connect(preview_width_spin_box,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_preview_width);
        QObject::connect(max_dpi_spin_box,
                         &QDoubleSpinBox::valueChanged,
                         this,
                         change_max_dpi);
        QObject::connect(paper_sizes->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_papersize);
        QObject::connect(themes->GetWidget(),
                         &QComboBox::currentTextChanged,
                         this,
                         change_theme);
    }
};

OptionsWidget::OptionsWidget(PrintProxyPrepApplication& application, Project& project)
{
    auto* card_options{ new CardOptionsWidget{ project } };
    auto* global_options{ new GlobalOptionsWidget{ application, project } };

    auto* widget{ new QWidget };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(card_options);
    layout->addWidget(global_options);
    layout->addStretch();

    widget->setLayout(layout);
    widget->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::MinimumExpanding);

    setWidget(widget);
    setMinimumHeight(400);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);

    CardOptions = card_options;
}

void OptionsWidget::RefreshWidgets()
{
    CardOptions->RefreshWidgets();
}

void OptionsWidget::BaseUnitChanged()
{
    CardOptions->BaseUnitChanged();
}
