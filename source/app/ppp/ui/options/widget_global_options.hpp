#pragma once

#include <QWidget>

#include <ppp/config.hpp>

class QDoubleSpinBox;

class Project;
class ComboBoxWithLabel;

class GlobalOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GlobalOptionsWidget(Config& config);

  signals:
    void AdvancedModeChanged(bool advanced_mode);

    void BaseUnitChanged(Unit base_unit);
    void DisplayColumnsChanged(uint32_t display_columns);
    void RenderBackendChanged(PdfBackend backend);
    void ImageCompressionChanged();
    void JpgQualityChanged();
    void ColorCubeChanged();
    void NoCropModeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();
    void CardOrderChanged();
    void CardOrderDirectionChanged();
    void MaxWorkerThreadsChanged();

    void PluginEnabled(std::string_view plugin_name);
    void PluginDisabled(std::string_view plugin_name);

  public slots:
    void RequestOpenPluginsWindow();

    void PageSizesChanged();
    void CardSizesChanged();

    void MaximumDisplayColumnsChanged(uint32_t maximum_display_columns);

    void ColorCubeAdded();
    void StyleAdded();

  private:
    void OpenPluginsWindow();

    Config& m_Cfg;

    QDoubleSpinBox* m_DisplayColumns{ nullptr };
    ComboBoxWithLabel* m_ColorCube{ nullptr };
    ComboBoxWithLabel* m_Style{ nullptr };
};
