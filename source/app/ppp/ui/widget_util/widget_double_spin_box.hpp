#pragma once

#include <QDoubleSpinBox>

#include <ppp/units.hpp>

class LengthSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

  public:
    LengthSpinBox(Unit base_unit);

    using QDoubleSpinBox::QDoubleSpinBox;

    template<class T>
    void ConnectUnitSignals(T* parent)
    {
        QObject::connect(parent,
                         &T::BaseUnitChanged,
                         this,
                         &LengthSpinBox::BaseUnitChanged);
    }

    void SetRange(Length min, Length max);
    void SetValue(Length v);

    Length Value() const;

  signals:
    void ValueChanged(Length v);

  public slots:
    void BaseUnitChanged(Unit new_base_unit);

  private:
    using QDoubleSpinBox::setRange;
    using QDoubleSpinBox::setSuffix;
    using QDoubleSpinBox::setValue;

    Unit m_Unit;
};

LengthSpinBox* MakeLengthSpinBox(Unit base_unit);
QDoubleSpinBox* MakeDoubleSpinBox();
