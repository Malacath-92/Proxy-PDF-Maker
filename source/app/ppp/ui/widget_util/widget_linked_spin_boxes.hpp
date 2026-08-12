#pragma once

#include <QIcon>
#include <QWidget>

#include <ppp/units.hpp>

class LengthSpinBox;
class QToolButton;

class LinkedSpinBoxes : public QWidget
{
    Q_OBJECT

  public:
    LinkedSpinBoxes(bool initially_linked,
                    Unit base_unit);

    LengthSpinBox* First();
    LengthSpinBox* Second();

    void SetLinked(bool linked);

  signals:
    void LinkChanged(bool linked);
    void Linked();
    void UnLinked();

  private:
    QIcon m_LinkedIcon;
    QIcon m_UnLinkedIcon;

    LengthSpinBox* m_First;
    LengthSpinBox* m_Second;
    QToolButton* m_ChainButton;
};
