#pragma once

#include <QIcon>
#include <QWidget>

class LengthSpinBox;
class QToolButton;

class LinkedSpinBoxes : public QWidget
{
    Q_OBJECT

  public:
    LinkedSpinBoxes(bool initially_linked);

    LengthSpinBox* First();
    LengthSpinBox* Second();

  signals:
    void Linked();
    void UnLinked();

  private:
    QIcon m_LinkedIcon;
    QIcon m_UnLinkedIcon;

    LengthSpinBox* m_First;
    LengthSpinBox* m_Second;
    QToolButton* m_ChainButton;
};
