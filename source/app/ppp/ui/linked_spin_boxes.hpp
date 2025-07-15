#pragma once

#include <QIcon>
#include <QWidget>

class QDoubleSpinBox;
class QToolButton;

class LinkedSpinBoxes : public QWidget
{
    Q_OBJECT

  public:
    LinkedSpinBoxes(bool initially_linked);

    QDoubleSpinBox* First();
    QDoubleSpinBox* Second();

  signals:
    void Linked();
    void UnLinked();

  private:
    QIcon m_LinkedIcon;
    QIcon m_UnLinkedIcon;

    QDoubleSpinBox* m_First;
    QDoubleSpinBox* m_Second;
    QToolButton* m_ChainButton;
};
