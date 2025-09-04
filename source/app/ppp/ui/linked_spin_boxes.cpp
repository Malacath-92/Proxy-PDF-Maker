#include <ppp/ui/linked_spin_boxes.hpp>

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <ppp/ui/widget_double_spin_box.hpp>

LinkedSpinBoxes::LinkedSpinBoxes(bool initially_linked)
{
    m_LinkedIcon = QIcon{ QPixmap{ ":/res/linked.png" } };
    m_UnLinkedIcon = QIcon{ QPixmap{ ":/res/unlinked.png" } };

    m_First = MakeDoubleSpinBox();

    m_Second = MakeDoubleSpinBox();
    m_Second->setEnabled(!initially_linked);

    auto* inner_layout{ new QVBoxLayout };
    inner_layout->addWidget(m_First);
    inner_layout->addWidget(m_Second);
    inner_layout->setContentsMargins(0, 0, 0, 0);

    auto* inner_widget{ new QWidget };
    inner_widget->setLayout(inner_layout);

    m_ChainButton = new QToolButton;
    m_ChainButton->setIcon(initially_linked ? m_LinkedIcon : m_UnLinkedIcon);
    m_ChainButton->setToolButtonStyle(Qt::ToolButtonStyle::ToolButtonIconOnly);
    m_ChainButton->setCheckable(true);
    m_ChainButton->setChecked(initially_linked);
    m_ChainButton->setStyleSheet("background:none; border:none");
    m_ChainButton->setToolTip("Link horizontal and vertical spacing.");

    auto* outer_layout{ new QHBoxLayout };
    outer_layout->addWidget(m_ChainButton);
    outer_layout->addWidget(inner_widget);
    outer_layout->setContentsMargins(0, 0, 0, 0);

    setLayout(outer_layout);

    QObject::connect(m_ChainButton,
                     &QToolButton::toggled,
                     [this](bool checked)
                     {
                         m_Second->setEnabled(!checked);

                         if (checked)
                         {
                             m_ChainButton->setIcon(m_LinkedIcon);
                             m_Second->setValue(m_First->value());

                             Linked();
                         }
                         else
                         {
                             m_ChainButton->setIcon(m_UnLinkedIcon);

                             UnLinked();
                         }
                     });
    QObject::connect(m_First,
                     &QDoubleSpinBox::valueChanged,
                     [this](double value)
                     {
                         if (!m_Second->isEnabled())
                         {
                             m_Second->setValue(value);
                         }
                     });
}

QDoubleSpinBox* LinkedSpinBoxes::First()
{
    return m_First;
}

QDoubleSpinBox* LinkedSpinBoxes::Second()
{
    return m_Second;
}
