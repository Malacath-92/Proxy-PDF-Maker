#include <ppp/ui/widget_global_options.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QVBoxLayout>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/cubes.hpp>
#include <ppp/style.hpp>

#include <ppp/ui/widget_label.hpp>

GlobalOptionsWidget::GlobalOptionsWidget(PrintProxyPrepApplication& application)
{
    setObjectName("Global Config");

    const auto base_unit_name{ g_Cfg.m_BaseUnit.m_Name };
    auto* base_unit{ new ComboBoxWithLabel{
        "&Units",
        g_Cfg.c_SupportedBaseUnits | std::views::transform(&UnitInfo::m_Name) | std::ranges::to<std::vector>(),
        base_unit_name } };
    base_unit->GetWidget()->setToolTip("Determines in which units measurements are given.");

    auto* display_columns_spin_box{ new QDoubleSpinBox };
    display_columns_spin_box->setDecimals(0);
    display_columns_spin_box->setRange(2, 10);
    display_columns_spin_box->setSingleStep(1);
    display_columns_spin_box->setValue(g_Cfg.m_DisplayColumns);
    auto* display_columns{ new WidgetWithLabel{ "Display &Columns", display_columns_spin_box } };
    display_columns->setToolTip("Number columns in card view");

    auto* backend{ new ComboBoxWithLabel{
        "&Rendering Backend", magic_enum::enum_names<PdfBackend>(), magic_enum::enum_name(g_Cfg.m_Backend) } };
    backend->GetWidget()->setToolTip("Determines how the backend used for rendering and the output format.");

    auto* precropped_checkbox{ new QCheckBox{ "Allow Precropped" } };
    precropped_checkbox->setChecked(g_Cfg.m_EnableUncrop);
    precropped_checkbox->setToolTip("Allows putting pre-cropped images into images/crop");

    auto* color_cube{ new ComboBoxWithLabel{
        "Color C&ube", GetCubeNames(), g_Cfg.m_ColorCube } };
    color_cube->GetWidget()->setToolTip("Requires rerunning cropper");

    auto* preview_width_spin_box{ new QDoubleSpinBox };
    preview_width_spin_box->setDecimals(0);
    preview_width_spin_box->setRange(120, 1000);
    preview_width_spin_box->setSingleStep(60);
    preview_width_spin_box->setSuffix("pixels");
    preview_width_spin_box->setValue(g_Cfg.m_BasePreviewWidth / 1_pix);
    auto* preview_width{ new WidgetWithLabel{ "&Preview Width", preview_width_spin_box } };
    preview_width->setToolTip("Requires rerunning cropper to take effect");

    auto* max_dpi_spin_box{ new QDoubleSpinBox };
    max_dpi_spin_box->setDecimals(0);
    max_dpi_spin_box->setRange(300, 1200);
    max_dpi_spin_box->setSingleStep(100);
    max_dpi_spin_box->setValue(g_Cfg.m_MaxDPI / 1_dpi);
    auto* max_dpi{ new WidgetWithLabel{ "&Max DPI", max_dpi_spin_box } };
    max_dpi->setToolTip("Requires rerunning cropper");

    auto* paper_sizes{ new ComboBoxWithLabel{
        "Default P&aper Size", std::views::keys(g_Cfg.m_PageSizes) | std::ranges::to<std::vector>(), g_Cfg.m_DefaultPageSize } };

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

    auto change_base_units{
        [this](const QString& t)
        {
            const auto base_unit{ g_Cfg.GetUnitFromName(t.toStdString())
                                      .value_or(Config::c_SupportedBaseUnits[0]) };
            g_Cfg.m_BaseUnit = base_unit;
            SaveConfig(g_Cfg);
            BaseUnitChanged();
        }
    };

    auto change_display_columns{
        [this](double v)
        {
            g_Cfg.m_DisplayColumns = static_cast<int>(v);
            SaveConfig(g_Cfg);
            DisplayColumnsChanged();
        }
    };

    auto change_render_backend{
        [this](const QString& t)
        {
            g_Cfg.SetPdfBackend(magic_enum::enum_cast<PdfBackend>(t.toStdString())
                                    .value_or(PdfBackend::LibHaru));
            SaveConfig(g_Cfg);
            RenderBackendChanged();
        }
    };

    auto change_precropped{
        [this](Qt::CheckState s)
        {
            g_Cfg.m_EnableUncrop = s == Qt::CheckState::Checked;
            SaveConfig(g_Cfg);
            EnableUncropChanged();
        }
    };

    auto change_color_cube{
        [this, &application](const QString& t)
        {
            g_Cfg.m_ColorCube = t.toStdString();
            SaveConfig(g_Cfg);
            PreloadCube(application, g_Cfg.m_ColorCube);
            ColorCubeChanged();
        }
    };

    auto change_preview_width{
        [this](double v)
        {
            g_Cfg.m_BasePreviewWidth = static_cast<float>(v) * 1_pix;
            SaveConfig(g_Cfg);
            BasePreviewWidthChanged();
        }
    };

    auto change_max_dpi{
        [this](double v)
        {
            g_Cfg.m_MaxDPI = static_cast<float>(v) * 1_dpi;
            SaveConfig(g_Cfg);
            MaxDPIChanged();
        }
    };

    auto change_papersize{
        [=](const QString& t)
        {
            g_Cfg.m_DefaultPageSize = t.toStdString();
            SaveConfig(g_Cfg);
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
