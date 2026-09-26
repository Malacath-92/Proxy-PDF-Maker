#include <ppp/ui/widget_util/widget_collapse_container.hpp>

#include <QVBoxLayout>

#include <ppp/ui/widget_util/widget_collapse_button.hpp>

CollapseContainer::CollapseContainer(QWidget* handled_widget, bool collapsed)
    : m_HandledWidget{ handled_widget }
{
    m_CollapseButton = new CollapseButton{ handled_widget, collapsed };
    QObject::connect(m_CollapseButton,
                     &CollapseButton::SetObjectVisibility,
                     this,
                     &CollapseContainer::SetObjectVisibility);

    auto* layout{ new QVBoxLayout };
    layout->addWidget(m_CollapseButton);
    layout->addItem(m_CollapseButton->GetLayoutItem());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setLayout(layout);
    setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Maximum);
}

QWidget* CollapseContainer::GetHandledWidget() const
{
    return m_HandledWidget;
}
void CollapseContainer::ReleaseHandledWidget()
{
    layout()->removeWidget(m_HandledWidget);
    m_HandledWidget->setParent(nullptr);
    m_HandledWidget = nullptr;
}
