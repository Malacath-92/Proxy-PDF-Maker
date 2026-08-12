#include <ppp/ui/widget_util/widget_double_spin_box.hpp>

#include <QWheelEvent>

#include <ppp/qt_util.hpp>

LengthSpinBox::LengthSpinBox(Unit base_unit)
    : m_Unit{ base_unit }
{
    const auto base_unit_name{ ToQString(UnitShortName(m_Unit)) };
    setSuffix(base_unit_name);

    QObject::connect(this,
                     &QDoubleSpinBox::valueChanged,
                     this,
                     [this](double v)
                     {
                         const auto base_unit{ UnitValue(m_Unit) };
                         ValueChanged(static_cast<float>(v) * base_unit);
                     });
}

void LengthSpinBox::SetRange(Length min, Length max)
{
    const auto base_unit{ UnitValue(m_Unit) };
    setRange(min / base_unit, max / base_unit);
}

void LengthSpinBox::SetValue(Length v)
{
    const auto base_unit{ UnitValue(m_Unit) };
    setValue(v / base_unit);
}

Length LengthSpinBox::Value() const
{
    const auto base_unit{ UnitValue(m_Unit) };
    return value() * base_unit;
}

void LengthSpinBox::BaseUnitChanged(Unit new_base_unit)
{
    const auto prev_base_unit{ UnitValue(m_Unit) };
    const auto base_unit{ UnitValue(new_base_unit) };
    const auto ratio{ prev_base_unit / base_unit };

    const auto new_minimum{ minimum() * ratio };
    const auto new_maximum{ maximum() * ratio };
    const auto new_value{ value() * ratio };

    blockSignals(true);
    setRange(new_minimum, new_maximum);
    setValue(new_value);
    blockSignals(false);

    const auto base_unit_name{ ToQString(UnitShortName(new_base_unit)) };
    setSuffix(base_unit_name);

    m_Unit = new_base_unit;
}

template<class SpinBoxBase>
class WheelOnFocusOnlyDoubleSpinBox : public SpinBoxBase
{
  public:
    using SpinBoxBase::SpinBoxBase;

    void wheelEvent(QWheelEvent* e)
    {
        if (SpinBoxBase::hasFocus())
        {
            SpinBoxBase::wheelEvent(e);
        }
        else
        {
            e->ignore();
        }
    }
};

LengthSpinBox* MakeLengthSpinBox(Unit base_unit)
{
    auto* double_spin_box{
        new WheelOnFocusOnlyDoubleSpinBox<LengthSpinBox>{ base_unit }
    };
    double_spin_box->setFocusPolicy(Qt::StrongFocus);
    return double_spin_box;
}

QDoubleSpinBox* MakeDoubleSpinBox()
{
    auto* double_spin_box{ new WheelOnFocusOnlyDoubleSpinBox<QDoubleSpinBox> };
    double_spin_box->setFocusPolicy(Qt::StrongFocus);
    return double_spin_box;
}
