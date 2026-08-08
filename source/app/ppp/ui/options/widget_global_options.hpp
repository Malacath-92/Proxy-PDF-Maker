#pragma once

#include <QWidget>

#include <ppp/config_types.hpp>
#include <ppp/units.hpp>
#include <ppp/util.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;

class GlobalOptionsViewModel;

class GlobalOptionsWidget : public QWidget
{
    Q_OBJECT

    friend class GlobalOptionsViewModel;

  public:
    GlobalOptionsWidget(GlobalOptionsViewModel* view_model);

  private slots:
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

  private:
    GlobalOptionsViewModel& m_ViewModel;

    QCheckBox* m_AdvancedMode{ nullptr };

    QComboBox* m_BaseUnit{ nullptr };

    QDoubleSpinBox* m_DisplayColumns{ nullptr };

    QCheckBox* m_VersionOutput{ nullptr };

    QCheckBox* m_RenderToPng{ nullptr };
    QComboBox* m_ImageFormat{ nullptr };
    QDoubleSpinBox* m_JpgQuality{ nullptr };

    QComboBox* m_ColorCube{ nullptr };

    QDoubleSpinBox* m_PreviewWidth{ nullptr };

    QCheckBox* m_NoCropMode{ nullptr };

    QDoubleSpinBox* m_MaxDPI{ nullptr };

    QComboBox* m_CardOrder{ nullptr };
    QComboBox* m_CardOrderDirection{ nullptr };

    QDoubleSpinBox* m_MaxWorkerThreads{ nullptr };

    QComboBox* m_Style{ nullptr };

    QCheckBox* m_CheckVersion{ nullptr };
    QDoubleSpinBox* m_ToastTimeout{ nullptr };
};
