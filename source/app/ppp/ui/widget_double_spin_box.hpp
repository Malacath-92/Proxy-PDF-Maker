#pragma once

#include <QDoubleSpinBox>

#include <ppp/config.hpp>

class LengthSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

  public:
    LengthSpinBox();

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
    void BaseUnitChanged();

  private:
    using QDoubleSpinBox::setRange;
    using QDoubleSpinBox::setValue;
    using QDoubleSpinBox::setSuffix;

    Unit m_Unit;
};

LengthSpinBox* MakeLengthSpinBox();
QDoubleSpinBox* MakeDoubleSpinBox();
