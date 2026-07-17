#pragma once

#include <QLineEdit>

#include <ppp/ui/popups.hpp>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

class NewProjectPopup : public PopupBase
{
    Q_OBJECT

  public:
    NewProjectPopup(QWidget* parent);

    bool CreateNewProject() const;

    QString NewProjectName() const;
    QString NewImageFolder() const;
    QString NewCardSize() const;
    QString NewPaperSize() const;
    bool ClearImages() const;

  private:
    bool m_Cancelled{ false };

    QLineEdit* m_ProjectName{ nullptr };
    QPushButton* m_ImageFolder{ nullptr };
    QString m_ActualImageFolder{ "images" };
    QComboBox* m_CardSize{ nullptr };
    QComboBox* m_PaperSize{ nullptr };
    QCheckBox* m_ClearImages{ nullptr };
};
