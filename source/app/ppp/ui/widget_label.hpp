#pragma once

#include <span>
#include <string_view>

#include <magic_enum/magic_enum.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

#include <ppp/qt_util.hpp>
#include <ppp/util.hpp>

class WidgetWithLabel : public QWidget
{
  public:
    WidgetWithLabel(std::string_view label_text, QWidget* widget);

    QLabel* GetLabel() const;
    virtual QWidget* GetWidget() const;

  private:
    QLabel* m_Label;
    QWidget* m_Widget;
};

class ComboBoxWithLabel : public WidgetWithLabel
{
  public:
    ComboBoxWithLabel(std::string_view label_text,
                      std::span<const std::string> options,
                      std::string_view default_option);
    ComboBoxWithLabel(std::string_view label_text,
                      std::span<const std::string_view> options,
                      std::string_view default_option);
    ComboBoxWithLabel(std::string_view label_text,
                      std::span<const std::string> options,
                      std::span<const std::string> tooltips,
                      std::string_view default_option);

    template<Enum T>
    ComboBoxWithLabel(std::string_view label_text,
                      T default_option)
        : ComboBoxWithLabel{
            label_text,
            magic_enum::enum_names<T>(),
            magic_enum::enum_name(default_option)
        }
    {
    }

    virtual QComboBox* GetWidget() const override;
};

class LineEditWithLabel : public WidgetWithLabel
{
  public:
    LineEditWithLabel(std::string_view label_text, std::string_view default_text);

    virtual QLineEdit* GetWidget() const override;
};

class LabelWithLabel : public WidgetWithLabel
{
  public:
    LabelWithLabel(std::string_view label_text, std::string_view other_text);

    virtual QLabel* GetWidget() const override;
};

class DoubleSpinBoxWithLabel : public WidgetWithLabel
{
  public:
    DoubleSpinBoxWithLabel(std::string_view label_text);

    virtual QDoubleSpinBox* GetWidget() const override;
};
