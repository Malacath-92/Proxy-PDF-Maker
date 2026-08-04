#include <ppp/ui/options/widget_options_area.hpp>

#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <ppp/app.hpp>
#include <ppp/config.hpp>
#include <ppp/plugins.hpp>
#include <ppp/plugins/plugin_interface.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/widget_util/collapse_button.hpp>

#include <ppp/profile/profile.hpp>

OptionsAreaWidget::OptionsAreaWidget(
    Project& project,
    const Config& config,
    PluginInterface& plugin_router,
    QWidget* project_options,
    QWidget* print_options,
    QWidget* guides_options,
    QWidget* card_options,
    QWidget* global_options)
    : m_Project{ project }
    , m_Cfg{ config }
    , m_PluginRouter{ plugin_router }
{
    TRACY_AUTO_SCOPE();

    auto* layout{ new QVBoxLayout };
    AddCollapsible(layout, project_options);
    AddCollapsible(layout, print_options);
    AddCollapsible(layout, guides_options);
    AddCollapsible(layout, card_options);
    AddCollapsible(layout, global_options);
    layout->addStretch();
    layout->setContentsMargins(0, 0, 0, 0);

    auto* widget{ new QWidget };
    widget->setLayout(layout);
    widget->setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::MinimumExpanding);

    setWidget(widget);
    setWidgetResizable(true);
    setMinimumHeight(400);
    setSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
    setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    for (const auto& plugin_name : GetPluginNames())
    {
        const auto it{ m_Cfg.m_PluginsState.find(std::string{ plugin_name }) };
        if (it != m_Cfg.m_PluginsState.end() && it->second)
        {
            PluginEnabled(plugin_name);
        }
    }
}

void OptionsAreaWidget::PluginEnabled(std::string_view plugin_name)
{
    TRACY_AUTO_SCOPE();

    if (!m_Plugins.contains(plugin_name))
    {
        auto* plugin{ InitPlugin(plugin_name, m_Project, m_Cfg) };
        m_Plugins[plugin_name] = plugin;

        auto* layout{ static_cast<QVBoxLayout*>(widget()->layout()) };
        AddCollapsible(layout, plugin->Widget());

        m_PluginRouter.Route(*plugin);
    }
}

void OptionsAreaWidget::PluginDisabled(std::string_view plugin_name)
{
    TRACY_AUTO_SCOPE();

    if (m_Plugins.contains(plugin_name))
    {
        auto* plugin{ m_Plugins[plugin_name] };
        auto* plugin_widget{ plugin->Widget() };

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

        m_Plugins.erase(plugin_name);
        DestroyPlugin(plugin_name, plugin);
    }
}

void OptionsAreaWidget::AddCollapsible(QVBoxLayout* layout, QWidget* widget)
{
    TRACY_AUTO_SCOPE();

    auto& application{ *static_cast<PrintProxyPrepApplication*>(qApp) };
    auto* collapse_button{ new CollapseButton{
        widget,
        !application.GetObjectVisibility(widget->objectName()),
    } };
    layout->addWidget(collapse_button);
    layout->addWidget(widget);

    QObject::connect(collapse_button,
                     &CollapseButton::SetObjectVisibility,
                     this,
                     [this, widget](bool visible)
                     {
                         SetObjectVisibility(widget->objectName(), visible);
                     });
}
