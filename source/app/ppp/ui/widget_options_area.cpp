#include <ppp/ui/widget_options_area.hpp>

#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <ppp/app.hpp>
#include <ppp/qt_util.hpp>

#include <ppp/ui/collapse_button.hpp>

OptionsAreaWidget::OptionsAreaWidget(const PrintProxyPrepApplication& app,
                                     QWidget* actions,
                                     QWidget* print_options,
                                     QWidget* guides_options,
                                     QWidget* card_options,
                                     QWidget* global_options)
{
    const auto add_collapsable{
        [this, &app](QVBoxLayout* layout, QWidget* widget)
        {
            auto* collapse_button{ new CollapseButton{ widget, !app.GetObjectVisibility(widget->objectName()) } };
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
    };

    auto* layout{ new QVBoxLayout };
    layout->addWidget(actions);
    add_collapsable(layout, print_options);
    add_collapsable(layout, guides_options);
    add_collapsable(layout, card_options);
    add_collapsable(layout, global_options);
    layout->addStretch();

    auto* widget{ new QWidget };
    widget->setLayout(layout);
    widget->setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::MinimumExpanding);

    setWidget(widget);
    setWidgetResizable(true);
    setMinimumHeight(400);
    setSizePolicy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Expanding);
    setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
}