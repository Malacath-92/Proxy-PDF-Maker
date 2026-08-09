#include <ppp/ui/view_models/options/view_model_global_options.hpp>

#include <ranges>

#include <magic_enum/magic_enum.hpp>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/cubes.hpp>
#include <ppp/plugins.hpp>
#include <ppp/qt_util.hpp>
#include <ppp/style.hpp>

#include <ppp/profile/profile.hpp>

GlobalOptionsViewModel::GlobalOptionsViewModel(Config& config)
    : m_Cfg{ config }
{
}

void GlobalOptionsViewModel::ChangeAdvancedMode(Qt::CheckState advanced_mode)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetAdvancedMode(advanced_mode != Qt::CheckState::Unchecked);
}

void GlobalOptionsViewModel::ChangeNoCropMode(Qt::CheckState no_crop_mode)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetNoCropMode(no_crop_mode != Qt::CheckState::Unchecked);
}

void GlobalOptionsViewModel::ChangeCheckVersionOnStartup(Qt::CheckState check_version)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetCheckVersionOnStartup(check_version != Qt::CheckState::Unchecked);
}
void GlobalOptionsViewModel::ChangeToastTimeoutMS(double toast_timeout_ms)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetToastTimeoutMS(static_cast<uint32_t>(toast_timeout_ms));
}

void GlobalOptionsViewModel::ChangeBasePreviewWidth(double base_preview_width)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetBasePreviewWidth(static_cast<float>(base_preview_width) * 1_pix);
}
void GlobalOptionsViewModel::ChangeMaxDPI(double max_dpi)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetMaxDPI(static_cast<float>(max_dpi) * 1_dpi);
}

void GlobalOptionsViewModel::ChangeCardOrder(const QString& card_order)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetCardOrder(
        magic_enum::enum_cast<CardOrder>(card_order.toStdString())
            .value_or(CardOrder::Alphabetical));
}
void GlobalOptionsViewModel::ChangeCardOrderDirection(const QString& card_order_direction)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetCardOrderDirection(
        magic_enum::enum_cast<CardOrderDirection>(card_order_direction.toStdString())
            .value_or(CardOrderDirection::Ascending));
}

void GlobalOptionsViewModel::ChangeMaxWorkerThreads(double max_worker_threads)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetMaxWorkerThreads(max_worker_threads);
}

void GlobalOptionsViewModel::ChangeDisplayColumns(double display_columns)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetDisplayColumns(static_cast<uint32_t>(display_columns));
}

void GlobalOptionsViewModel::ChangeColorCube(const QString& color_cube)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetColorCube(color_cube.toStdString());
}

void GlobalOptionsViewModel::ChangeVersionOutput(Qt::CheckState version_output)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetVersionOutput(version_output != Qt::CheckState::Unchecked);
}

void GlobalOptionsViewModel::ChangePdfBackend(PdfBackend pdf_backend)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetPdfBackend(pdf_backend);
}
void GlobalOptionsViewModel::ChangeImageCompression(const QString& compression)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetImageCompression(
        magic_enum::enum_cast<ImageCompression>(compression.toStdString())
            .value_or(ImageCompression::Lossy));
}
void GlobalOptionsViewModel::ChangePngCompression(double png_compression)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetPngCompression(static_cast<int>(png_compression));
}
void GlobalOptionsViewModel::ChangeJpgQuality(double jpg_quality)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetJpgQuality(static_cast<int>(jpg_quality));
}

void GlobalOptionsViewModel::ChangeBaseUnit(const QString& base_unit)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.SetBaseUnit(UnitFromName(base_unit.toStdString())
                          .value_or(Unit::Inches));
}

void GlobalOptionsViewModel::ChangeStyle(const QString& style)
{
    TRACY_AUTO_SCOPE();

    auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
    application.SetTheme(style.toStdString());
    SetStyle(application.GetTheme());
}

void GlobalOptionsViewModel::EnablePlugin(std::string_view plugin_name)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.EnablePlugin(std::string{ plugin_name });
}
void GlobalOptionsViewModel::DisablePlugin(std::string_view plugin_name)
{
    TRACY_AUTO_SCOPE();
    m_Cfg.DisablePlugin(std::string{ plugin_name });
}

const std::unordered_map<std::string, bool>& GlobalOptionsViewModel::GetPluginsState() const
{
    return m_Cfg.m_PluginsState;
}

void GlobalOptionsViewModel::EmitDefaults()
{
    TRACY_AUTO_SCOPE();

    AdvancedModeChanged(m_Cfg.m_AdvancedMode);

    NoCropModeChanged(m_Cfg.m_NoCropMode);

    CheckVersionOnStartupChanged(m_Cfg.m_CheckVersionOnStartup);
    ToastTimeoutMSChanged(m_Cfg.m_ToastTimeoutMS);

    BasePreviewWidthChanged(m_Cfg.m_BasePreviewWidth);
    MaxDPIChanged(m_Cfg.m_MaxDPI);

    CardOrderChanged(m_Cfg.m_CardOrder);
    CardOrderDirectionChanged(m_Cfg.m_CardOrderDirection);

    MaxWorkerThreadsChanged(m_Cfg.m_MaxWorkerThreads);

    DisplayColumnsChanged(m_Cfg.m_DisplayColumns);
    MaxDisplayColumnsChanged(m_Cfg.m_MaxDisplayColumns);

    ColorCubeChanged(m_Cfg.m_ColorCube);

    VersionOutputChanged(m_Cfg.m_VersionOutput);

    PdfBackendChanged(m_Cfg.m_Backend);
    ImageCompressionChanged(m_Cfg.m_PdfImageCompression);
    PngCompressionChanged(m_Cfg.m_PngCompression);
    JpgQualityChanged(m_Cfg.m_JpgQuality);

    BaseUnitChanged(m_Cfg.m_BaseUnit);
}
