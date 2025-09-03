#include <ppp/ui/widget_combo_box.hpp>

#include <QWheelEvent>

class WheelOnFocusOnlyComboBox : public QComboBox
{
  public:
    using QComboBox::QComboBox;

    void wheelEvent(QWheelEvent* e)
    {
        if (hasFocus())
        {
            QComboBox::wheelEvent(e);
        }
        else
        {
            e->ignore();
        }
    }
};

QComboBox* MakeComboBox()
{
    auto* combo_box{ new WheelOnFocusOnlyComboBox };
    combo_box->setFocusPolicy(Qt::StrongFocus);
    return combo_box;
}
