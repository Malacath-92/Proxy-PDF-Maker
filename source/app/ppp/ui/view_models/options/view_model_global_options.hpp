#pragma once

#include <QWidget>

#include <ppp/config.hpp>

class GlobalOptionsViewModel : public QObject
{
    Q_OBJECT

    friend class GlobalOptionsWidget;

  public:
    GlobalOptionsViewModel(Config& config);

    void Init();

  signals:
    // forward
    void AdvancedModeChanged(bool advanced_mode);

    void NoCropModeChanged(bool no_crop_mode);

    void CheckVersionOnStartupChanged(bool check_version);
    void ToastTimeoutMSChanged(uint32_t toast_timeout_ms);

    void BasePreviewWidthChanged(Pixel base_preview_width);
    void MaxDPIChanged(PixelDensity max_dpi);

    void CardOrderChanged(CardOrder card_order);
    void CardOrderDirectionChanged(CardOrderDirection card_order_direction);

    void MaxWorkerThreadsChanged(uint32_t max_worker_threads);

    void DisplayColumnsChanged(uint32_t display_columns);
    void MaxDisplayColumnsChanged(uint32_t max_display_columns);

    void ColorCubeChanged(std::string_view color_cube);

    void VersionOutputChanged(bool version_output);

    void PdfBackendChanged(PdfBackend pdf_backend);
    void ImageCompressionChanged(ImageCompression compression);
    void PngCompressionChanged(std::optional<int> png_compression);
    void JpgQualityChanged(std::optional<int> jpg_quality);

    void BaseUnitChanged(Unit base_unit);

    void OpenPluginsWindow();

    void ColorCubeAdded();
    void StyleAdded();

  private slots:
    void ChangeAdvancedMode(Qt::CheckState advanced_mode);

    void ChangeNoCropMode(Qt::CheckState no_crop_mode);

    void ChangeCheckVersionOnStartup(Qt::CheckState check_version);
    void ChangeToastTimeoutMS(double toast_timeout_ms);

    void ChangeBasePreviewWidth(double base_preview_width);
    void ChangeMaxDPI(double max_dpi);

    void ChangeCardOrder(const QString& card_order);
    void ChangeCardOrderDirection(const QString& card_order_direction);

    void ChangeMaxWorkerThreads(double max_worker_threads);

    void ChangeDisplayColumns(double display_columns);

    void ChangeColorCube(const QString& color_cube);

    void ChangeVersionOutput(Qt::CheckState version_output);

    void ChangePdfBackend(PdfBackend pdf_backend);
    void ChangeImageCompression(const QString& compression);
    void ChangePngCompression(double png_compression);
    void ChangeJpgQuality(double jpg_quality);

    void ChangeBaseUnit(const QString& base_unit);

    void ChangeStyle(const QString& style);

    void EnablePlugin(std::string_view plugin_name);
    void DisablePlugin(std::string_view plugin_name);

  private:
    void EmitDefaults();

    const std::unordered_map<std::string, bool>& GetPluginsState() const;

    Config& m_Cfg;
};
