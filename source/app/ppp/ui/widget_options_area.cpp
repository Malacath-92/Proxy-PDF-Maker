#include <ppp/ui/widget_options_area.hpp>

#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/plugins.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/collapse_button.hpp>

OptionsAreaWidget::OptionsAreaWidget(const PrintProxyPrepApplication& app,
                                     Project& project,
                                     QWidget* actions,
                                     QWidget* print_options,
                                     QWidget* guides_options,
                                     QWidget* card_options,
                                     QWidget* global_options)
    : m_App{ app }
    , m_Project{ project }
{
    auto* layout{ new QVBoxLayout };
    layout->addWidget(actions);
    layout->addStretch();
    AddCollapsible(layout, print_options);
    AddCollapsible(layout, guides_options);
    AddCollapsible(layout, card_options);
    AddCollapsible(layout, global_options);
    for (const auto& plugin_name : GetPluginNames())
    {
        if (g_Cfg.m_PluginsState[std::string{ plugin_name }])
        {
            auto* plugin_widget{ InitPlugin(plugin_name, project) };
            m_PluginWidgets[plugin_name] = plugin_widget;
            AddCollapsible(layout, plugin_widget);
        }
    }

    auto* widget{ new QWidget };
    widget->setLayout(layout);
    widget->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::MinimumExpanding);

    setWidget(widget);
    setWidgetResizable(true);
    setMinimumHeight(400);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
}

void OptionsAreaWidget::PluginEnabled(std::string_view plugin_name)
{
    if (!m_PluginWidgets.contains(plugin_name))
    {
        auto* plugin_widget{ InitPlugin(plugin_name, m_Project) };
        m_PluginWidgets[plugin_name] = plugin_widget;

        auto* layout{ static_cast<QVBoxLayout*>(widget()->layout()) };
        AddCollapsible(layout, plugin_widget);
    }
}

void OptionsAreaWidget::PluginDisabled(std::string_view plugin_name)
{
    if (m_PluginWidgets.contains(plugin_name))
    {
        auto* plugin_widget{ m_PluginWidgets[plugin_name] };

        auto* layout{ static_cast<QVBoxLayout*>(widget()->layout()) };
        const auto plugin_widget_index{ layout->indexOf(plugin_widget) };
        if (plugin_widget_index >= 0)
        {
            if (auto* collapse_button{ layout->itemAt(plugin_widget_index - 1)->widget() })
            {
                layout->removeWidget(collapse_button);
                delete collapse_button;
            }
            layout->removeWidget(plugin_widget);
        }

        m_PluginWidgets.erase(plugin_name);
        DestroyPlugin(plugin_name, plugin_widget);
    }
}

void OptionsAreaWidget::AddCollapsible(QVBoxLayout* layout, QWidget* widget)
{
    auto* collapse_button{ new CollapseButton{ widget, !m_App.GetObjectVisibility(widget->objectName()) } };
    layout->insertWidget(layout->count() - 1, collapse_button);
    layout->insertWidget(layout->count() - 1, widget);

    QObject::connect(collapse_button,
                     &CollapseButton::SetObjectVisibility,
                     this,
                     [this, widget](bool visible)
                     {
                         SetObjectVisibility(widget->objectName(), visible);
                     });
}
