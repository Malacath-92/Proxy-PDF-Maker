#pragma once

#include <QWidget>

class Project;

class ComboBoxWithLabel;

class GlobalOptionsWidget : public QWidget
{
    Q_OBJECT

  public:
    GlobalOptionsWidget();

  signals:
    void AdvancedModeChanged();

    void BaseUnitChanged();
    void DisplayColumnsChanged();
    void RenderBackendChanged();
    void ImageCompressionChanged();
    void JpgQualityChanged();
    void ColorCubeChanged();
    void BasePreviewWidthChanged();
    void MaxDPIChanged();
    void CardOrderChanged();
    void CardOrderDirectionChanged();
    void MaxWorkerThreadsChanged();

    void PluginEnabled(std::string_view plugin_name);
    void PluginDisabled(std::string_view plugin_name);

  public slots:
    void PageSizesChanged();
    void CardSizesChanged();

    void ColorCubeAdded();
    void StyleAdded();

  private:
    ComboBoxWithLabel* m_ColorCube{ nullptr };
    ComboBoxWithLabel* m_Style{ nullptr };
};
