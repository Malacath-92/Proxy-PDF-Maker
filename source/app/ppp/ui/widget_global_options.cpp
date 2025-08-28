#include <ppp/ui/widget_global_options.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/cubes.hpp>
#include <ppp/plugins.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/style.hpp>

#include <ppp/ui/popups.hpp>
#include <ppp/ui/widget_combo_box.hpp>
#include <ppp/ui/widget_label.hpp>

class PluginsPopup : public PopupBase
{
    Q_OBJECT

  public:
    PluginsPopup(QWidget* parent)
        : PopupBase{ parent }
    {
        m_AutoCenter = false;
        setWindowFlags(Qt::WindowType::Dialog);

        auto* plugins{ new QGroupBox{} };
        {
            auto* layout{ new QVBoxLayout };
            for (const auto& plugin_name : GetPluginNames())
            {
                auto* plugin_checkbox{ new QCheckBox{ ToQString(plugin_name) } };
                plugin_checkbox->setChecked(g_Cfg.m_PluginsState[std::string{ plugin_name }]);
                layout->addWidget(plugin_checkbox);

                auto change_plugin_enabled{
                    [this, plugin_name](Qt::CheckState s)
                    {
                        const bool enabled{ s == Qt::CheckState::Checked };
                        g_Cfg.m_PluginsState[std::string{ plugin_name }] = enabled;
                        SaveConfig(g_Cfg);

                        if (enabled)
                        {
                            PluginEnabled(plugin_name);
                        }
                        else
                        {
                            PluginDisabled(plugin_name);
                        }
                    }
                };

                QObject::connect(plugin_checkbox,
                                 &QCheckBox::checkStateChanged,
                                 this,
                                 change_plugin_enabled);
            }
            plugins->setLayout(layout);
        }

        auto* buttons{ new QWidget{} };
        {
            auto* okay_button{ new QPushButton{ "OK" } };

            auto* layout{ new QHBoxLayout };
            layout->addWidget(okay_button);
            buttons->setLayout(layout);

            QObject::connect(okay_button,
                             &QPushButton::clicked,
                             this,
                             &AboutPopup::close);
        }

        auto* layout{ new QVBoxLayout };
        layout->addWidget(plugins);
        layout->addWidget(buttons);
        setLayout(layout);
    }

  signals:
    void PluginEnabled(std::string_view plugin_name);
    void PluginDisabled(std::string_view plugin_name);
};

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

    auto* image_format{ new ComboBoxWithLabel{
        "Image &Format", magic_enum::enum_names<ImageFormat>(), magic_enum::enum_name(g_Cfg.m_PdfImageFormat) } };
    image_format->GetWidget()->setToolTip("Determines how images are saved inside the pdf. Use Jpg to reduce output size.");
    image_format->setVisible(g_Cfg.m_Backend != PdfBackend::Png);

    auto* jpg_quality_spin_box{ new QDoubleSpinBox };
    jpg_quality_spin_box->setDecimals(0);
    jpg_quality_spin_box->setRange(1, 100);
    jpg_quality_spin_box->setSingleStep(1);
    jpg_quality_spin_box->setValue(g_Cfg.m_JpgQuality.value_or(100));
    auto* jpg_quality{ new WidgetWithLabel{ "Jpg &Quality", jpg_quality_spin_box } };
    jpg_quality->setToolTip("Quality of the jpg files embedded in the pdf.");
    jpg_quality->setVisible(g_Cfg.m_Backend != PdfBackend::Png && g_Cfg.m_PdfImageFormat == ImageFormat::Jpg);

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
    m_PageSizes = paper_sizes->GetWidget();

    auto* themes{ new ComboBoxWithLabel{
        "&Theme", GetStyles(), application.GetTheme() } };

    auto* plugins{ new QPushButton{ "Plugins" } };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(base_unit);
    layout->addWidget(display_columns);
    layout->addWidget(backend);
    layout->addWidget(image_format);
    layout->addWidget(jpg_quality);
    layout->addWidget(color_cube);
    layout->addWidget(preview_width);
    layout->addWidget(max_dpi);
    layout->addWidget(paper_sizes);
    layout->addWidget(themes);
    layout->addWidget(plugins);
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
        [this, image_format, jpg_quality](const QString& t)
        {
            g_Cfg.SetPdfBackend(magic_enum::enum_cast<PdfBackend>(t.toStdString())
                                    .value_or(PdfBackend::LibHaru));
            SaveConfig(g_Cfg);
            RenderBackendChanged();

            image_format->setVisible(g_Cfg.m_Backend != PdfBackend::Png);
            jpg_quality->setVisible(g_Cfg.m_Backend != PdfBackend::Png && g_Cfg.m_PdfImageFormat == ImageFormat::Jpg);
        }
    };

    auto change_image_format{
        [this, jpg_quality](const QString& t)
        {
            g_Cfg.m_PdfImageFormat = magic_enum::enum_cast<ImageFormat>(t.toStdString())
                                         .value_or(ImageFormat::Jpg);
            SaveConfig(g_Cfg);
            ImageFormatChanged();

            jpg_quality->setVisible(g_Cfg.m_PdfImageFormat == ImageFormat::Jpg);
        }
    };

    auto change_jpg_quality{
        [this](double v)
        {
            g_Cfg.m_JpgQuality = static_cast<int>(v);
            SaveConfig(g_Cfg);
            JpgQualityChanged();
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

    const auto open_plugins_popup{
        [this]()
        {
            PluginsPopup plugins{ nullptr };

            QObject::connect(&plugins,
                             &PluginsPopup::PluginEnabled,
                             this,
                             &GlobalOptionsWidget::PluginEnabled);
            QObject::connect(&plugins,
                             &PluginsPopup::PluginDisabled,
                             this,
                             &GlobalOptionsWidget::PluginDisabled);

            window()->setEnabled(false);
            plugins.Show();
            window()->setEnabled(true);
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
    QObject::connect(image_format->GetWidget(),
                     &QComboBox::currentTextChanged,
                     this,
                     change_image_format);
    QObject::connect(jpg_quality_spin_box,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     change_jpg_quality);
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
    QObject::connect(plugins,
                     &QPushButton::clicked,
                     this,
                     open_plugins_popup);
}

void GlobalOptionsWidget::PageSizesChanged()
{
    if (!g_Cfg.m_PageSizes.contains(g_Cfg.m_DefaultPageSize))
    {
        g_Cfg.m_DefaultPageSize = g_Cfg.GetFirstValidPageSize();
    }

    UpdateComboBox(m_PageSizes,
                   std::span<const std::string>(std::views::keys(g_Cfg.m_PageSizes) | std::ranges::to<std::vector>()),
                   {},
                   g_Cfg.m_DefaultPageSize);

    SaveConfig(g_Cfg);
}

#include <widget_global_options.moc>
