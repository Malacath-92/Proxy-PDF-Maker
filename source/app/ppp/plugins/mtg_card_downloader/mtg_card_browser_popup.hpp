#pragma once

#include <ppp/ui/popups/popups.hpp>

#include <ppp/project/project_types.hpp>

class QLineEdit;

class MtGCardBrowserViewModel;
class SelectableCardGrid;

class MtGCardBrowserPopup : public PopupBase
{
    Q_OBJECT

  public:
    MtGCardBrowserPopup(QWidget* parent,
                        MtGCardBrowserViewModel* view_model);

    void Show();

    enum class Choice
    {
        Ok,
        Cancel,
    };
    Choice GetChoice() const;

  private:
    void CloseWithChoice(Choice choice);

    MtGCardBrowserViewModel& m_ViewModel;

    QLineEdit* m_Filter{ nullptr };
    SelectableCardGrid* m_Grid{ nullptr };

    Choice m_Choice{ Choice::Ok };
};
