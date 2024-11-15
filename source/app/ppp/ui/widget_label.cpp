#include <ppp/ui/widget_label.hpp>

#include <QHBoxLayout>
#include <QLabel>

WidgetWithLabel::WidgetWithLabel(std::string_view label_text, QWidget* widget)
    : QWidget{ nullptr }
    , Widget{ widget }
{
    auto* label{ new QLabel(QString::fromStdString(std::string{ label_text }) + ":") };
    if (label_text.contains("&"))
    {
        label->setBuddy(widget);
    }

    auto* layout{ new QHBoxLayout };
    layout->addWidget(label);
    layout->addWidget(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);
}

QWidget* WidgetWithLabel::GetWidget() const
{
    return Widget;
}

ComboBoxWithLabel::ComboBoxWithLabel(std::string_view label_text, std::span<std::string_view> options, std::string_view default_option)
    : WidgetWithLabel(label_text, new QLineEdit)
{
    for (const auto& option : options)
    {
        GetWidget()->addItem(QString::fromStdString(std::string{ option }));
    }

    if (std::ranges::contains(options, default_option))
    {
        GetWidget()->setCurrentText(QString::fromStdString(std::string{ default_option }));
    }
}

QComboBox* ComboBoxWithLabel::GetWidget() const
{
    return static_cast<QComboBox*>(WidgetWithLabel::GetWidget());
}

LineEditWithLabel::LineEditWithLabel(std::string_view label_text, std::string_view default_text)
    : WidgetWithLabel(label_text, new QLineEdit{ QString::fromStdString(std::string{ default_text }) })
{
}

QLineEdit* LineEditWithLabel::GetWidget() const
{
    return static_cast<QLineEdit*>(WidgetWithLabel::GetWidget());
}
