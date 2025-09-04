#include <ppp/ui/widget_double_spin_box.hpp>

#include <QWheelEvent>

class WheelOnFocusOnlyDoubleSpinBox : public QDoubleSpinBox
{
  public:
    using QDoubleSpinBox::QDoubleSpinBox;

    void wheelEvent(QWheelEvent* e)
    {
        if (hasFocus())
        {
            QDoubleSpinBox::wheelEvent(e);
        }
        else
        {
            e->ignore();
        }
    }
};

QDoubleSpinBox* MakeDoubleSpinBox()
{
    auto* double_spin_box{ new WheelOnFocusOnlyDoubleSpinBox };
    double_spin_box->setFocusPolicy(Qt::StrongFocus);
    return double_spin_box;
}
