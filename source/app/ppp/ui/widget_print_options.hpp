#pragma once

#include <QWidget>

#include <ppp/util.hpp>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

class Project;
class WidgetWithLabel;

class PrintOptionsWidget : public QWidget
{
  public:
    PrintOptionsWidget(Project& project);

  public slots:
    void NewProjectOpened();
    void PageSizeChanged();
    void BleedChanged();
    void BaseUnitChanged();
    void RenderBackendChanged();

  private:
    void RefreshSizes();

    static std::vector<std::string> GetBasePdfNames();
    static std::string SizeToString(Size size);

    Project& m_Project;

    QLineEdit* m_PrintOutput{ nullptr };
    QComboBox* m_PaperSize{ nullptr };
    QComboBox* m_Orientation{ nullptr };
    QLabel* m_PaperInfo{ nullptr };
    QLabel* m_CardsInfo{ nullptr };
    QWidget* m_BasePdf{ nullptr };
    QDoubleSpinBox* m_LeftMarginSpin{ nullptr };
    QDoubleSpinBox* m_TopMarginSpin{ nullptr };
};
