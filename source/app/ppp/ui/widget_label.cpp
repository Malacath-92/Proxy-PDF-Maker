#include <ppp/ui/widget_label.hpp>

#include <QHBoxLayout>
#include <QLabel>

#include <ppp/qt_util.hpp>

WidgetWithLabel::WidgetWithLabel(std::string_view label_text, QWidget* widget)
    : QWidget{ nullptr }
    , m_Widget{ widget }
{
    m_Label = new QLabel(label_text.empty() ? "" : ToQString(label_text) + ":");
    if (label_text.contains("&"))
    {
        m_Label->setBuddy(widget);
    }

    auto* layout{ new QHBoxLayout };
    layout->addWidget(m_Label);
    layout->addWidget(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);
}

QLabel* WidgetWithLabel::GetLabel() const
{
    return m_Label;
}

QWidget* WidgetWithLabel::GetWidget() const
{
    return m_Widget;
}

ComboBoxWithLabel::ComboBoxWithLabel(std::string_view label_text, std::span<const std::string> options, std::string_view default_option)
    : ComboBoxWithLabel(label_text, options, default_option, DelegatingTag{})
{
}

ComboBoxWithLabel::ComboBoxWithLabel(std::string_view label_text,
                                     std::span<const std::string> options,
                                     std::span<const std::string> tooltips,
                                     std::string_view default_option)
    : ComboBoxWithLabel(label_text, options, default_option, DelegatingTag{})
{
    for (size_t i = 0; i < tooltips.size(); i++)
    {
        if (!tooltips[i].empty())
        {
            ComboBoxWithLabel::GetWidget()->setItemData(
                static_cast<int>(i),
                ToQString(tooltips[i]),
                Qt::ToolTipRole);
        }
    }
}

ComboBoxWithLabel::ComboBoxWithLabel(std::string_view label_text, std::span<const std::string_view> options, std::string_view default_option)
    : ComboBoxWithLabel(label_text, options, default_option, DelegatingTag{})
{
}

template<class StringT>
ComboBoxWithLabel::ComboBoxWithLabel(std::string_view label_text, std::span<const StringT> options, std::string_view default_option, DelegatingTag)
    : WidgetWithLabel(label_text, new QComboBox)
{
    for (const auto& option : options)
    {
        ComboBoxWithLabel::GetWidget()->addItem(ToQString(option));
    }

    if (std::ranges::contains(options, default_option))
    {
        ComboBoxWithLabel::GetWidget()->setCurrentText(ToQString(default_option));
    }
}

QComboBox* ComboBoxWithLabel::GetWidget() const
{
    return static_cast<QComboBox*>(WidgetWithLabel::GetWidget());
}

LineEditWithLabel::LineEditWithLabel(std::string_view label_text, std::string_view default_text)
    : WidgetWithLabel(label_text, new QLineEdit{ ToQString(default_text) })
{
}

QLineEdit* LineEditWithLabel::GetWidget() const
{
    return static_cast<QLineEdit*>(WidgetWithLabel::GetWidget());
}

LabelWithLabel::LabelWithLabel(std::string_view label_text, std::string_view other_text)
    : WidgetWithLabel(label_text, new QLabel{ ToQString(other_text) })
{
}

QLabel* LabelWithLabel::GetWidget() const
{
    return static_cast<QLabel*>(WidgetWithLabel::GetWidget());
}

DoubleSpinBoxWithLabel::DoubleSpinBoxWithLabel(std::string_view label_text)
    : WidgetWithLabel(label_text, new QDoubleSpinBox)
{
}

QDoubleSpinBox* DoubleSpinBoxWithLabel::GetWidget() const
{
    return static_cast<QDoubleSpinBox*>(WidgetWithLabel::GetWidget());
}
