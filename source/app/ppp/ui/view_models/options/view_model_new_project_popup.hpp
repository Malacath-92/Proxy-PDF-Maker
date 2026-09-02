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
    fs::path NewImageFolder() const;
    QString NewCardSize() const;
    QString NewPaperSize() const;
    bool ClearImages() const;

  private slots:
    void Confirm();

    void ChangeProjectName(const QString& project_name);
    void ChangeImageFolder(const fs::path& image_folder);
    void ChangeCardSize(const QString& card_size);
    void ChangePaperSize(const QString& paper_size);
    void ChangeClearImages(Qt::CheckState clear_images);

  private:
    std::string GetDefaultCardSize() const;
    const CardSizes& GetCardSizes() const;

    std::string GetDefaultPageSize() const;
    const PageSizes& GetPageSizes() const;

    const Config& m_Cfg;

    bool m_Confirmed{ false };

    QString m_ProjectName;
    fs::path m_ImageFolder;
    QString m_CardSize;
    QString m_PaperSize;
    bool m_ClearImages;
};
