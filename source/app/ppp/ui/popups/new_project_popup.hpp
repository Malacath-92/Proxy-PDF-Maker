#pragma once

#include <ppp/ui/popups/popups.hpp>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

class NewProjectPopupViewModel;

class NewProjectPopup : public PopupBase
{
    Q_OBJECT

  public:
    NewProjectPopup(QWidget* parent,
                    NewProjectPopupViewModel* view_model);

  private:
    NewProjectPopupViewModel& m_ViewModel;

    bool m_Cancelled{ false };

    QLineEdit* m_ProjectName{ nullptr };
    QPushButton* m_ImageFolder{ nullptr };
    QComboBox* m_CardSize{ nullptr };
    QComboBox* m_PaperSize{ nullptr };
    QCheckBox* m_ClearImages{ nullptr };
};
