#pragma once

#include <span>
#include <string_view>

#include <QComboBox>
#include <QLineEdit>
#include <QWidget>

class WidgetWithLabel : public QWidget
{
  public:
    WidgetWithLabel(std::string_view label_text, QWidget* widget);

    virtual QWidget* GetWidget() const;

  private:
    QWidget* Widget;
};

class ComboBoxWithLabel : public WidgetWithLabel
{
  public:
    ComboBoxWithLabel(std::string_view label_text, std::span<const std::string_view> options, std::string_view default_option);

    virtual QComboBox* GetWidget() const override;
};

class LineEditWithLabel : public WidgetWithLabel
{
  public:
    LineEditWithLabel(std::string_view label_text, std::string_view default_text);

    virtual QLineEdit* GetWidget() const override;
};
