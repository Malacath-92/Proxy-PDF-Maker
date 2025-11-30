#include <ppp/ui/default_project_value_actions.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QWidget>

#include <nlohmann/json.hpp>

#include <ppp/project/project.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/app.hpp>

#include <ppp/ui/widget_double_spin_box.hpp>

void ResetImpl(LengthSpinBox* widget, const nlohmann::json& value)
{
    if (value.is_number())
    {
        widget->SetValue(value.get<float>() * 1_cm);
    }
}
void ResetImpl(QCheckBox* widget, const nlohmann::json& value)
{
    if (value.is_boolean())
    {
        widget->setChecked(value);
    }
}
void ResetImpl(QComboBox* widget, const nlohmann::json& value)
{
    if (value.is_string())
    {
        widget->setCurrentText(ToQString(value.get_ref<const std::string&>()));
    }
}
void ResetImpl(QDoubleSpinBox* widget, const nlohmann::json& value)
{
    if (value.is_number())
    {
        widget->setValue(value.get<float>());
    }
}
void ResetImpl(QLineEdit* widget, const nlohmann::json& value)
{
    if (value.is_string())
    {
        widget->setText(ToQString(value.get_ref<const std::string&>()));
    }
}

nlohmann::json GetValueImpl(LengthSpinBox* widget)
{
    return widget->Value() / 1_cm;
}
nlohmann::json GetValueImpl(QCheckBox* widget)
{
    return widget->isChecked();
}
nlohmann::json GetValueImpl(QComboBox* widget)
{
    return widget->currentText().toStdString();
}
nlohmann::json GetValueImpl(QDoubleSpinBox* widget)
{
    return widget->value();
}
nlohmann::json GetValueImpl(QLineEdit* widget)
{
    return widget->text().toStdString();
}

void ResetToDefault(QWidget* widget, std::string_view path)
{
    const auto value{
        [path]()
        {
            const auto* app{ static_cast<const PrintProxyPrepApplication*>(qApp) };
            const auto user_default_value{ app->GetProjectDefault(path) };
            if (!user_default_value.is_null())
            {
                return user_default_value;
            }
            else
            {
                // TODO: Get json-value of app-default per-value rather than the whole project
                const auto app_default_json{ nlohmann::json::parse(Project{}.DumpToJson()) };
                const auto app_default_value{ GetJsonValue(app_default_json, path) };
                return app_default_value;
            }
        }()
    };

#define CALL_FOR_TYPE(T)                               \
    if (auto* cast_widget{ dynamic_cast<T*>(widget) }) \
    {                                                  \
        ResetImpl(cast_widget, value);                 \
        return;                                        \
    }

    CALL_FOR_TYPE(LengthSpinBox);
    CALL_FOR_TYPE(QCheckBox);
    CALL_FOR_TYPE(QComboBox);
    CALL_FOR_TYPE(QDoubleSpinBox);
    CALL_FOR_TYPE(QLineEdit);

#undef CALL_FOR_TYPE
}
void SetAsDefault(QWidget* widget, std::string_view path)
{
    const auto value{
        [widget]()
        {
#define CALL_FOR_TYPE(T)                               \
    if (auto* cast_widget{ dynamic_cast<T*>(widget) }) \
    {                                                  \
        return GetValueImpl(cast_widget);              \
    }
            CALL_FOR_TYPE(LengthSpinBox);
            CALL_FOR_TYPE(QCheckBox);
            CALL_FOR_TYPE(QComboBox);
            CALL_FOR_TYPE(QDoubleSpinBox);
            CALL_FOR_TYPE(QLineEdit);

#undef CALL_FOR_TYPE

            return nlohmann::json{};
        }()
    };

    auto* app{ static_cast<PrintProxyPrepApplication*>(qApp) };
    app->SetProjectDefault(path, value);
}

void EnableOptionWidgetForDefaults(QWidget* widget, std::string_view path)
{
    QAction* reset_to_default_action{ new QAction{ "Reset to Default", widget } };
    QAction* set_as_default_action{ new QAction{ "Set as Default", widget } };

    widget->addAction(reset_to_default_action);
    widget->addAction(set_as_default_action);
    widget->setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);

    QObject::connect(reset_to_default_action,
                     &QAction::triggered,
                     widget,
                     std::bind_front(&ResetToDefault, widget, path));
    QObject::connect(set_as_default_action,
                     &QAction::triggered,
                     widget,
                     std::bind_front(&SetAsDefault, widget, path));
}
