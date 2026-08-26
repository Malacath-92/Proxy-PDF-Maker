#pragma once

#include <QObject>
#include <QString>

#include <ppp/config_types.hpp>

class Config;

class NewProjectPopupViewModel : public QObject
{
    friend class NewProjectPopup;

  public:
    NewProjectPopupViewModel(const Config& config);

    bool CreateNewProject() const;

    QString NewProjectName() const;
    QString NewImageFolder() const;
    QString NewCardSize() const;
    QString NewPaperSize() const;
    bool ClearImages() const;

  private slots:
    void Cancel();

    void ChangeProjectName(const QString& project_name);
    void ChangeImageFolder(const QString& image_folder);
    void ChangeCardSize(const QString& card_size);
    void ChangePaperSize(const QString& paper_size);
    void ChangeClearImages(Qt::CheckState clear_images);

  private:
    std::string GetDefaultCardSize() const;
    const CardSizes& GetCardSizes() const;

    std::string GetDefaultPageSize() const;
    const PageSizes& GetPageSizes() const;

    const Config& m_Cfg;

    bool m_Cancelled{ false };

    QString m_ProjectName;
    QString m_ImageFolder;
    QString m_CardSize;
    QString m_PaperSize;
    bool m_ClearImages;
};
