#include <ppp/ui/options/widget_global_options.hpp>

#include <ranges>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/cubes.hpp>
#include <ppp/plugins.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/style.hpp>

#include <ppp/ui/popups/popups.hpp>
#include <ppp/ui/widget_util/widget_combo_box.hpp>
#include <ppp/ui/widget_util/widget_double_spin_box.hpp>
#include <ppp/ui/widget_util/widget_label.hpp>

#include <ppp/ui/view_models/options/view_model_global_options.hpp>
#include <ppp/ui/view_models/util.hpp>

#include <ppp/profile/profile.hpp>

class PluginsPopup : public PopupBase
{
    Q_OBJECT

  public:
    PluginsPopup(QWidget* parent,
                 const std::unordered_map<std::string, bool>& plugins_state)
        : PopupBase{ parent }
    {
        TRACY_AUTO_SCOPE();

        m_AutoCenter = false;
        setWindowFlags(Qt::WindowType::Dialog);
        setWindowTitle("Plugins");

        auto* plugins{ new QGroupBox{} };
        plugins->setTitle("Plugins");
        {
            auto* layout{ new QVBoxLayout };
            for (const auto& plugin_name : GetPluginNames())
            {
                const auto state_it{ plugins_state.find(std::string{ plugin_name }) };

                auto* plugin_checkbox{ new QCheckBox{ ToQString(plugin_name) } };
                plugin_checkbox->setChecked(state_it != plugins_state.end() && state_it->second);
                layout->addWidget(plugin_checkbox);

                auto change_plugin_enabled{
                    [this, plugin_name](Qt::CheckState s)
                    {
                        const bool enabled{ s == Qt::CheckState::Checked };
                        if (enabled)
                        {
                            EnablePlugin(plugin_name);
                        }
                        else
                        {
                            DisablePlugin(plugin_name);
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

        auto* info{ new QLabel{ "Enabled plugins can be opened at the very bottom of the side panel." } };
        info->setWordWrap(true);

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
        layout->addWidget(info);
        layout->addWidget(buttons);
        setLayout(layout);
    }

  signals:
    void EnablePlugin(std::string_view plugin_name);
    void DisablePlugin(std::string_view plugin_name);
};

GlobalOptionsWidget::GlobalOptionsWidget(GlobalOptionsViewModel* view_model)
    : m_ViewModel{ *view_model }
{
    TRACY_AUTO_SCOPE();

    setObjectName("Global Config");
    m_ViewModel.setParent(this);

    m_AdvancedMode = new QCheckBox{ "Advanced Mode" };
    m_AdvancedMode->setToolTip("Enables advanced features such as custom margins, guides controls, and card orientation.");

    const auto base_unit{ new ComboBoxWithLabel{
        "&Units",
        magic_enum::enum_values<Unit>() |
            std::views::transform(&UnitName) |
            std::ranges::to<std::vector>(),
        "Base Unit" } };
    m_BaseUnit = base_unit->GetWidget();
    m_BaseUnit->setToolTip("Determines in which units measurements are given.");

    m_DisplayColumns = MakeDoubleSpinBox();
    m_DisplayColumns->setDecimals(0);
    m_DisplayColumns->setSingleStep(1);
    auto* display_columns{ new WidgetWithLabel{ "Display &Columns", m_DisplayColumns } };
    display_columns->setToolTip("Number columns in card view");

    m_VersionOutput = new QCheckBox{ "&Version Output" };
    m_VersionOutput->setToolTip("If checked, output will not be overwritten, but instead versioned. I.e. _printme.pdf -> _printme_1.pdf -> ....");

    m_RenderToPng = new QCheckBox{ "&Render to Png" };
    m_RenderToPng->setToolTip("If checked, will render final document to a set of .png files instead of a .pdf file.");

    auto image_format{ new ComboBoxWithLabel{
        "Image Compress&ion",
        magic_enum::enum_names<ImageCompression>(),
        "Image Compression" } };
    m_ImageFormat = image_format->GetWidget();
    m_ImageFormat->setToolTip("Determines how images are saved inside the pdf. Use Lossy to reduce output size.");

    m_JpgQuality = MakeDoubleSpinBox();
    m_JpgQuality->setDecimals(0);
    m_JpgQuality->setRange(1, 100);
    m_JpgQuality->setSingleStep(1);
    auto* jpg_quality{ new WidgetWithLabel{ "Jpg &Quality", m_JpgQuality } };
    jpg_quality->setToolTip("Quality of the jpg files embedded in the pdf.");

    auto* color_cube{ new ComboBoxWithLabel{
        "Color C&ube", GetCubeNames(), "Color Cube" } };
    m_ColorCube = color_cube->GetWidget();

    m_PreviewWidth = MakeDoubleSpinBox();
    m_PreviewWidth->setDecimals(0);
    m_PreviewWidth->setRange(120, 1000);
    m_PreviewWidth->setSingleStep(60);
    m_PreviewWidth->setSuffix("pixels");
    auto* preview_width{ new WidgetWithLabel{ "&Preview Width", m_PreviewWidth } };
    preview_width->setToolTip("Width of each card in pixels in the preview.");

    m_NoCropMode = new QCheckBox{ "No &Crop Mode" };
    m_NoCropMode->setToolTip("If checked, images won't be cropped. Speeds up work while making pdfs slightly bigger.");

    m_MaxDPI = MakeDoubleSpinBox();
    m_MaxDPI->setDecimals(0);
    m_MaxDPI->setRange(300, 1200);
    m_MaxDPI->setSingleStep(100);
    auto* max_dpi{ new WidgetWithLabel{ "&Max DPI", m_MaxDPI } };

    auto* card_order{ new ComboBoxWithLabel{
        "&Card Sorting",
        CardOrder::Alphabetical } };
    m_CardOrder = card_order->GetWidget();
    card_order->GetWidget()->setToolTip("Determines how cards are sorted in the pdf and card grid.");

    auto* card_order_direction{ new ComboBoxWithLabel{
        "&Sort Direction",
        CardOrderDirection::Ascending } };
    m_CardOrderDirection = card_order_direction->GetWidget();

    const auto ideal_thread_count{ static_cast<uint32_t>(QThread::idealThreadCount()) };
    m_MaxWorkerThreads = MakeDoubleSpinBox();
    m_MaxWorkerThreads->setDecimals(0);
    m_MaxWorkerThreads->setRange(1, ideal_thread_count - 1);
    m_MaxWorkerThreads->setSingleStep(1);
    auto* max_worker_threads{ new WidgetWithLabel{ "Max &Worker Threads", m_MaxWorkerThreads } };
    max_worker_threads->setToolTip("Higher numbers speed up cropping and pdf generation, but cost more system resources");

    auto& application{ *ppApp };
    auto* style{ new ComboBoxWithLabel{
        "&Theme", GetStyles(), application.GetTheme() } };
    m_Style = style->GetWidget();

    m_CheckVersion = new QCheckBox{ "Check for Updates" };
    m_CheckVersion->setToolTip("Determine whether to check for updates at startup.");

    m_ToastTimeout = MakeDoubleSpinBox();
    m_ToastTimeout->setDecimals(2);
    m_ToastTimeout->setRange(0, 10);
    m_ToastTimeout->setSingleStep(0.5);
    m_ToastTimeout->setSuffix("s");
    auto* toast_duration{ new WidgetWithLabel{ "Toast Duration", m_ToastTimeout } };
    toast_duration->setToolTip("Determines the length of time a toast notification stays on screen, disables toast at 0s");

    auto* plugins{ new QPushButton{ "Plugins" } };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_AdvancedMode);
    layout->addWidget(base_unit);
    layout->addWidget(display_columns);
    layout->addWidget(m_VersionOutput);
    layout->addWidget(m_RenderToPng);
    layout->addWidget(image_format);
    layout->addWidget(jpg_quality);
    layout->addWidget(color_cube);
    layout->addWidget(preview_width);
    layout->addWidget(m_NoCropMode);
    layout->addWidget(max_dpi);
    layout->addWidget(card_order);
    layout->addWidget(card_order_direction);
    layout->addWidget(max_worker_threads);
    layout->addWidget(m_Style);
    layout->addWidget(m_CheckVersion);
    layout->addWidget(toast_duration);
    layout->addWidget(plugins);
    setLayout(layout);

    QObject::connect(m_AdvancedMode,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeAdvancedMode);
    QObject::connect(base_unit->GetWidget(),
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeBaseUnit);
    QObject::connect(m_DisplayColumns,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeDisplayColumns);
    QObject::connect(m_VersionOutput,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeVersionOutput);
    QObject::connect(m_RenderToPng,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     [this](Qt::CheckState s)
                     {
                         m_ViewModel.ChangePdfBackend(s == Qt::CheckState::Checked
                                                          ? PdfBackend::Png
                                                          : PdfBackend::PoDoFo);
                     });
    QObject::connect(m_ImageFormat,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeImageCompression);
    QObject::connect(m_JpgQuality,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeJpgQuality);
    QObject::connect(m_ColorCube,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeColorCube);
    QObject::connect(m_NoCropMode,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeNoCropMode);
    QObject::connect(m_PreviewWidth,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeBasePreviewWidth);
    QObject::connect(m_MaxDPI,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeMaxDPI);
    QObject::connect(m_CardOrder,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeCardOrder);
    QObject::connect(m_CardOrderDirection,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeCardOrderDirection);
    QObject::connect(m_MaxWorkerThreads,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeMaxWorkerThreads);
    QObject::connect(m_Style,
                     &QComboBox::currentTextChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeStyle);
    QObject::connect(m_CheckVersion,
                     &QCheckBox::checkStateChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeCheckVersionOnStartup);
    QObject::connect(m_ToastTimeout,
                     &QDoubleSpinBox::valueChanged,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::ChangeToastTimeoutMS);

    QObject::connect(plugins,
                     &QPushButton::clicked,
                     this,
                     &GlobalOptionsWidget::OpenPluginsWindow);

    FORWARD_SIGNAL_FROM_VIEW_MODEL(AdvancedModeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(NoCropModeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CheckVersionOnStartupChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ToastTimeoutMSChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BasePreviewWidthChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(MaxDPIChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardOrderChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(CardOrderDirectionChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(MaxWorkerThreadsChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(DisplayColumnsChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(MaxDisplayColumnsChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ColorCubeChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(VersionOutputChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PdfBackendChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ImageCompressionChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(PngCompressionChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(JpgQualityChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(BaseUnitChanged);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(OpenPluginsWindow);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(ColorCubeAdded);
    FORWARD_SIGNAL_FROM_VIEW_MODEL(StyleAdded);

    m_ViewModel.EmitDefaults();
}

void GlobalOptionsWidget::OpenPluginsWindow()
{
    const auto& plugins_state{ m_ViewModel.GetPluginsState() };
    PluginsPopup plugins{ nullptr, plugins_state };

    QObject::connect(&plugins,
                     &PluginsPopup::EnablePlugin,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::EnablePlugin);
    QObject::connect(&plugins,
                     &PluginsPopup::DisablePlugin,
                     &m_ViewModel,
                     &GlobalOptionsViewModel::DisablePlugin);

    window()->setEnabled(false);
    plugins.Show();
    window()->setEnabled(true);
}

void GlobalOptionsWidget::AdvancedModeChanged(bool advanced_mode)
{
    m_AdvancedMode->blockSignals(true);
    m_AdvancedMode->setChecked(advanced_mode);
    m_AdvancedMode->blockSignals(false);
}

void GlobalOptionsWidget::NoCropModeChanged(bool no_crop_mode)
{
    m_NoCropMode->blockSignals(true);
    m_NoCropMode->setChecked(no_crop_mode);
    m_NoCropMode->blockSignals(false);
}

void GlobalOptionsWidget::CheckVersionOnStartupChanged(bool check_version)
{
    m_CheckVersion->blockSignals(true);
    m_CheckVersion->setChecked(check_version);
    m_CheckVersion->blockSignals(false);
}
void GlobalOptionsWidget::ToastTimeoutMSChanged(uint32_t toast_timeout_ms)
{
    m_ToastTimeout->blockSignals(true);
    m_ToastTimeout->setValue(toast_timeout_ms);
    m_ToastTimeout->blockSignals(false);
}

void GlobalOptionsWidget::BasePreviewWidthChanged(Pixel base_preview_width)
{
    m_PreviewWidth->blockSignals(true);
    m_PreviewWidth->setValue(base_preview_width / 1_pix);
    m_PreviewWidth->blockSignals(false);
}
void GlobalOptionsWidget::MaxDPIChanged(PixelDensity max_dpi)
{
    m_MaxDPI->blockSignals(true);
    m_MaxDPI->setValue(max_dpi / 1_dpi);
    m_MaxDPI->blockSignals(false);
}

void GlobalOptionsWidget::CardOrderChanged(CardOrder card_order)
{
    m_CardOrder->blockSignals(true);
    m_CardOrder->setCurrentText(ToQString(magic_enum::enum_name(card_order)));
    m_CardOrder->blockSignals(false);
}
void GlobalOptionsWidget::CardOrderDirectionChanged(CardOrderDirection card_order_direction)
{
    m_CardOrderDirection->blockSignals(true);
    m_CardOrderDirection->setCurrentText(ToQString(magic_enum::enum_name(card_order_direction)));
    m_CardOrderDirection->blockSignals(false);
}

void GlobalOptionsWidget::MaxWorkerThreadsChanged(uint32_t max_worker_threads)
{
    m_MaxWorkerThreads->blockSignals(true);
    m_MaxWorkerThreads->setValue(max_worker_threads);
    m_MaxWorkerThreads->blockSignals(false);
}

void GlobalOptionsWidget::DisplayColumnsChanged(uint32_t display_columns)
{
    m_DisplayColumns->blockSignals(true);
    m_DisplayColumns->setValue(display_columns);
    m_DisplayColumns->blockSignals(false);
}
void GlobalOptionsWidget::MaxDisplayColumnsChanged(uint32_t max_display_columns)
{
    m_DisplayColumns->blockSignals(true);
    m_DisplayColumns->setRange(1, max_display_columns);
    m_DisplayColumns->blockSignals(false);
}

void GlobalOptionsWidget::ColorCubeChanged(std::string_view color_cube)
{
    m_ColorCube->blockSignals(true);
    m_ColorCube->setCurrentText(ToQString(color_cube));
    m_ColorCube->blockSignals(false);
}

void GlobalOptionsWidget::VersionOutputChanged(bool version_output)
{
    m_VersionOutput->blockSignals(true);
    m_VersionOutput->setChecked(version_output);
    m_VersionOutput->blockSignals(false);
}

void GlobalOptionsWidget::PdfBackendChanged(PdfBackend pdf_backend)
{
    m_RenderToPng->blockSignals(true);
    m_RenderToPng->setChecked(pdf_backend == PdfBackend::Png);
    m_RenderToPng->blockSignals(false);

    m_ImageFormat->setVisible(pdf_backend != PdfBackend::Png);
    m_JpgQuality->setVisible(pdf_backend != PdfBackend::Png &&
                             m_ImageFormat->currentText() ==
                                 magic_enum::enum_name(ImageCompression::Lossy));
}
void GlobalOptionsWidget::ImageCompressionChanged(ImageCompression compression)
{
    m_ImageFormat->blockSignals(true);
    m_ImageFormat->setCurrentText(ToQString(magic_enum::enum_name(compression)));
    m_ImageFormat->blockSignals(false);

    m_ImageFormat->setVisible(!m_RenderToPng->isChecked());
    m_JpgQuality->setVisible(!m_RenderToPng->isChecked() &&
                             compression == ImageCompression::Lossy);
}
void GlobalOptionsWidget::PngCompressionChanged(std::optional<int> /*png_compression*/)
{
    // Currently not exposed ...
}
void GlobalOptionsWidget::JpgQualityChanged(std::optional<int> jpg_quality)
{
    m_JpgQuality->blockSignals(true);
    m_JpgQuality->setValue(jpg_quality.value_or(100));
    m_JpgQuality->blockSignals(false);
}

void GlobalOptionsWidget::BaseUnitChanged(Unit base_unit)
{
    const auto base_unit_name{ UnitName(base_unit) };

    m_BaseUnit->blockSignals(true);
    m_BaseUnit->setCurrentText(ToQString(base_unit_name));
    m_BaseUnit->blockSignals(false);
}

void GlobalOptionsWidget::ColorCubeAdded()
{
    TRACY_AUTO_SCOPE();

    const auto has_color_cube{
        [this](const auto& color_cube_name)
        {
            for (int i = 0; i < m_ColorCube->count(); i++)
            {
                if (m_ColorCube->itemText(i).toStdString() == color_cube_name)
                {
                    return true;
                }
            }
            return false;
        }
    };

    for (const auto& color_cube_name : GetCubeNames())
    {
        if (!has_color_cube(color_cube_name))
        {
            m_ColorCube->addItem(ToQString(color_cube_name));
        }
    }
}

void GlobalOptionsWidget::StyleAdded()
{
    TRACY_AUTO_SCOPE();

    const auto has_style{
        [this](const auto& style_name)
        {
            for (int i = 0; i < m_Style->count(); i++)
            {
                if (m_Style->itemText(i).toStdString() == style_name)
                {
                    return true;
                }
            }
            return false;
        }
    };

    for (const auto& style_name : GetStyles())
    {
        if (!has_style(style_name))
        {
            m_Style->addItem(ToQString(style_name));
        }
    }
}

#include <widget_global_options.moc>
