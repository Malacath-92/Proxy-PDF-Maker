#include <ppp/ui/widget_label.hpp>

#include <QHBoxLayout>
#include <QLabel>

WidgetWithLabel::WidgetWithLabel(std::string_view label_text, QWidget* widget)
    : QWidget{ nullptr }
    , Widget{ widget }
{
    auto* label{ new QLabel(QString::fromLatin1(label_text) + ":") };
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

ComboBoxWithLabel::ComboBoxWithLabel(std::string_view label_text, std::span<const std::string_view> options, std::string_view default_option)
    : WidgetWithLabel(label_text, new QComboBox)
{
    for (const auto& option : options)
    {
        GetWidget()->addItem(QString::fromLatin1(option));
    }

    if (std::ranges::contains(options, default_option))
    {
        GetWidget()->setCurrentText(QString::fromLatin1(default_option));
    }
}

QComboBox* ComboBoxWithLabel::GetWidget() const
{
    return static_cast<QComboBox*>(WidgetWithLabel::GetWidget());
}

LineEditWithLabel::LineEditWithLabel(std::string_view label_text, std::string_view default_text)
    : WidgetWithLabel(label_text, new QLineEdit{ QString::fromLatin1(default_text) })
{
}

QLineEdit* LineEditWithLabel::GetWidget() const
{
    return static_cast<QLineEdit*>(WidgetWithLabel::GetWidget());
}
